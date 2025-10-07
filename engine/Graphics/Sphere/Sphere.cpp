#include "Sphere.h"
#include <cassert>
#include <cmath>
#include "Math/Math.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Sphere::~Sphere() {
  if (vb_.resource)
    vb_.resource->Release();
  if (ib_.resource)
    ib_.resource->Release();
  if (cbWvp_.resource)
    cbWvp_.resource->Release();
  if (cbMat_.resource)
    cbMat_.resource->Release();
  if (cbLight_.resource)
    cbLight_.resource->Release();
}

void Sphere::Initialize(ID3D12Device *device, float radius, UINT sliceCount,
                        UINT stackCount) {
  device_ = device;

  // メッシュ生成
  BuildGeometry(radius, sliceCount, stackCount);
  UploadVB_();
  UploadIB_();

  // CB: WVP
  cbWvp_.resource = CreateBufferResource(device_, sizeof(TransformationMatrix));
  cbWvp_.resource->Map(0, nullptr, reinterpret_cast<void **>(&cbWvp_.mapped));
  cbWvp_.mapped->WVP = MakeIdentity4x4();
  cbWvp_.mapped->World = MakeIdentity4x4();

  // CB: Material
  cbMat_.resource = CreateBufferResource(device_, sizeof(Material));
  cbMat_.resource->Map(0, nullptr, reinterpret_cast<void **>(&cbMat_.mapped));
  cbMat_.mapped->color = {1, 1, 1, 1};
  cbMat_.mapped->uvTransform = MakeIdentity4x4();
  cbMat_.mapped->lightingMode = 2; // HalfLambert 既定

  // CB: Light（球ごとに持つ）
  cbLight_.resource = CreateBufferResource(device_, sizeof(DirectionalLight));
  cbLight_.resource->Map(0, nullptr,
                         reinterpret_cast<void **>(&cbLight_.mapped));
  cbLight_.mapped->color = {1, 1, 1, 1};
  cbLight_.mapped->direction = {0.0f, -1.0f, 0.0f};
  cbLight_.mapped->intensity = 1.0f;
}

void Sphere::Update(const Matrix4x4 &view, const Matrix4x4 &proj) {
  Matrix4x4 world = MakeAffineMatrix(transform_.scale, transform_.rotation,
                                     transform_.translation);
  cbWvp_.mapped->World = world;
  cbWvp_.mapped->WVP = Multiply(world, Multiply(view, proj));
}

