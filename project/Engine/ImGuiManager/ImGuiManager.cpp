#include "ImGuiManager.h"
#include "Log/Log.h"
#include <cassert>
#include <string>

void ImGuiManager::reserveSrvSlotForImGui_(Dx12Core &core) {
  core.SRV().AllocateCPU(
      1); // スロット0を確保（以降は1〜） :contentReference[oaicite:5]{index=5}
}

void ImGuiManager::Init(HWND hwnd, Dx12Core &core, bool enableDocking, float jpFontSize,
                        const char *jpFontPath) {
  if (initialized_)
    return;

#if RC_ENABLE_IMGUI
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  dockingEnabled_ = enableDocking;
  if (dockingEnabled_) {
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  }

  ImGuiIO &io = ImGui::GetIO();
  fontDefault_ = io.FontDefault;
  fontJP_ = io.Fonts->AddFontFromFileTTF(jpFontPath, jpFontSize, nullptr,
                                         io.Fonts->GetGlyphRangesJapanese());
  if (fontJP_)
    io.FontDefault = fontJP_;

  ImGui_ImplWin32_Init(hwnd);

  ImGui_ImplDX12_InitInfo initInfo = {};
  initInfo.Device = core.GetDevice();
  initInfo.CommandQueue = core.Queue();
  initInfo.NumFramesInFlight = core.FrameCount();
  initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  initInfo.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
  initInfo.SrvDescriptorHeap = core.SRV().Heap();
  initInfo.LegacySingleSrvCpuDescriptor = core.SRV().CPUAt(0);
  initInfo.LegacySingleSrvGpuDescriptor = core.SRV().GPUAt(0);

  ImGui_ImplDX12_Init(&initInfo);
#endif

  // ImGui有無に関わらずスロット0は予約して並びを固定
  reserveSrvSlotForImGui_(core);

  initialized_ = true;
}

void ImGuiManager::NewFrame() {
#if RC_ENABLE_IMGUI
  if (!initialized_)
    return;
  ImGui_ImplDX12_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();

  if (dockingEnabled_) {
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpaceOverViewport(dockspace_id, nullptr, ImGuiDockNodeFlags_None);
  }
#endif
}

void ImGuiManager::Render(ID3D12GraphicsCommandList *cmdList) {
#if RC_ENABLE_IMGUI
  if (!initialized_)
    return;
  ImGui::Render();
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);
#endif
}

void ImGuiManager::Shutdown() {
#if RC_ENABLE_IMGUI
  if (!initialized_)
    return;
  ImGui_ImplDX12_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
#endif
  initialized_ = false;
}
