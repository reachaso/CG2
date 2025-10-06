#include "ImGuiManager.h"
#include <cassert>

void ImGuiManager::reserveSrvSlotForImGui_(Dx12Core &core) {
  // ImGui_ImplDX12_Init に渡すCPU/GPUハンドルは固定スロットを想定。
  // 本プロジェクトでは先頭[0]をImGui専用にし、その後ろから通常SRVを割り当てます。
  // main.cppでも先頭をImGui用に確保していました（AllocateCPU(1)）。:contentReference[oaicite:2]{index=2}
  core.SRV().AllocateCPU(1);
}

void ImGuiManager::Init(HWND hwnd, Dx12Core &core, float jpFontSize,
                        const char *jpFontPath) {
  if (initialized_)
    return;

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  // フォント（日本語＋既定）
  ImGuiIO &io = ImGui::GetIO();
  // 既定フォントはImGui内部のデフォルトを使いつつ…
  fontDefault_ = io.FontDefault;
  // 日本語フォントを追加
  fontJP_ = io.Fonts->AddFontFromFileTTF(jpFontPath, jpFontSize, nullptr,
                                         io.Fonts->GetGlyphRangesJapanese());
  if (fontJP_) {
    io.FontDefault = fontJP_; // 既定を日本語フォントに
  }

  // Platform / Renderer
  ImGui_ImplWin32_Init(hwnd);
  ImGui_ImplDX12_Init(
      core.GetDevice(),
      core.FrameCount(), // フレームインフライト数（Dx12Core）:contentReference[oaicite:3]{index=3}
      DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, // RTVフォーマット（本プロジェクト既定）:contentReference[oaicite:4]{index=4}
      core.SRV().Heap(),                         // SRVヒープ
      core.SRV().CPUAt(0), core.SRV().GPUAt(0)); // 先頭スロットをImGuiに割当

  reserveSrvSlotForImGui_(core); // 以降のSRVはスロット1以降を使用

  initialized_ = true;
}

void ImGuiManager::NewFrame() {
  // ImGuiのフレーム開始（呼び出し順はこの通り）
  ImGui_ImplDX12_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
}

void ImGuiManager::Render(ID3D12GraphicsCommandList *cmdList) {
  // ここでは「ImGuiの」Renderのみ行う。アプリ側でUI構築を終えてから呼ぶこと。
  ImGui::Render();
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);
}

void ImGuiManager::Shutdown() {
  if (!initialized_)
    return;
  ImGui_ImplDX12_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
  initialized_ = false;
}
