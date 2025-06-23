#include "Sphere.h"
#include <cassert>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Sphere::Sphere(ID3D12Device *device, float radius, int sliceCount,
               int stackCount) {
  GenerateSphere(radius, sliceCount, stackCount);
  CreateBuffers(device);
}

Sphere::~Sphere() {
  if (vertexBuffer_)
    vertexBuffer_->Release();
  if (indexBuffer_)
    indexBuffer_->Release();
}

void Sphere::GenerateSphere(float radius, int sliceCount, int stackCount) {
  vertices_.clear();
  indices_.clear();
  for (int stack = 0; stack <= stackCount; ++stack) {
    float phi = float(M_PI) * stack / stackCount;
    for (int slice = 0; slice <= sliceCount; ++slice) {
      float theta = 2.0f * float(M_PI) * slice / sliceCount;
      float x = radius * sinf(phi) * cosf(theta);
      float y = radius * cosf(phi);
      float z = radius * sinf(phi) * sinf(theta);
      float u = float(slice) / sliceCount;
      float v = float(stack) / stackCount;
      vertices_.push_back({Vector4(x, y, z, 1.0f), Vector2(u, v)});
    }
  }
  for (int stack = 0; stack < stackCount; ++stack) {
    for (int slice = 0; slice < sliceCount; ++slice) {
      int first = (stack * (sliceCount + 1)) + slice;
      int second = first + sliceCount + 1;
      indices_.push_back(first);
      indices_.push_back(second);
      indices_.push_back(first + 1);
      indices_.push_back(second);
      indices_.push_back(second + 1);
      indices_.push_back(first + 1);
    }
  }
}

void Sphere::CreateBuffers(ID3D12Device *device) {
  // 頂点バッファ
  size_t vbSize = sizeof(VertexData) * vertices_.size();
  D3D12_HEAP_PROPERTIES heapProp = {};
  heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
  D3D12_RESOURCE_DESC resDesc = {};
  resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  resDesc.Width = vbSize;
  resDesc.Height = 1;
  resDesc.DepthOrArraySize = 1;
  resDesc.MipLevels = 1;
  resDesc.SampleDesc.Count = 1;
  resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  HRESULT hr = device->CreateCommittedResource(
      &heapProp, D3D12_HEAP_FLAG_NONE, &resDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexBuffer_));
  assert(SUCCEEDED(hr));
  void *mapped = nullptr;
  vertexBuffer_->Map(0, nullptr, &mapped);
  memcpy(mapped, vertices_.data(), vbSize);
  vertexBuffer_->Unmap(0, nullptr);

  vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
  vertexBufferView_.SizeInBytes = static_cast<UINT>(vbSize);
  vertexBufferView_.StrideInBytes = sizeof(VertexData);

  // インデックスバッファ
  size_t ibSize = sizeof(uint32_t) * indices_.size();
  resDesc.Width = ibSize;
  hr = device->CreateCommittedResource(
      &heapProp, D3D12_HEAP_FLAG_NONE, &resDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&indexBuffer_));
  assert(SUCCEEDED(hr));
  indexBuffer_->Map(0, nullptr, &mapped);
  memcpy(mapped, indices_.data(), ibSize);
  indexBuffer_->Unmap(0, nullptr);

  indexBufferView_.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
  indexBufferView_.SizeInBytes = static_cast<UINT>(ibSize);
  indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
}
