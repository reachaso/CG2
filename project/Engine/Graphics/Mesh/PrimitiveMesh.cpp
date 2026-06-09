#include "PrimitiveMesh.h"
#include "RenderContext.h"
#include "Math/Math.h"
#include "imgui/imgui.h"
#include <cassert>
#include <cstring>
#include <string>

using namespace RC;

PrimitiveMesh::~PrimitiveMesh() {
  vb_.resource.Reset();
  ib_.resource.Reset();
  cbWvp_.resource.Reset();
  cbMat_.resource.Reset();
}

void PrimitiveMesh::Initialize(ID3D12Device *device, const ModelData &data) {
  device_ = device;

  UploadVB_(data.vertices);
  UploadIB_(data.indices);

  // CB: WVP
  cbWvp_.resource = CreateBufferResource(device_.Get(), sizeof(TransformationMatrix), L"PrimitiveMesh::cbWvp_");
  cbWvp_.resource->Map(0, nullptr, reinterpret_cast<void **>(&cbWvp_.mapped));
  
  // CB: Material
  cbMat_.resource = CreateBufferResource(device_.Get(), sizeof(Material), L"PrimitiveMesh::cbMat_");
  cbMat_.resource->Map(0, nullptr, reinterpret_cast<void **>(&cbMat_.mapped));
  cbMat_.mapped->color = {1, 1, 1, 1};
  cbMat_.mapped->uvTransform = MakeIdentity4x4();
  cbMat_.mapped->lightingMode = 2; // Half Lambert 既定
  cbMat_.mapped->shininess = 32.0f;
}

