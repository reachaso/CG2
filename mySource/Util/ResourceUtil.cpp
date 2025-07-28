#include "ResourceUtil.h"
#include <cassert>
#include <fstream>

ID3D12Resource *CreateBufferResource(ID3D12Device *device, size_t sizeInBytes) {
  D3D12_HEAP_PROPERTIES heapProperties{};
  heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

  D3D12_RESOURCE_DESC bufferDesc{};
  bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  bufferDesc.Width = sizeInBytes;
  bufferDesc.Height = 1;
  bufferDesc.DepthOrArraySize = 1;
  bufferDesc.MipLevels = 1;
  bufferDesc.SampleDesc.Count = 1;
  bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  ID3D12Resource *buffer = nullptr;
  HRESULT hr = device->CreateCommittedResource(
      &heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer));
  assert(SUCCEEDED(hr));
  return buffer;
}

ID3D12Resource *CreateTextureResource(ID3D12Device *device,
                                      const DirectX::TexMetadata &metadata) {
  D3D12_RESOURCE_DESC resourceDesc{};
  resourceDesc.Width = UINT(metadata.width);
  resourceDesc.Height = UINT(metadata.height);
  resourceDesc.MipLevels = UINT16(metadata.mipLevels);
  resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);
  resourceDesc.Format = metadata.format;
  resourceDesc.SampleDesc.Count = 1;
  resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

  D3D12_HEAP_PROPERTIES heapProperties{};
  heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM;
  heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
  heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

  ID3D12Resource *resource = nullptr;
  HRESULT hr = device->CreateCommittedResource(
      &heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));
  assert(SUCCEEDED(hr));
  return resource;
}

ID3D12Resource *CreateDepthStencilTextureResource(ID3D12Device *device,
                                                  int32_t width,
                                                  int32_t height) {
  D3D12_RESOURCE_DESC resourceDesc{};
  resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  resourceDesc.Width = width;
  resourceDesc.Height = height;
  resourceDesc.DepthOrArraySize = 1;
  resourceDesc.MipLevels = 1;
  resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  resourceDesc.SampleDesc.Count = 1;
  resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

  D3D12_HEAP_PROPERTIES heapProperties{};
  heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

  D3D12_CLEAR_VALUE depthClearValue{};
  depthClearValue.DepthStencil.Depth = 1.0f;
  depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

  ID3D12Resource *resource = nullptr;
  HRESULT hr = device->CreateCommittedResource(
      &heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
      D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClearValue,
      IID_PPV_ARGS(&resource));
  assert(SUCCEEDED(hr));
  return resource;
}

ID3D12DescriptorHeap *CreateDescriptorHeap(ID3D12Device *device,
                                           D3D12_DESCRIPTOR_HEAP_TYPE heapType,
                                           UINT numDescriptors,
                                           bool shaderVisible) {
  ID3D12DescriptorHeap *descriptorHeap = nullptr;
  D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
  descriptorHeapDesc.Type = heapType;
  descriptorHeapDesc.NumDescriptors = numDescriptors;
  descriptorHeapDesc.Flags = shaderVisible
                                 ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                                 : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc,
                                            IID_PPV_ARGS(&descriptorHeap));
  assert(SUCCEEDED(hr));
  return descriptorHeap;
}

void UploadTextureData(ID3D12Resource *texture,
                       const DirectX::ScratchImage &mipImages) {
  const DirectX::TexMetadata &metadata = mipImages.GetMetadata();
  for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel) {
    const DirectX::Image *img = mipImages.GetImage(mipLevel, 0, 0);
    HRESULT hr =
        texture->WriteToSubresource(UINT(mipLevel), nullptr, img->pixels,
                                    UINT(img->rowPitch), UINT(img->slicePitch));
    assert(SUCCEEDED(hr));
  }
}

D3D12_CPU_DESCRIPTOR_HANDLE
GetCPUDescriptorHandle(ID3D12DescriptorHeap *descriptorHeap,
                       uint32_t descriptorSize, uint32_t index) {
  D3D12_CPU_DESCRIPTOR_HANDLE handleCPU =
      descriptorHeap->GetCPUDescriptorHandleForHeapStart();
  handleCPU.ptr += (descriptorSize * index);
  return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE
GetGPUDescriptorHandle(ID3D12DescriptorHeap *descriptorHeap,
                       uint32_t descriptorSize, uint32_t index) {
  D3D12_GPU_DESCRIPTOR_HANDLE handleGPU =
      descriptorHeap->GetGPUDescriptorHandleForHeapStart();
  handleGPU.ptr += (descriptorSize * index);
  return handleGPU;
}

