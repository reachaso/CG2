#include "ShaderCompiler.h"
#include <cassert>

using Microsoft::WRL::ComPtr;

ShaderCompiler::ShaderCompiler() {}
ShaderCompiler::~ShaderCompiler() { Term(); }

bool ShaderCompiler::Init() {
  HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils_));
  if (FAILED(hr))
    return false;
  hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler_));
  if (FAILED(hr))
    return false;
  hr = utils_->CreateDefaultIncludeHandler(&includeHandler_);
  return SUCCEEDED(hr);
}

void ShaderCompiler::Term() {
  includeHandler_.Reset();
  compiler_.Reset();
  utils_.Reset();
}

CompiledShader ShaderCompiler::Compile(const ShaderDesc &desc) const {
  assert(utils_ && compiler_ && includeHandler_);

  // ファイル読み込み
  ComPtr<IDxcBlobEncoding> srcBlob;
  HRESULT hr = utils_->LoadFile(desc.path.c_str(), nullptr, &srcBlob);
  if (FAILED(hr))
    return {};

  // 引数組み立て
  std::vector<LPCWSTR> args;
  args.push_back(desc.path.c_str()); // ソース名（任意）
  args.push_back(L"-E");
  args.push_back(desc.entry.c_str());
  args.push_back(L"-T");
  args.push_back(desc.target.c_str());
  if (desc.optimize) {
    args.push_back(L"-O3");
  } else {
    args.push_back(L"-Od");
  }
  if (desc.debugInfo) {
    args.push_back(L"-Zi");
    args.push_back(L"-Qembed_debug");
  }
  // 行番号を含むPDB名抑制（任意）
  args.push_back(L"-Qstrip_reflect"); // 最小化したい場合

  // マクロ定義
  std::vector<DxcDefine> defs = desc.defines; // コピー（必要なら外で保持）
  DxcBuffer src{};
  src.Ptr = srcBlob->GetBufferPointer();
  src.Size = srcBlob->GetBufferSize();
  src.Encoding = DXC_CP_UTF8;

  ComPtr<IDxcResult> result;
  hr = compiler_->Compile(&src, args.data(), (UINT)args.size(),
                          includeHandler_.Get(), IID_PPV_ARGS(&result));
  if (FAILED(hr) || !result)
    return {};

  ComPtr<IDxcBlobUtf8> errors;
  result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);

  HRESULT status = S_OK;
  result->GetStatus(&status);
  if (FAILED(status)) {
    // 失敗。ログを返す
    return CompiledShader(nullptr, errors);
  }

  ComPtr<IDxcBlob> obj;
  result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&obj), nullptr);
  return CompiledShader(obj, errors);
}
