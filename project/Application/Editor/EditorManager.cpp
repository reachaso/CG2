#include "EditorManager.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "Log/Log.h"
#include "Dx12/Dx12Core.h"
#include "Dx12/Utility/ScreenCapture.h"
#include "RC.h"
#include "../Game/Scene/Scene.h"
#include "Render/RenderContext.h"
#include "Graphics/Texture/TextureManager/TextureManager.h"

void EditorManager::Initialize() {
#if RC_ENABLE_IMGUI
  playState_ = PlayState::Stopped;
  
  playIconTex_ = RC::GetRenderContext().Textures().LoadID("Resources/icon/play.png");
  pauseIconTex_ = RC::GetRenderContext().Textures().LoadID("Resources/icon/pause.png");
  stopIconTex_ = RC::GetRenderContext().Textures().LoadID("Resources/icon/stop.png");

  ApplyDarkTheme();
#endif
}

void EditorManager::ApplyDarkTheme() {
#if RC_ENABLE_IMGUI
  ImGuiStyle& style = ImGui::GetStyle();
  ImVec4* colors = style.Colors;

  // UE5/Unity-like dark theme
  colors[ImGuiCol_WindowBg]           = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
  colors[ImGuiCol_ChildBg]            = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
  colors[ImGuiCol_PopupBg]            = ImVec4(0.10f, 0.10f, 0.10f, 0.98f);
  colors[ImGuiCol_Border]             = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
  colors[ImGuiCol_BorderShadow]       = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_FrameBg]            = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
  colors[ImGuiCol_FrameBgHovered]     = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
  colors[ImGuiCol_FrameBgActive]      = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
  colors[ImGuiCol_TitleBg]            = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
  colors[ImGuiCol_TitleBgActive]      = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
  colors[ImGuiCol_TitleBgCollapsed]   = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
  colors[ImGuiCol_MenuBarBg]          = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
  colors[ImGuiCol_ScrollbarBg]        = ImVec4(0.05f, 0.05f, 0.05f, 0.50f);
  colors[ImGuiCol_ScrollbarGrab]      = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabHovered]= ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabActive]= ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
  colors[ImGuiCol_CheckMark]          = ImVec4(0.85f, 0.45f, 0.05f, 1.00f); // Orange accent
  colors[ImGuiCol_SliderGrab]         = ImVec4(0.85f, 0.45f, 0.05f, 1.00f);
  colors[ImGuiCol_SliderGrabActive]   = ImVec4(0.95f, 0.55f, 0.15f, 1.00f);
  colors[ImGuiCol_Button]             = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
  colors[ImGuiCol_ButtonHovered]      = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
  colors[ImGuiCol_ButtonActive]       = ImVec4(0.85f, 0.45f, 0.05f, 1.00f);
  colors[ImGuiCol_Header]             = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
  colors[ImGuiCol_HeaderHovered]      = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
  colors[ImGuiCol_HeaderActive]       = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
  colors[ImGuiCol_Separator]          = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
  colors[ImGuiCol_SeparatorHovered]   = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
  colors[ImGuiCol_SeparatorActive]    = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
  colors[ImGuiCol_ResizeGrip]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
  colors[ImGuiCol_ResizeGripHovered]  = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
  colors[ImGuiCol_ResizeGripActive]   = ImVec4(0.85f, 0.45f, 0.05f, 1.00f);
  colors[ImGuiCol_Tab]                = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
  colors[ImGuiCol_TabHovered]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
  colors[ImGuiCol_TabActive]          = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
  colors[ImGuiCol_TabUnfocused]       = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
  colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
  colors[ImGuiCol_DockingPreview]     = ImVec4(0.85f, 0.45f, 0.05f, 0.50f);
  colors[ImGuiCol_DockingEmptyBg]     = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);

  style.WindowRounding    = 4.0f;
  style.ChildRounding     = 4.0f;
  style.FrameRounding     = 4.0f;
  style.PopupRounding     = 4.0f;
  style.ScrollbarRounding = 4.0f;
  style.GrabRounding      = 4.0f;
  style.TabRounding       = 4.0f;
  style.WindowBorderSize  = 1.0f;
  style.FrameBorderSize   = 1.0f;
#endif
}

