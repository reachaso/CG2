#pragma once
#include "DirectXTex/DirectXTex.h"
#include "struct.h"     
#include <d3d12.h>
#include <dxcapi.h>


// バッファリソース作成
ID3D12Resource *CreateBufferResource(ID3D12Device *device, size_t sizeInBytes);

IDxcBlob *CompileShader(
    // CompilerするShaderファイルへのパス
    const std::wstring &filePath,
    // CompilerにしようするProfile
    const wchar_t *profile,
    // 初期化で生成したものを3つ
    IDxcUtils *dxcUtils, IDxcCompiler3 *dxcCompiler,
    IDxcIncludeHandler *includeHandler);
