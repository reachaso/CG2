#include "ModelResource.h"
#include "Render/FrameResource.h"
#include "Texture/TextureManager/TextureManager.h"
#include <algorithm>
#include <cassert>
#include <cstring>

using namespace RC;

ModelResource::~ModelResource() {
  cbMat_.resource.Reset();
  cbLight_.resource.Reset();
}

void ModelResource::Initialize(ID3D12Device *device) {
  device_ = device;

  // Material CB
  cbMat_.resource = CreateBufferResource(device_.Get(), sizeof(Material),
                                         L"ModelResource::cbMat_");
  cbMat_.resource->Map(0, nullptr,
                       reinterpret_cast<void **>(&cbMat_.mapped));
  cbMat_.mapped->color = {1, 1, 1, 1};
  cbMat_.mapped->lightingMode = 2; // 既定 HalfLambert
  cbMat_.mapped->uvTransform = MakeIdentity4x4();

  // padding 初期化（ガラスでは environmentCoefficient=IOR, padding=roughness として使う）
  cbMat_.mapped->environmentCoefficient = 0.0f; // 通常モデル: 映り込みなし / Glass: IOR（0ならPS側で1.5扱い）
  cbMat_.mapped->padding = 0.0f;                // Glass: roughness

  // Light CB（各Objectが自前で持つ）
  cbLight_.resource = CreateBufferResource(device_.Get(),
                                           sizeof(DirectionalLight),
                                           L"ModelResource::cbLight_");
  cbLight_.resource->Map(0, nullptr,
                         reinterpret_cast<void **>(&cbLight_.mapped));
  cbLight_.mapped->color = {1, 1, 1, 1};
  cbLight_.mapped->direction = {0.0f, -1.0f, 0.0f};
  cbLight_.mapped->intensity = 1.0f;
  cbMat_.mapped->shininess = 32.0f;
}

void ModelResource::SetMesh(const std::shared_ptr<ModelMesh> &mesh) {
  mesh_ = mesh;
  // meshが変わったらMaterial SRVキャッシュは破棄
  materialSrvs_.clear();
  // overrideも一旦クリア
  textureSrv_ = {};
}

void ModelResource::ResetTextureToMtl() {
  textureSrv_ = {};
  materialSrvs_.clear();
  EnsureMaterialSrvsLoaded_();
}

void ModelResource::ApplyLighting(int lightingMode, const float color[3],
                                  const float dir[3], float intensity) {
  if (cbMat_.mapped) {
    cbMat_.mapped->lightingMode = lightingMode;
  }
  if (cbLight_.mapped) {
    cbLight_.mapped->color = {color[0], color[1], color[2], 1.0f};
    cbLight_.mapped->direction = {dir[0], dir[1], dir[2]};
    cbLight_.mapped->intensity = intensity;
  }
}

// ============================================================================
// 描画ヘルパー
// ============================================================================

void ModelResource::EnsureMaterialSrvsLoaded_() {
  if (!texman_ || !mesh_) {
    return;
  }

  const auto &mats = mesh_->Materials();
  if (mats.empty()) {
    // 互換：昔の1枚だけ
    materialSrvs_.clear();
    const auto &mtl = mesh_->MaterialFile();
    if (!mtl.textureFilePath.empty()) {
      materialSrvs_.resize(1);
      materialSrvs_[0] = texman_->Load(mtl.textureFilePath, /*srgb=*/true);
    }
    return;
  }

  if (!materialSrvs_.empty() && materialSrvs_.size() == mats.size()) {
    return; // 既にロード済み
  }

  materialSrvs_.assign(mats.size(), D3D12_GPU_DESCRIPTOR_HANDLE{});
  for (uint32_t i = 0; i < mats.size(); ++i) {
    if (mats[i].textureFilePath.empty())
      continue;

    const D3D12_GPU_DESCRIPTOR_HANDLE srv =
        texman_->Load(mats[i].textureFilePath, /*srgb=*/true);
    materialSrvs_[i] = (srv.ptr == 0) ? D3D12_GPU_DESCRIPTOR_HANDLE{} : srv;
  }
}

