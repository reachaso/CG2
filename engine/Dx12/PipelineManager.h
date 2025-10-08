#pragma once
#include "GraphicsPipeline/GraphicsPipeline.h"
#include "ShaderCompiler/ShaderCompiler.h"
#include <d3d12.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

enum class InputLayoutType {
  Object3D,
  // 他のレイアウトタイプをここに追加
};

struct PipelineDesc {
  // シェーダ
  std::wstring vsPath = L"";
  std::wstring vsEntry = L"main";
  std::wstring vsTarget = L"vs_6_0";
  std::wstring psPath = L"";
  std::wstring psEntry = L"main";
  std::wstring psTarget = L"ps_6_0";
  bool optimize = true;
  bool debugInfo = false;

  // 入力レイアウト（実体を保持してポインタ寿命問題を回避）
  std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;

  // ラスタ/フィル
  D3D12_CULL_MODE cull = D3D12_CULL_MODE_BACK;
  D3D12_FILL_MODE fill = D3D12_FILL_MODE_SOLID;
};

class PipelineManager {
public:
  void Init(ID3D12Device *device, DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt);

  void Term();

  // HLSLファイルから作成（キーで管理）
  GraphicsPipeline *CreateFromFiles(const std::string &key,
                                    const PipelineDesc &desc);

  // HLSLファイルから作成
  GraphicsPipeline *CreateFromFiles(const std::string &key, const std::wstring &vsPath,
                  const std::wstring &psPath,
                  InputLayoutType layoutType = InputLayoutType::Object3D);

  bool Rebuild(const std::string &key);

  // 取得系
  bool Exists(const std::string &key) const {
    return pipelines_.count(key) > 0;
  }
  GraphicsPipeline *Get(const std::string &key) {
    auto it = pipelines_.find(key);
    return (it == pipelines_.end()) ? nullptr : it->second.pipeline.get();
  }

private:
  GraphicsPipeline *createFromBlobs_(const std::string &key,
                                     const PipelineDesc &desc, IDxcBlob *vs,
                                     IDxcBlob *ps);

  // 入力レイアウトの定義を取得
  static std::vector<D3D12_INPUT_ELEMENT_DESC> GetInputLayout(InputLayoutType type);

  struct Entry {
    PipelineDesc desc;
    std::unique_ptr<GraphicsPipeline> pipeline;
  };

private:
  // 非所有
  ID3D12Device *device_ = nullptr;
  DXGI_FORMAT rtvFmt_{DXGI_FORMAT_R8G8B8A8_UNORM_SRGB};
  DXGI_FORMAT dsvFmt_{DXGI_FORMAT_D24_UNORM_S8_UINT};

  ShaderCompiler compiler_; // 内部で使い回し
  std::unordered_map<std::string, Entry> pipelines_;
};
