#pragma once
#include "../../externals/DirectXTex/DirectXTex.h"
#include "struct.h"     
#include <d3d12.h>
#include <dxcapi.h>


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

// ディスクリプタハンドル取得
D3D12_CPU_DESCRIPTOR_HANDLE
GetCPUDescriptorHandle(ID3D12DescriptorHeap *descriptorHeap,
                       uint32_t descriptorSize, uint32_t index);
D3D12_GPU_DESCRIPTOR_HANDLE
GetGPUDescriptorHandle(ID3D12DescriptorHeap *descriptorHeap,
                       uint32_t descriptorSize, uint32_t index);


IDxcBlob *CompileShader(
    // CompilerするShaderファイルへのパス
    const std::wstring &filePath,
    // CompilerにしようするProfile
    const wchar_t *profile,
    // 初期化で生成したものを3つ
    IDxcUtils *dxcUtils, IDxcCompiler3 *dxcCompiler,
    IDxcIncludeHandler *includeHandler);

DirectX::ScratchImage LoadTexture(const std::string &filePath);

MaterialData LoadMaterialTemplateFile(const std::string &directoryPath,
                                      const std::string &filename);

// objファイルを読み込む関数
ModelData LoadObjFile(const std::string &directoryPath,
                      const std::string &filename);