D3D12_GPU_DESCRIPTOR_HANDLE
ModelResource::GetSrvForMaterial_(uint32_t materialIndex) const {
  if (!materialSrvs_.empty() && materialIndex < materialSrvs_.size() &&
      materialSrvs_[materialIndex].ptr != 0) {
    return materialSrvs_[materialIndex];
  }

  // fallback：先頭の有効SRV
  for (const auto &h : materialSrvs_) {
    if (h.ptr != 0)
      return h;
  }

  return D3D12_GPU_DESCRIPTOR_HANDLE{};
}

// ============================================================================
// Draw（単発描画）
// ============================================================================

void ModelResource::Draw(ID3D12GraphicsCommandList *cmdList,
                         const Matrix4x4 &world, const Matrix4x4 &view,
                         const Matrix4x4 &proj, FrameResource &frame) {
  if (!isReady_ || !mesh_ || !mesh_->Ready())
    return;

  // Node階層を使う場合は DrawItem を使う
  const auto &items = mesh_->DrawItems();

  // テクスチャ（overrideが無い場合だけ materialIndex対応を準備）
  if (textureSrv_.ptr == 0) {
    EnsureMaterialSrvsLoaded_();
  }

  const auto &vbv = mesh_->VBV();
  cmdList->IASetVertexBuffers(0, 1, &vbv);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  cmdList->SetGraphicsRootConstantBufferView(
      0, cbMat_.resource->GetGPUVirtualAddress());

  // Light CB（b1）: 外部ライトが指定されていればそちらを使う
  const D3D12_GPU_VIRTUAL_ADDRESS lightAddr =
      (externalLightCBAddress_ != 0)
          ? externalLightCBAddress_
          : cbLight_.resource->GetGPUVirtualAddress();
  cmdList->SetGraphicsRootConstantBufferView(3, lightAddr);

  if (items.empty()) {
    // 互換：DrawItemが無い場合は全頂点を1発
    void *dst = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS addr =
        frame.AllocCB(sizeof(TransformationMatrix), &dst);

    auto *tm = reinterpret_cast<TransformationMatrix *>(dst);
    tm->World = world;
    tm->WVP = Multiply(world, Multiply(view, proj));
    tm->worldInverseTranspose = Transpose(Inverse(world));

    cmdList->SetGraphicsRootConstantBufferView(1, addr);

    // texture
    cmdList->SetGraphicsRootDescriptorTable(
        2, (textureSrv_.ptr != 0) ? textureSrv_ : GetSrvForMaterial_(0));

    if (mesh_->HasIndexBuffer()) {
      cmdList->IASetIndexBuffer(&mesh_->IBV());
      cmdList->DrawIndexedInstanced(mesh_->IndexCount(), 1, 0, 0, 0);
    } else {
      cmdList->DrawInstanced(mesh_->VertexCount(), 1, 0, 0);
    }
    return;
  }

  // IndexBufferがある場合はIASetを事前に行う
  if (mesh_->HasIndexBuffer()) {
    cmdList->IASetIndexBuffer(&mesh_->IBV());
  }

  for (uint32_t i = 0; i < items.size(); ++i) {
    const auto &it = items[i];

    // Node行列（モデル空間）→ world を掛ける
    const Matrix4x4 nodeWorld = Multiply(it.nodeWorld, world);

    // FrameResource から CB 領域を確保
    void *dst = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS addr =
        frame.AllocCB(sizeof(TransformationMatrix), &dst);

    auto *tm = reinterpret_cast<TransformationMatrix *>(dst);
    tm->World = nodeWorld;
    tm->WVP = Multiply(nodeWorld, Multiply(view, proj));
    tm->worldInverseTranspose = Transpose(Inverse(nodeWorld));

    cmdList->SetGraphicsRootConstantBufferView(1, addr);

    // テクスチャ（override優先）
    const D3D12_GPU_DESCRIPTOR_HANDLE srv =
        (textureSrv_.ptr != 0) ? textureSrv_
                               : GetSrvForMaterial_(it.materialIndex);
    cmdList->SetGraphicsRootDescriptorTable(2, srv);

    // 描画
    if (mesh_->HasIndexBuffer() && it.indexCount > 0) {
      cmdList->DrawIndexedInstanced(it.indexCount, 1, it.indexStart, it.vertexStart, 0);
    } else {
      cmdList->DrawInstanced(it.vertexCount, 1, it.vertexStart, 0);
    }
  }
}

