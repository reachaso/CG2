#include "Dx12Core.h"
#include <cassert>

void Dx12Core::Init(HWND hwnd, const Desc &d) {
  desc_ = d;

  // Device
  device_.Init(d.debug, d.gpuValidation);
  device_.SetupInfoQueue(true, true);
  allowTearing_ = d.allowTearingIfSupported && device_.IsTearingSupported();

  ID3D12Device *dev = device_.GetDevice();

  // Command
  cmd_.Init(dev, D3D12_COMMAND_LIST_TYPE_DIRECT, d.frameCount);

  // Heaps
  rtv_.Init(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, d.frameCount, false);
  dsv_.Init(dev, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
  srv_.Init(dev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, d.srvHeapCapacity,
            true);

  // SwapChain
  swap_.SetRtvHeap(rtv_.Heap(), rtv_.Increment());
  // スワップチェーンは UNORM で作成（RTVはSRGBビューで作る）
  DXGI_FORMAT scFormat =
      (desc_.rtvFormat == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
          ? DXGI_FORMAT_R8G8B8A8_UNORM
          : desc_.rtvFormat; // それ以外はそのまま
  swap_.Init(device_.Factory(), dev, cmd_.Queue(), hwnd, d.width, d.height,
              scFormat, d.frameCount, allowTearing_);

  // Depth
  depth_.Init(dev, d.width, d.height, dsv_, d.dsvFormat, d.dsvFormat);

  // Pipeline（頂点レイアウトは最小例。必要なら外から差し替え可）
  D3D12_INPUT_ELEMENT_DESC inputElems[3] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
       D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
       0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
       D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
       0},
  };
  pipeline_.Init(dev);
  // シェーダは呼び出し側で用意してから Build する運用でもOK。
  // とりあえず空で置いておき、呼び出し側で Build し直しても良い。
}

void Dx12Core::BeginFrame() {
  backIndex_ = swap_.CurrentBackBufferIndex();
  cmd_.BeginFrame(backIndex_);

  // Present → RenderTarget
  cmd_.Transition(swap_.BackBuffer(backIndex_), D3D12_RESOURCE_STATE_PRESENT,
                  D3D12_RESOURCE_STATE_RENDER_TARGET);

  // OM バインド
  auto *cl = cmd_.List();
  auto rtv = swap_.RtvAt(backIndex_);
  auto dsv = depth_.Dsv();
  cl->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

  // SRV heap セット（ImGui 等を先頭に置いている前提）
  ID3D12DescriptorHeap *heaps[] = {srv_.Heap()};
  cl->SetDescriptorHeaps(1, heaps);
}

void Dx12Core::Clear(float r, float g, float b, float a) {
  float clear[4] = {r, g, b, a};
  cmd_.List()->ClearRenderTargetView(CurrentRTV(), clear, 0, nullptr);
  cmd_.List()->ClearDepthStencilView(
      Dsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0,
      nullptr);
}

void Dx12Core::EndFrame() {
  // RenderTarget → Present
  cmd_.Transition(swap_.BackBuffer(backIndex_),
                  D3D12_RESOURCE_STATE_RENDER_TARGET,
                  D3D12_RESOURCE_STATE_PRESENT);

  cmd_.EndFrame();
  // vsync=1, tearingなら 0 でもOK（好みで）
  swap_.Present(1, 0);
  cmd_.WaitForFrame(backIndex_);
}

void Dx12Core::WaitForGPU() { cmd_.FlushGPU(); }

void Dx12Core::Term() {
  cmd_.FlushGPU();
  pipeline_.Term();
  depth_.Term();
  swap_.Term();
  srv_.Term();
  dsv_.Term();
  rtv_.Term();
  cmd_.Term();
  device_.Term();
}
