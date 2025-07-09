#pragma once
#include "../../externals/DirectXTex/DirectXTex.h"
#include <d3d12.h>

// バッファリソース作成
ID3D12Resource *CreateBufferResource(ID3D12Device *device, size_t sizeInBytes);

// テクスチャリソース作成
ID3D12Resource *CreateTextureResource(ID3D12Device *device,
                                      const DirectX::TexMetadata &metadata);

// デプスステンシルリソース作成
ID3D12Resource *CreateDepthStencilTextureResource(ID3D12Device *device,
                                                  int32_t width,
                                                  int32_t height);

// ディスクリプタヒープ作成
ID3D12DescriptorHeap *CreateDescriptorHeap(ID3D12Device *device,
                                           D3D12_DESCRIPTOR_HEAP_TYPE heapType,
                                           UINT numDescriptors,
                                           bool shaderVisible);

// テクスチャデータアップロード
void UploadTextureData(ID3D12Resource *texture,
                       const DirectX::ScratchImage &mipImages);