// ============================================================================
// DrawBatch（インスタンシング・白色版）
// ============================================================================

void ModelResource::DrawBatch(ID3D12GraphicsCommandList *cmdList,
                              const Matrix4x4 &view, const Matrix4x4 &proj,
                              const std::vector<Transform> &instances,
                              FrameResource &frame) {
  if (!isReady_ || !mesh_ || !mesh_->Ready() || instances.empty()) {
    return;
  }

  const auto &items = mesh_->DrawItems();

  const auto &vbv = mesh_->VBV();
  cmdList->IASetVertexBuffers(0, 1, &vbv);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  cmdList->SetGraphicsRootConstantBufferView(
      0, cbMat_.resource->GetGPUVirtualAddress());

  if (textureSrv_.ptr == 0) {
    EnsureMaterialSrvsLoaded_();
  }

  const D3D12_GPU_VIRTUAL_ADDRESS lightAddr =
      (externalLightCBAddress_ != 0)
          ? externalLightCBAddress_
          : cbLight_.resource->GetGPUVirtualAddress();
  cmdList->SetGraphicsRootConstantBufferView(3, lightAddr);

  // FrameResource から SRV 領域を一括確保
  const uint32_t count = static_cast<uint32_t>(instances.size());
  const uint32_t dataSize = count * static_cast<uint32_t>(sizeof(InstanceDataGPU));

  void *mapped = nullptr;
  D3D12_GPU_VIRTUAL_ADDRESS instAddr = frame.AllocSRV(dataSize, &mapped);
  auto *dst = reinterpret_cast<InstanceDataGPU *>(mapped);

  for (uint32_t i = 0; i < count; ++i) {
    const Transform &tr = instances[i];
    Matrix4x4 world = MakeAffineMatrix(tr.scale, tr.rotation, tr.translation);
    dst[i].World = world;
    dst[i].WVP = Multiply(world, Multiply(view, proj));
    dst[i].WorldInverseTranspose = Transpose(Inverse(world));
    dst[i].color = {1, 1, 1, 1};
  }

  cmdList->SetGraphicsRootShaderResourceView(1, instAddr);

  if (items.empty()) {
    cmdList->SetGraphicsRootDescriptorTable(
        2, (textureSrv_.ptr != 0) ? textureSrv_ : GetSrvForMaterial_(0));
    if (mesh_->HasIndexBuffer()) {
      cmdList->IASetIndexBuffer(&mesh_->IBV());
      cmdList->DrawIndexedInstanced(mesh_->IndexCount(), count, 0, 0, 0);
    } else {
      cmdList->DrawInstanced(mesh_->VertexCount(), count, 0, 0);
    }
  } else {
    if (mesh_->HasIndexBuffer()) {
      cmdList->IASetIndexBuffer(&mesh_->IBV());
    }
    for (const auto &it : items) {
      const D3D12_GPU_DESCRIPTOR_HANDLE srv =
          (textureSrv_.ptr != 0) ? textureSrv_
                                 : GetSrvForMaterial_(it.materialIndex);
      cmdList->SetGraphicsRootDescriptorTable(2, srv);
      if (mesh_->HasIndexBuffer() && it.indexCount > 0) {
        cmdList->DrawIndexedInstanced(it.indexCount, count, it.indexStart, it.vertexStart, 0);
      } else {
        cmdList->DrawInstanced(it.vertexCount, count, it.vertexStart, 0);
      }
    }
  }
}

