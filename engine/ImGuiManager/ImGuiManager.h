#pragma once
#include "Dx12Core.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx12.h"
#include "imgui/imgui_impl_win32.h"
#include <Windows.h>
#include <d3d12.h>

class ImGuiManager {
public:
  ImGuiManager() = default;
  ~ImGuiManager() = default;

  void
  Init(HWND hwnd, Dx12Core &core, float jpFontSize = 15.0f,
       const char *jpFontPath = "Resources/fonts/Huninn/Huninn-Regular.ttf");

  // 1フレーム開始（NewFrame）
  void NewFrame();

  // 収集済みDrawDataをDX12コマンドリストへ描画
  void Render(ID3D12GraphicsCommandList *cmdList);

  // 明示終了
  void Shutdown();

  ImFont *GetDefaultFont() const { return fontDefault_; }
  ImFont *GetJPFont() const { return fontJP_; }

private:
  bool initialized_ = false;
  ImFont *fontDefault_ = nullptr;
  ImFont *fontJP_ = nullptr;

  // ImGui用にSRVヒープの先頭スロットを占有（0固定運用）
  // ※ core.SRV().CPUAt(0)/GPUAt(0) を使い、その後に1つ進める
  void reserveSrvSlotForImGui_(Dx12Core &core);
};