void EditorManager::Update(Dx12Core* core, std::function<void()> onMenuAppend) {
#if RC_ENABLE_IMGUI
  // ============================
  // メニューバー
  // ============================
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Exit")) {
        PostQuitMessage(0);
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Window")) {
      ImGui::MenuItem("ImGui Demo", nullptr, &showDemoWindow_);
      ImGui::MenuItem("Performance (FPS)", nullptr, &showPerfWindow_);
      if (ImGui::MenuItem("Reset Layout")) {
        resetLayout_ = true;
      }
      ImGui::EndMenu();
    }

    if (onMenuAppend) {
      onMenuAppend();
    }

    // キャプチャ・録画機能の直接ボタン
    if (ImGui::MenuItem("スクリーンショット")) {
      if (core) core->RequestScreenshot();
    }
    
    bool isRecording = core && core->GetVideoRecorder().IsRecording();
    const char* recLabel = isRecording ? "画面録画停止" : "画面録画開始";
    if (ImGui::MenuItem(recLabel)) {
      if (core) {
        if (isRecording) core->StopRecording();
        else core->StartRecording();
      }
    }

    // ----------------------------
    // 中央の Play / Pause / Stop ボタン
    // ----------------------------
    float playButtonWidth = 40.0f;
    float playButtonsTotalWidth = playButtonWidth * 3.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
    ImGui::SameLine((ImGui::GetWindowWidth() - playButtonsTotalWidth) * 0.5f);

    // Play ボタン
    if (playState_ == PlayState::Playing) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    } else {
      ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Button));
    }
    ImTextureID playId = (ImTextureID)RC::GetRenderContext().Textures().GetSrv(playIconTex_).ptr;
    if (ImGui::ImageButton("##Play", playId, ImVec2(16.0f, 16.0f))) {
      playState_ = PlayState::Playing;
    }
    ImGui::PopStyleColor();

    ImGui::SameLine();

    // Pause ボタン
    if (playState_ == PlayState::Paused) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.6f, 0.2f, 1.0f));
    } else {
      ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Button));
    }
    ImTextureID pauseId = (ImTextureID)RC::GetRenderContext().Textures().GetSrv(pauseIconTex_).ptr;
    if (ImGui::ImageButton("##Pause", pauseId, ImVec2(16.0f, 16.0f))) {
      playState_ = PlayState::Paused;
    }
    ImGui::PopStyleColor();

    ImGui::SameLine();

    // Stop ボタン
    if (playState_ == PlayState::Stopped) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
    } else {
      ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Button));
    }
    ImTextureID stopId = (ImTextureID)RC::GetRenderContext().Textures().GetSrv(stopIconTex_).ptr;
    if (ImGui::ImageButton("##Stop", stopId, ImVec2(16.0f, 16.0f))) {
      // 停止ボタンが押された場合、App側で検知してシーンをリロードさせる
      playState_ = PlayState::Stopped;
    }
    ImGui::PopStyleColor();

    // ----------------------------
    // 右上のウィンドウコントロールボタン
    // ----------------------------
    float buttonWidth = 36.0f;
    float buttonCount = 2.0f;
    float totalWidth = buttonWidth * buttonCount;
    float menuBarHeight = ImGui::GetWindowSize().y;
    
    // カーソルを右端へ移動
    ImGui::SameLine(ImGui::GetWindowWidth() - totalWidth);
    
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0)); // ボタン間の隙間をなくす
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0)); // 背景透明
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 0.5f));

    HWND hwnd = (HWND)ImGui::GetMainViewport()->PlatformHandleRaw;

    // 最小化ボタン (-)
    if (ImGui::Button("ー", ImVec2(buttonWidth, menuBarHeight))) {
        if (hwnd) ShowWindow(hwnd, SW_MINIMIZE);
    }
    ImGui::SameLine();
    // 閉じる (X)
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.3f, 0.3f, 0.8f));
    if (ImGui::Button("X", ImVec2(buttonWidth, menuBarHeight))) {
        if (hwnd) PostMessage(hwnd, WM_CLOSE, 0, 0);
        else PostQuitMessage(0);
    }
    
    ImGui::PopStyleColor(2); // Xボタン用の色を戻す
    ImGui::PopStyleColor(3); // 透明背景などの色を戻す
    ImGui::PopStyleVar(2); // FramePadding, ItemSpacing

    ImGui::EndMainMenuBar();
  }

  // レイアウト初期化処理
  SetupDockingLayout();

#endif
}

void EditorManager::SetupDockingLayout() {
#if RC_ENABLE_IMGUI
  ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");

  if (firstLayout_) {
    firstLayout_ = false;
    // imgui.ini からロードされたノードが存在しない場合のみ初回レイアウトを構築
    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
      resetLayout_ = true;
    }
  }

  if (!resetLayout_) return;
  resetLayout_ = false;
  
  // 既存のレイアウトをクリア
  ImGui::DockBuilderRemoveNode(dockspace_id); 
  ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

  // ウィンドウを分割していく
  ImGuiID dock_main_id = dockspace_id;
  ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
  ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
  ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.30f, nullptr, &dock_main_id);

  // パネルを各ノードに割り当て
  ImGui::DockBuilderDockWindow("Viewport", dock_main_id);
  ImGui::DockBuilderDockWindow("Hierarchy", dock_id_left);
  ImGui::DockBuilderDockWindow("Inspector", dock_id_right);
  ImGui::DockBuilderDockWindow("Content Browser", dock_id_bottom);
  ImGui::DockBuilderDockWindow("Console", dock_id_bottom);

  ImGui::DockBuilderFinish(dockspace_id);
