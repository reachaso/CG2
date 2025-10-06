#include "GraphicsPipeline.h"
#include <dxcapi.h>

void GraphicsPipeline::Term() {
  if (pso_) {
    pso_->Release();
    pso_ = nullptr;
  }
  if (root_) {
    root_->Release();
    root_ = nullptr;
  }
  device_ = nullptr;
}

void GraphicsPipeline::Build(const D3D12_INPUT_ELEMENT_DESC *inputElems,
                             UINT elemCount, D3D12_SHADER_BYTECODE vs,
                             D3D12_SHADER_BYTECODE ps, DXGI_FORMAT rtvFmt,
                             DXGI_FORMAT dsvFmt, D3D12_CULL_MODE cull,
                             D3D12_FILL_MODE fill) {
  assert(device_);
  buildRootSignature_();
  buildPSO_(inputElems, elemCount, vs, ps, rtvFmt, dsvFmt, cull, fill);
}

// ID3DBlob* 版
void GraphicsPipeline::Build(const D3D12_INPUT_ELEMENT_DESC *inputElems,
                             UINT elemCount, ID3DBlob *vsBlob, ID3DBlob *psBlob,
                             DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt,
                             D3D12_CULL_MODE cull, D3D12_FILL_MODE fill) {
  D3D12_SHADER_BYTECODE vs{vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()};
  D3D12_SHADER_BYTECODE ps{psBlob->GetBufferPointer(), psBlob->GetBufferSize()};
  Build(inputElems, elemCount, vs, ps, rtvFmt, dsvFmt, cull, fill);
}
// IDxcBlob* 版
void GraphicsPipeline::Build(const D3D12_INPUT_ELEMENT_DESC *inputElems,
                             UINT elemCount, IDxcBlob *vsBlob, IDxcBlob *psBlob,
                             DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt,
                             D3D12_CULL_MODE cull, D3D12_FILL_MODE fill) {
  D3D12_SHADER_BYTECODE vs{vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()};
  D3D12_SHADER_BYTECODE ps{psBlob->GetBufferPointer(), psBlob->GetBufferSize()};
  Build(inputElems, elemCount, vs, ps, rtvFmt, dsvFmt, cull, fill);
}

void GraphicsPipeline::buildRootSignature_() {
  // ルートパラメータ
  D3D12_ROOT_PARAMETER params[4] = {};
  // 0: CBV b0 (PS)
  params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  params[0].Descriptor.ShaderRegister = 0;
  // 1: CBV b0 (VS)
  params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
  params[1].Descriptor.ShaderRegister = 0;
  // 2: SRV table t0 (PS)
  D3D12_DESCRIPTOR_RANGE range{};
  range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  range.BaseShaderRegister = 0;
  range.NumDescriptors = 1;
  range.OffsetInDescriptorsFromTableStart =
      D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
  params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  params[2].DescriptorTable.NumDescriptorRanges = 1;
  params[2].DescriptorTable.pDescriptorRanges = &range;
  // 3: CBV b1 (PS)
  params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  params[3].Descriptor.ShaderRegister = 1;

  // 静的サンプラ s0
  D3D12_STATIC_SAMPLER_DESC samp{};
  samp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  samp.AddressU = samp.AddressV = samp.AddressW =
      D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  samp.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
  samp.MaxLOD = D3D12_FLOAT32_MAX;
  samp.ShaderRegister = 0;
  samp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  D3D12_ROOT_SIGNATURE_DESC desc{};
  desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
  desc.pParameters = params;
  desc.NumParameters = _countof(params);
  desc.pStaticSamplers = &samp;
  desc.NumStaticSamplers = 1;

  ID3DBlob *sig = nullptr;
  ID3DBlob *err = nullptr;
  HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                           &sig, &err);
  if (FAILED(hr)) {
    if (err)
      OutputDebugStringA((char *)err->GetBufferPointer());
    assert(false);
  }
  hr = device_->CreateRootSignature(0, sig->GetBufferPointer(),
                                    sig->GetBufferSize(), IID_PPV_ARGS(&root_));
  assert(SUCCEEDED(hr));
  if (sig)
    sig->Release();
  if (err)
    err->Release();
}

void GraphicsPipeline::buildPSO_(const D3D12_INPUT_ELEMENT_DESC *inputElems,
                                 UINT elemCount, D3D12_SHADER_BYTECODE vs,
                                 D3D12_SHADER_BYTECODE ps, DXGI_FORMAT rtvFmt,
                                 DXGI_FORMAT dsvFmt, D3D12_CULL_MODE cull,
                                 D3D12_FILL_MODE fill) {
  D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
  d.pRootSignature = root_;
  d.InputLayout = {inputElems, elemCount};
  d.VS = vs;
  d.PS = ps;

  // Blend
  D3D12_BLEND_DESC b{};
  b.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  d.BlendState = b;

  // Rasterizer
  D3D12_RASTERIZER_DESC r{};
  r.CullMode = cull;
  r.FillMode = fill;
  d.RasterizerState = r;

  // Depth
  D3D12_DEPTH_STENCIL_DESC ds{};
  ds.DepthEnable = TRUE;
  ds.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
  ds.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
  d.DepthStencilState = ds;
  d.DSVFormat = dsvFmt;

  // RT
  d.NumRenderTargets = 1;
  d.RTVFormats[0] = rtvFmt;

  d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  d.SampleDesc.Count = 1;
  d.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

  HRESULT hr = device_->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&pso_));
  assert(SUCCEEDED(hr));
}
