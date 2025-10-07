#pragma once
#include <string>
#include <vector>
#include <wrl.h>
#include <dxcapi.h>
#include <d3d12.h>
#pragma comment(lib, "dxcompiler.lib")

struct ShaderDesc {
    std::wstring path;          // 例: L"Shader/Object3D.VS.hlsl"
    std::wstring target;        // 例: L"vs_6_0" / L"ps_6_0"
    std::wstring entry = L"main";
    std::vector<DxcDefine> defines; // 例: { {L"USE_FOG", L"1"} }
    bool optimize = true;       // /O3 相当
    bool debugInfo = false;     // /Zi
};

class CompiledShader {
public:
    CompiledShader() = default;
    CompiledShader(Microsoft::WRL::ComPtr<IDxcBlob> blob,
                   Microsoft::WRL::ComPtr<IDxcBlobUtf8> log)
        : blob_(std::move(blob)), log_(std::move(log)) {}
    IDxcBlob* Blob() const { return blob_.Get(); }
    const char* Log() const { return log_ ? (const char*)log_->GetStringPointer() : ""; }
    bool HasBlob() const { return blob_ != nullptr; }

    // D3D12_SHADER_BYTECODE にそのまま渡せる形を返す
    D3D12_SHADER_BYTECODE Bytecode() const {
        D3D12_SHADER_BYTECODE bc{};
        bc.pShaderBytecode = blob_ ? blob_->GetBufferPointer() : nullptr;
        bc.BytecodeLength  = blob_ ? blob_->GetBufferSize() : 0;
        return bc;
    }
private:
    Microsoft::WRL::ComPtr<IDxcBlob> blob_;
    Microsoft::WRL::ComPtr<IDxcBlobUtf8> log_;
};

class ShaderCompiler {
public:
    ShaderCompiler();
    ~ShaderCompiler();

    bool Init();     // DXCの初期化
    void Term();     // リソース解放

    // 失敗時は HasBlob()==false、Log() にエラー文字列
    CompiledShader Compile(const ShaderDesc& desc) const;

private:
    Microsoft::WRL::ComPtr<IDxcUtils> utils_;
    Microsoft::WRL::ComPtr<IDxcCompiler3> compiler_;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;
};