#endif
}

void EditorManager::DrawUI(D3D12_GPU_DESCRIPTOR_HANDLE viewportSrv, Dx12Core* core, float deltaTime) {
#if RC_ENABLE_IMGUI
  if (showDemoWindow_) {
    ImGui::ShowDemoWindow(&showDemoWindow_);
  }

  // Performance パネル
  if (showPerfWindow_) {
    if (ImGui::Begin("Performance", &showPerfWindow_)) {
      ImGuiIO &io = ImGui::GetIO();
      const float fps = io.Framerate;
      ImGui::Text("FPS: %.1f", fps);
      ImGui::Text("Frame: %.3f ms", 1000.0f / (fps > 0.0f ? fps : 1.0f));
    }
    ImGui::End();
  }

  // Viewport パネル
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); // 余白なし
  if (ImGui::Begin("Viewport")) {
    isViewportHovered_ = ImGui::IsWindowHovered();
    ImVec2 vMin = ImGui::GetWindowContentRegionMin();
    ImVec2 vMax = ImGui::GetWindowContentRegionMax();
    vMin.x += ImGui::GetWindowPos().x;
    vMin.y += ImGui::GetWindowPos().y;
    vMax.x += ImGui::GetWindowPos().x;
    vMax.y += ImGui::GetWindowPos().y;
    
    float width = vMax.x - vMin.x;
    float height = vMax.y - vMin.y;

    // ゲーム描画用SRVをImGuiのImageとして表示
    if (viewportSrv.ptr != 0 && width > 0 && height > 0) {
      ImGui::Image((ImTextureID)viewportSrv.ptr, ImVec2(width, height));
      
      // シェーディングモードのアイコン群をビューポート右上にオーバーレイ表示
      // 描画開始位置を決定 (上部のバーと重ならないようY座標を少し下げる)
      ImGui::SetCursorPos(ImVec2(width - 186.0f, 24.0f)); 
      ImVec2 cursorPos = ImGui::GetCursorScreenPos();
      
      // 幅は「ボタン6個(24px) + 隙間5個(4px) = 164px」に左右余白6pxずつ足して 176px にする
      ImVec2 overlaySize = ImVec2(176.0f, 32.0f);
      
      // ボタン群（高さ24px）の背景として、上下左右に余白を持たせた半透明の枠を描画
      // cursorPos は最初のボタンの左上絶対座標。枠は少し左・上にずらして描画する。
      ImGui::GetWindowDrawList()->AddRectFilled(
          ImVec2(cursorPos.x - 6.0f, cursorPos.y - 4.0f), 
          ImVec2(cursorPos.x - 6.0f + overlaySize.x, cursorPos.y - 4.0f + overlaySize.y), 
          IM_COL32(20, 20, 20, 160), 
          6.0f
      );

      // ボタンの描画を実行
      RC::DrawViewShadingModeImGui("");
      
    } else {
      ImGui::Text("No Viewport Texture");
    }
  }
  ImGui::End();
  ImGui::PopStyleVar();

  // Hierarchy パネル
  if (ImGui::Begin("Hierarchy")) {
    ImGui::Text("Scene Root");
    if (ImGui::TreeNode("Directional Light")) { ImGui::TreePop(); }
    if (ImGui::TreeNode("Main Camera")) { ImGui::TreePop(); }
    if (ImGui::TreeNode("Player")) { ImGui::TreePop(); }
  }
  ImGui::End();

  // Inspector パネル
  if (ImGui::Begin("Inspector")) {
    ImGui::Text("Properties");
    ImGui::Separator();
    float pos[3] = {0.0f, 0.0f, 0.0f};
    ImGui::DragFloat3("Position", pos);
    float rot[3] = {0.0f, 0.0f, 0.0f};
    ImGui::DragFloat3("Rotation", rot);
    float scl[3] = {1.0f, 1.0f, 1.0f};
    ImGui::DragFloat3("Scale", scl);
  }
  ImGui::End();

  // Content Browser パネル
  if (ImGui::Begin("Content Browser")) {
    ImGui::Text("Assets");
    // TODO: Asset List
  }
  ImGui::End();

  // Console パネル
  if (ImGui::Begin("Console")) {
    if (ImGui::Button("Clear")) {
      Log::ClearHistory();
    }
    ImGui::Separator();

    // スクロール可能な領域を作成
    ImGui::BeginChild("LogScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    const auto& history = Log::GetHistory();
    for (const auto& msg : history) {
      ImGui::TextUnformatted(msg.c_str());
    }
    
    // オートスクロール（一番下にいる場合のみ追従）
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
      ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
  }
  ImGui::End();

  // === Screenshot Pop-out ===
  if (core) {
    ScreenCapture::DrawImGui(deltaTime, core);
  }

#endif
}

PlayState EditorManager::GetPlayState() const {
  return playState_;
}

void EditorManager::SetPlayState(PlayState state) {
  playState_ = state;
}