void PrimitiveMesh::Draw(ID3D12GraphicsCommandList *cmdList) {
  if (!vb_.resource || !visible_)
    return;

  cmdList->IASetVertexBuffers(0, 1, &vb_.view);
  if (ib_.resource) {
    cmdList->IASetIndexBuffer(&ib_.view);
  }
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  cbMat_.mapped->useNormalMap = (normalMapSrv_.ptr != 0) ? 1 : 0;
  cbMat_.mapped->useRoughnessMap = (roughnessMapSrv_.ptr != 0) ? 1 : 0;

  // RootParam: 0:Material, 1:WVP, 2:SRV, 3:Light
  cmdList->SetGraphicsRootConstantBufferView(0, cbMat_.resource->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootConstantBufferView(1, cbWvp_.resource->GetGPUVirtualAddress());
  const D3D12_GPU_DESCRIPTOR_HANDLE mainSrv = textureSrv_.ptr != 0 ? textureSrv_ : D3D12_GPU_DESCRIPTOR_HANDLE{};
  if (mainSrv.ptr != 0) {
    cmdList->SetGraphicsRootDescriptorTable(2, mainSrv);
  }
  // Slot 9 is NormalMap
  if (normalMapSrv_.ptr != 0) {
    cmdList->SetGraphicsRootDescriptorTable(9, normalMapSrv_);
  } else if (mainSrv.ptr != 0) {
    cmdList->SetGraphicsRootDescriptorTable(9, mainSrv);
  }
  // Slot 10 is RoughnessMap
  if (roughnessMapSrv_.ptr != 0) {
    cmdList->SetGraphicsRootDescriptorTable(10, roughnessMapSrv_);
  } else if (mainSrv.ptr != 0) {
    cmdList->SetGraphicsRootDescriptorTable(10, mainSrv);
  }

  // PrimitiveMesh ではライト管理がまだ簡易。
  // 必要に応じて RenderContext から 共通ライトを取得してバインド
}

void PrimitiveMesh::Draw(ID3D12GraphicsCommandList *cmdList, const RC::Matrix4x4 &world) {
  if (!vb_.resource || !visible_ || !cbWvp_.mapped)
    return;

  auto &ctx = GetRenderContext();
  cbWvp_.mapped->World = world;
  Matrix4x4 vp = Multiply(ctx.View(), ctx.Proj());
  cbWvp_.mapped->WVP = Multiply(world, vp);
  cbWvp_.mapped->worldInverseTranspose = Transpose(Inverse(world));

  // IA
  cmdList->IASetVertexBuffers(0, 1, &vb_.view);
  if (ib_.resource) {
    cmdList->IASetIndexBuffer(&ib_.view);
  }
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  cbMat_.mapped->useNormalMap = (normalMapSrv_.ptr != 0) ? 1 : 0;
  cbMat_.mapped->useRoughnessMap = (roughnessMapSrv_.ptr != 0) ? 1 : 0;

  // RootParam: 0:Material, 1:WVP, 2:SRV, 3:Light
  cmdList->SetGraphicsRootConstantBufferView(0, cbMat_.resource->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootConstantBufferView(1, cbWvp_.resource->GetGPUVirtualAddress());
  const D3D12_GPU_DESCRIPTOR_HANDLE mainSrv2 = textureSrv_.ptr != 0 ? textureSrv_ : D3D12_GPU_DESCRIPTOR_HANDLE{};
  if (mainSrv2.ptr != 0) {
    cmdList->SetGraphicsRootDescriptorTable(2, mainSrv2);
  }
  if (normalMapSrv_.ptr != 0) {
    cmdList->SetGraphicsRootDescriptorTable(9, normalMapSrv_);
  } else if (mainSrv2.ptr != 0) {
    cmdList->SetGraphicsRootDescriptorTable(9, mainSrv2);
  }
  if (roughnessMapSrv_.ptr != 0) {
    cmdList->SetGraphicsRootDescriptorTable(10, roughnessMapSrv_);
  } else if (mainSrv2.ptr != 0) {
    cmdList->SetGraphicsRootDescriptorTable(10, mainSrv2);
  }

  if (ib_.resource) {
    cmdList->DrawIndexedInstanced(ib_.indexCount, 1, 0, 0, 0);
  } else {
    cmdList->DrawInstanced(vb_.vertexCount, 1, 0, 0);
  }
}

void PrimitiveMesh::DrawImGui(const char *name) {
#if RC_ENABLE_IMGUI
  std::string label = name ? name : "PrimitiveMesh";
  if (!ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
    return;

  ImGui::PushID(label.c_str());

  if (ImGui::TreeNode("Transform")) {
    ImGui::DragFloat3("Position", &transform_.translation.x, 0.1f);
    ImGui::SliderAngle("Rotate X", &transform_.rotation.x);
    ImGui::SliderAngle("Rotate Y", &transform_.rotation.y);
    ImGui::SliderAngle("Rotate Z", &transform_.rotation.z);
    ImGui::DragFloat3("Scale", &transform_.scale.x, 0.05f);
    ImGui::TreePop();
  }

  if (cbMat_.mapped) {
    if (ImGui::TreeNode("Material")) {
      ImGui::ColorEdit4("Color", &cbMat_.mapped->color.x);
      ImGui::DragFloat("Shininess", &cbMat_.mapped->shininess, 0.5f, 0.0f, 512.0f);

      const char *items[] = {"None", "Lambert", "Half-Lambert"};
      ImGui::Combo("Lighting Mode", &cbMat_.mapped->lightingMode, items, 3);
      ImGui::TreePop();
    }
  }

  ImGui::PopID();
#endif
}

void PrimitiveMesh::UploadVB_(const std::vector<VertexData> &vertices) {
  vb_.vertexCount = static_cast<uint32_t>(vertices.size());
  if (vb_.vertexCount == 0)
    return;

  const UINT sizeBytes = UINT(sizeof(VertexData) * vb_.vertexCount);
  vb_.resource = CreateBufferResource(device_.Get(), sizeBytes, L"PrimitiveMesh::vb_");

  void *mapped = nullptr;
  vb_.resource->Map(0, nullptr, &mapped);
  std::memcpy(mapped, vertices.data(), sizeBytes);
  vb_.resource->Unmap(0, nullptr);

  vb_.view.BufferLocation = vb_.resource->GetGPUVirtualAddress();
  vb_.view.SizeInBytes = sizeBytes;
  vb_.view.StrideInBytes = sizeof(VertexData);
}

void PrimitiveMesh::UploadIB_(const std::vector<uint32_t> &indices) {
  ib_.indexCount = static_cast<uint32_t>(indices.size());
  if (ib_.indexCount == 0)
    return;

  const UINT sizeBytes = UINT(sizeof(uint32_t) * ib_.indexCount);
  ib_.resource = CreateBufferResource(device_.Get(), sizeBytes, L"PrimitiveMesh::ib_");

  uint32_t *mapped = nullptr;
  ib_.resource->Map(0, nullptr, reinterpret_cast<void **>(&mapped));
  std::memcpy(mapped, indices.data(), sizeBytes);
  ib_.resource->Unmap(0, nullptr);

  ib_.view.BufferLocation = ib_.resource->GetGPUVirtualAddress();
  ib_.view.Format = DXGI_FORMAT_R32_UINT;
  ib_.view.SizeInBytes = sizeBytes;
}