void Sphere::Draw(ID3D12GraphicsCommandList *cmdList) {
  if (!vb_.resource || !ib_.resource)
    return;

  cmdList->IASetVertexBuffers(0, 1, &vb_.view);
  cmdList->IASetIndexBuffer(&ib_.view);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // RootParam: 0:Material, 1:WVP, 2:SRV, 3:Light
  cmdList->SetGraphicsRootConstantBufferView(
      0, cbMat_.resource->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootConstantBufferView(
      1, cbWvp_.resource->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootDescriptorTable(2, textureSrv_);
  cmdList->SetGraphicsRootConstantBufferView(
      3, cbLight_.resource->GetGPUVirtualAddress());

  cmdList->DrawIndexedInstanced(ib_.indexCount, 1, 0, 0, 0);
}

void Sphere::BuildGeometry(float radius, UINT sliceCount, UINT stackCount) {
  vertices_.clear();
  indices_.clear();

  // 上極点
  vertices_.push_back({Vector4(0, +radius, 0, 1), Vector2(0.0f, 0.0f)});

  // 中間リング（緯度）
  for (UINT lat = 1; lat < stackCount; ++lat) {
    float phi = lat * (float(M_PI) / float(stackCount));
    float v = float(lat) / float(stackCount);

    for (UINT lon = 0; lon <= sliceCount; ++lon) {
      float theta = lon * (2.0f * float(M_PI) / float(sliceCount));
      float u = float(lon) / float(sliceCount);

      float x = radius * sinf(phi) * cosf(theta);
      float y = radius * cosf(phi);
      float z = radius * sinf(phi) * sinf(theta);

      vertices_.push_back({Vector4(x, y, z, 1), Vector2(u, v)});
    }
  }

  // 下極点
  vertices_.push_back({Vector4(0, -radius, 0, 1), Vector2(0.0f, 1.0f)});

  // 法線（位置の正規化）
  for (auto &v : vertices_) {
    float len =
        std::sqrt(v.position.x * v.position.x + v.position.y * v.position.y +
                  v.position.z * v.position.z);
    if (len > 0.0f) {
      v.normal.x = v.position.x / len;
      v.normal.y = v.position.y / len;
      v.normal.z = v.position.z / len;
    } else {
      v.normal = {0.0f, 1.0f, 0.0f};
    }
  }

  // インデックス
  UINT ringVerts = sliceCount + 1;

  // 上極ファン（i と i+1 を入れ替え）
  for (UINT i = 1; i <= sliceCount; ++i) {
    indices_.push_back(0);
    indices_.push_back(i + 1);
    indices_.push_back(i);
  }

  // 中間帯のクアッド
  for (UINT i = 0; i < stackCount - 2; ++i) {
    for (UINT j = 0; j < sliceCount; ++j) {
      UINT a = 1 + i * ringVerts + j;
      UINT b = 1 + i * ringVerts + j + 1;
      UINT c = 1 + (i + 1) * ringVerts + j;
      UINT d = 1 + (i + 1) * ringVerts + j + 1;
      indices_.push_back(a);
      indices_.push_back(b);
      indices_.push_back(d);
      indices_.push_back(a);
      indices_.push_back(d);
      indices_.push_back(c);
    }
  }

  // 下極ファン
  UINT southPoleIndex = UINT(vertices_.size() - 1);
  UINT baseIndex = southPoleIndex - ringVerts;
  for (UINT i = 0; i < sliceCount; ++i) {
    indices_.push_back(southPoleIndex);
    indices_.push_back(baseIndex + i);
    indices_.push_back(baseIndex + i + 1);
  }
}

void Sphere::UploadVB_() {
  vb_.vertexCount = static_cast<uint32_t>(vertices_.size());
  if (vb_.vertexCount == 0)
    return;

  const UINT sizeBytes = UINT(sizeof(VertexData) * vb_.vertexCount);
  vb_.resource = CreateBufferResource(device_, sizeBytes);

  void *mapped = nullptr;
  vb_.resource->Map(0, nullptr, &mapped);
  std::memcpy(mapped, vertices_.data(), sizeBytes);
  vb_.resource->Unmap(0, nullptr);

  vb_.view.BufferLocation = vb_.resource->GetGPUVirtualAddress();
  vb_.view.SizeInBytes = sizeBytes;
  vb_.view.StrideInBytes = sizeof(VertexData);
}

void Sphere::UploadIB_() {
  ib_.indexCount = static_cast<uint32_t>(indices_.size());
  if (ib_.indexCount == 0)
    return;

  const UINT sizeBytes = UINT(sizeof(uint16_t) * ib_.indexCount);
  // アップロードヒープでOK（頻繁に書き換えない・簡易）
  D3D12_HEAP_PROPERTIES heapProps{};
  heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

  D3D12_RESOURCE_DESC resDesc{};
  resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  resDesc.Width = sizeBytes;
  resDesc.Height = 1;
  resDesc.DepthOrArraySize = 1;
  resDesc.MipLevels = 1;
  resDesc.Format = DXGI_FORMAT_UNKNOWN;
  resDesc.SampleDesc.Count = 1;
  resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  HRESULT hr = device_->CreateCommittedResource(
      &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&ib_.resource));
  assert(SUCCEEDED(hr));

  uint16_t *mapped = nullptr;
  ib_.resource->Map(0, nullptr, reinterpret_cast<void **>(&mapped));
  std::memcpy(mapped, indices_.data(), sizeBytes);
  ib_.resource->Unmap(0, nullptr);

  ib_.view.BufferLocation = ib_.resource->GetGPUVirtualAddress();
  ib_.view.Format = DXGI_FORMAT_R16_UINT;
  ib_.view.SizeInBytes = sizeBytes;
}