// ============================================================================
// DrawBatch（インスタンシング・カラー指定版）
// ============================================================================

void ModelResource::DrawBatch(ID3D12GraphicsCommandList *cmdList,
                              const Matrix4x4 &view, const Matrix4x4 &proj,
                              const std::vector<Transform> &instances,
                              const RC::Vector4 &color,
                              FrameResource &frame) {
  if (!isReady_ || !mesh_ || !mesh_->Ready() || instances.empty()) {
    return;
  }

  const auto &items = mesh_->DrawItems();

  const auto &vbv = mesh_->VBV();
  cmdList->IASetVertexBuffers(0, 1, &vbv);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  cmdList->SetGraphicsRootConstantBufferView(
      0, cbMat_.resource->GetGPUVirtualAddress());

  if (textureSrv_.ptr == 0) {
    EnsureMaterialSrvsLoaded_();
  }

  const D3D12_GPU_VIRTUAL_ADDRESS lightAddr =
      (externalLightCBAddress_ != 0)
          ? externalLightCBAddress_
          : cbLight_.resource->GetGPUVirtualAddress();
  cmdList->SetGraphicsRootConstantBufferView(3, lightAddr);

  // FrameResource から SRV 領域を一括確保
  const uint32_t count = static_cast<uint32_t>(instances.size());
  const uint32_t dataSize = count * static_cast<uint32_t>(sizeof(InstanceDataGPU));

  void *mapped = nullptr;
  D3D12_GPU_VIRTUAL_ADDRESS instAddr = frame.AllocSRV(dataSize, &mapped);
  auto *dst = reinterpret_cast<InstanceDataGPU *>(mapped);

  for (uint32_t i = 0; i < count; ++i) {
    const Transform &tr = instances[i];
    Matrix4x4 world = MakeAffineMatrix(tr.scale, tr.rotation, tr.translation);
    dst[i].World = world;
    dst[i].WVP = Multiply(world, Multiply(view, proj));
    dst[i].WorldInverseTranspose = Transpose(Inverse(world));
    dst[i].color = color;
  }

  cmdList->SetGraphicsRootShaderResourceView(1, instAddr);

  if (items.empty()) {
    cmdList->SetGraphicsRootDescriptorTable(
        2, (textureSrv_.ptr != 0) ? textureSrv_ : GetSrvForMaterial_(0));
    if (mesh_->HasIndexBuffer()) {
      cmdList->IASetIndexBuffer(&mesh_->IBV());
      cmdList->DrawIndexedInstanced(mesh_->IndexCount(), count, 0, 0, 0);
    } else {
      cmdList->DrawInstanced(mesh_->VertexCount(), count, 0, 0);
    }
  } else {
    if (mesh_->HasIndexBuffer()) {
      cmdList->IASetIndexBuffer(&mesh_->IBV());
    }
    for (const auto &it : items) {
      const D3D12_GPU_DESCRIPTOR_HANDLE srv =
          (textureSrv_.ptr != 0) ? textureSrv_
                                 : GetSrvForMaterial_(it.materialIndex);
      cmdList->SetGraphicsRootDescriptorTable(2, srv);
      if (mesh_->HasIndexBuffer() && it.indexCount > 0) {
        cmdList->DrawIndexedInstanced(it.indexCount, count, it.indexStart, it.vertexStart, 0);
      } else {
        cmdList->DrawInstanced(it.vertexCount, count, it.vertexStart, 0);
      }
    }
  }
}

// ============================================================================
// DrawSkinned（スキニング描画）
// ============================================================================

