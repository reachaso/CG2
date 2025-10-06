#pragma once
#include <cassert>
#include <d3d12.h>
#include <vector>

class GraphicsPipeline {
public:
  void Init(ID3D12Device *device) { device_ = device; }
  void Term();

  // ルートシグネチャ＋PSOをまとめて構築
  // rtvFmt: 例 DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
  // dsvFmt: 例 DXGI_FORMAT_D24_UNORM_S8_UINT
  // 共通ビルド（バイトコードを直接受け取る）
  void Build(const D3D12_INPUT_ELEMENT_DESC *inputElems, UINT elemCount,
             D3D12_SHADER_BYTECODE vs, D3D12_SHADER_BYTECODE ps,
             DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt,
             D3D12_CULL_MODE cull = D3D12_CULL_MODE_BACK,
             D3D12_FILL_MODE fill = D3D12_FILL_MODE_SOLID);
  // 便利オーバーロード：ID3DBlob*
  void Build(const D3D12_INPUT_ELEMENT_DESC *inputElems, UINT elemCount,
             ID3DBlob *vs, ID3DBlob *ps, DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt,
             D3D12_CULL_MODE cull = D3D12_CULL_MODE_BACK,
             D3D12_FILL_MODE fill = D3D12_FILL_MODE_SOLID);
  // 便利オーバーロード：IDxcBlob*
  void Build(const D3D12_INPUT_ELEMENT_DESC *inputElems, UINT elemCount,
             struct IDxcBlob *vs, struct IDxcBlob *ps, DXGI_FORMAT rtvFmt,
             DXGI_FORMAT dsvFmt, D3D12_CULL_MODE cull = D3D12_CULL_MODE_BACK,
             D3D12_FILL_MODE fill = D3D12_FILL_MODE_SOLID);

  // バインド用
  ID3D12RootSignature *Root() const { return root_; }
  ID3D12PipelineState *PSO() const { return pso_; }

private:
  void buildRootSignature_();
  void buildPSO_(const D3D12_INPUT_ELEMENT_DESC *inputElems, UINT elemCount,
                 D3D12_SHADER_BYTECODE vs, D3D12_SHADER_BYTECODE ps,
                 DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt, D3D12_CULL_MODE cull,
                 D3D12_FILL_MODE fill);

private:
  ID3D12Device *device_ = nullptr;      // 非所有
  ID3D12RootSignature *root_ = nullptr; // 所有
  ID3D12PipelineState *pso_ = nullptr;  // 所有
};
