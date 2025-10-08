#include "PipelineManager.h"

void PipelineManager::Init(ID3D12Device *device, DXGI_FORMAT rtvFmt,
                           DXGI_FORMAT dsvFmt) {
  device_ = device;
  rtvFmt_ = rtvFmt;
  dsvFmt_ = dsvFmt;
  bool ok = compiler_.Init();
  assert(ok && "ShaderCompiler::Init failed");
}

void PipelineManager::Term() {
  // 生成済みPipelineを解放
  for (auto &kv : pipelines_) {
    if (kv.second.pipeline)
      kv.second.pipeline->Term();
  }
  pipelines_.clear();
  device_ = nullptr;
}

// HLSLファイルから作成（キーで管理）
GraphicsPipeline *PipelineManager::CreateFromFiles(const std::string &key,
                                                   const PipelineDesc &desc) {
  // シェーダをコンパイル
  ShaderDesc vs{};
  vs.path = desc.vsPath.c_str();
  vs.target = desc.vsTarget.c_str();
  vs.entry = desc.vsEntry.c_str();
  vs.optimize = desc.optimize;
  vs.debugInfo = desc.debugInfo;

  ShaderDesc ps{};
  ps.path = desc.psPath.c_str();
  ps.target = desc.psTarget.c_str();
  ps.entry = desc.psEntry.c_str();
  ps.optimize = desc.optimize;
  ps.debugInfo = desc.debugInfo;

  CompiledShader VS = compiler_.Compile(vs);
  CompiledShader PS = compiler_.Compile(ps);
  assert(VS.HasBlob() && PS.HasBlob());

  return createFromBlobs_(key, desc, VS.Blob(), PS.Blob());
}

GraphicsPipeline *PipelineManager::CreateFromFiles(const std::string &key,
                                                   const std::wstring &vsPath,
                                                   const std::wstring &psPath,
                                                   InputLayoutType layoutType) {
  PipelineDesc pdesc{};
  pdesc.vsPath = vsPath;
  pdesc.psPath = psPath;
  pdesc.inputLayout = GetInputLayout(layoutType);
#ifdef _DEBUG
  pdesc.optimize = false;
  pdesc.debugInfo = true;
#else
  pdesc.optimize = true;
  pdesc.debugInfo = false;
#endif
  return CreateFromFiles(key, pdesc);
}

bool PipelineManager::Rebuild(const std::string &key) {
  auto it = pipelines_.find(key);
  if (it == pipelines_.end())
    return false;
  const PipelineDesc &desc = it->second.desc;

  ShaderDesc vs{}, ps{};
  vs.path = desc.vsPath.c_str();
  vs.target = desc.vsTarget.c_str();
  vs.entry = desc.vsEntry.c_str();
  vs.optimize = desc.optimize;
  vs.debugInfo = desc.debugInfo;
  ps.path = desc.psPath.c_str();
  ps.target = desc.psTarget.c_str();
  ps.entry = desc.psEntry.c_str();
  ps.optimize = desc.optimize;
  ps.debugInfo = desc.debugInfo;

  CompiledShader VS = compiler_.Compile(vs);
  CompiledShader PS = compiler_.Compile(ps);
  if (!VS.HasBlob() || !PS.HasBlob())
    return false;

  // いったん解体して再Build
  it->second.pipeline->Term();
  it->second.pipeline->Init(device_);
  it->second.pipeline->Build(
      it->second.desc.inputLayout.data(),
      static_cast<UINT>(it->second.desc.inputLayout.size()), VS.Blob(),
      PS.Blob(), rtvFmt_, dsvFmt_, it->second.desc.cull, it->second.desc.fill);
  return true;
}

GraphicsPipeline *PipelineManager::createFromBlobs_(const std::string &key,
                                                    const PipelineDesc &desc,
                                                    IDxcBlob *vs,
                                                    IDxcBlob *ps) {
  Entry e;
  e.desc = desc;
  e.pipeline = std::make_unique<GraphicsPipeline>();
  e.pipeline->Init(device_);
  e.pipeline->Build(e.desc.inputLayout.data(),
                    static_cast<UINT>(e.desc.inputLayout.size()), vs, ps,
                    rtvFmt_, dsvFmt_, e.desc.cull, e.desc.fill);
  auto &ref = pipelines_[key] = std::move(e);
  return ref.pipeline.get();
}

std::vector<D3D12_INPUT_ELEMENT_DESC>
PipelineManager::GetInputLayout(InputLayoutType type) {
  switch (type) {
  case InputLayoutType::Object3D:
    return {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
  }
  return {}; // 不明なタイプ
}