void ModelResource::DrawSkinned(ID3D12GraphicsCommandList *cmdList,
                                const Matrix4x4 &world, const Matrix4x4 &view,
                                const Matrix4x4 &proj,
                                const std::vector<Matrix4x4> &skinMatrices,
                                FrameResource &frame) {
  if (!isReady_ || !mesh_ || !mesh_->Ready() || skinMatrices.empty())
    return;

  const auto &items = mesh_->DrawItems();

  // テクスチャ
  if (textureSrv_.ptr == 0) {
    EnsureMaterialSrvsLoaded_();
  }

  const auto &vbv = mesh_->VBV();
  cmdList->IASetVertexBuffers(0, 1, &vbv);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  if (mesh_->HasIndexBuffer()) {
    cmdList->IASetIndexBuffer(&mesh_->IBV());
  }

  // Material CB (slot 0, PS)
  cmdList->SetGraphicsRootConstantBufferView(
      0, cbMat_.resource->GetGPUVirtualAddress());

  // Light CB (slot 3, PS)
  const D3D12_GPU_VIRTUAL_ADDRESS lightAddr =
      (externalLightCBAddress_ != 0)
          ? externalLightCBAddress_
          : cbLight_.resource->GetGPUVirtualAddress();
  cmdList->SetGraphicsRootConstantBufferView(3, lightAddr);

  // 行列パレットをSRVとして転送 (slot 9, VS)
  const uint32_t matCount = static_cast<uint32_t>(skinMatrices.size());
  const uint32_t matSize = matCount * static_cast<uint32_t>(sizeof(Matrix4x4));
  void *matMapped = nullptr;
  D3D12_GPU_VIRTUAL_ADDRESS matAddr = frame.AllocSRV(matSize, &matMapped);
  std::memcpy(matMapped, skinMatrices.data(), matSize);
  cmdList->SetGraphicsRootShaderResourceView(9, matAddr);

  // スキニングモデルではnodeWorldは掛けない
  // （スキニング計算で既にスケルトン空間→ワールドはworldだけで十分）

  if (items.empty()) {
    // DrawItem無しの場合
    void *dst = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS addr =
        frame.AllocCB(sizeof(TransformationMatrix), &dst);
    auto *tm = reinterpret_cast<TransformationMatrix *>(dst);
    tm->World = world;
    tm->WVP = Multiply(world, Multiply(view, proj));
    tm->worldInverseTranspose = Transpose(Inverse(world));
    cmdList->SetGraphicsRootConstantBufferView(1, addr);

    cmdList->SetGraphicsRootDescriptorTable(
        2, (textureSrv_.ptr != 0) ? textureSrv_ : GetSrvForMaterial_(0));

    if (mesh_->HasIndexBuffer()) {
      cmdList->DrawIndexedInstanced(mesh_->IndexCount(), 1, 0, 0, 0);
    } else {
      cmdList->DrawInstanced(mesh_->VertexCount(), 1, 0, 0);
    }
    return;
  }

  for (uint32_t i = 0; i < items.size(); ++i) {
    const auto &it = items[i];

    // スキニングモデルではnodeWorldは使わない（スキニング行列で処理済み）
    void *dst = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS addr =
        frame.AllocCB(sizeof(TransformationMatrix), &dst);
    auto *tm = reinterpret_cast<TransformationMatrix *>(dst);
    tm->World = world;
    tm->WVP = Multiply(world, Multiply(view, proj));
    tm->worldInverseTranspose = Transpose(Inverse(world));
    cmdList->SetGraphicsRootConstantBufferView(1, addr);

    const D3D12_GPU_DESCRIPTOR_HANDLE srv =
        (textureSrv_.ptr != 0) ? textureSrv_
                               : GetSrvForMaterial_(it.materialIndex);
    cmdList->SetGraphicsRootDescriptorTable(2, srv);

    if (mesh_->HasIndexBuffer() && it.indexCount > 0) {
      cmdList->DrawIndexedInstanced(it.indexCount, 1, it.indexStart, it.vertexStart, 0);
    } else {
      cmdList->DrawInstanced(it.vertexCount, 1, it.vertexStart, 0);
    }
  }
}
