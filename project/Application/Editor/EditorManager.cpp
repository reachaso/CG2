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
#include "ECS/Entity.h"
#include "ECS/TransformComponent.h"
#include "ECS/ModelRendererComponent.h"
#include "ECS/SkyboxComponent.h"
#include "ECS/SkydomeComponent.h"
#include "ECS/LightComponent.h"
#include "ECS/CameraComponent.h"
#include "ECS/AnimationComponent.h"
#include "ECS/PrimitiveMeshComponent.h"
#include "ECS/SpriteRendererComponent.h"
#include "Render/RenderCommon.h"
#include "Math/Math.h"
#include <algorithm>
#include <filesystem>

void EditorManager::Initialize() {
#if RC_ENABLE_IMGUI
  playState_ = PlayState::Stopped;
  
  playIconTex_ = RC::GetRenderContext().Textures().LoadID("Resources/icons/play.png");
  pauseIconTex_ = RC::GetRenderContext().Textures().LoadID("Resources/icons/pause.png");
  stopIconTex_ = RC::GetRenderContext().Textures().LoadID("Resources/icons/stop.png");

  eyeVisibleTex_ = RC::GetRenderContext().Textures().LoadID("Resources/icons/eye_visible.png");
  eyeHiddenTex_ = RC::GetRenderContext().Textures().LoadID("Resources/icons/eye_hidden.png");
  lockLockedTex_ = RC::GetRenderContext().Textures().LoadID("Resources/icons/lock_locked.png");
  lockUnlockedTex_ = RC::GetRenderContext().Textures().LoadID("Resources/icons/lock_unlocked.png");
  
  folderIconTex_ = RC::GetRenderContext().Textures().LoadID("Resources/icons/folder.png");
  fileIconTex_ = RC::GetRenderContext().Textures().LoadID("Resources/icons/file.png");
  fileImageTex_ = RC::GetRenderContext().Textures().LoadID("Resources/icons/file_image.png");
  file3DTex_ = RC::GetRenderContext().Textures().LoadID("Resources/icons/file_3d.png");
  fileMaterialTex_ = RC::GetRenderContext().Textures().LoadID("Resources/icons/file_material.png");
  fileDocTex_ = RC::GetRenderContext().Textures().LoadID("Resources/icons/file_doc.png");
  fileFontTex_ = RC::GetRenderContext().Textures().LoadID("Resources/icons/file_font.png");

  ApplyDarkTheme();
#endif
}

uint32_t EditorManager::GetSelectedEntityId() const {
  if (auto e = selectedEntity_.lock()) return e->Id();
  return 0;
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

void EditorManager::Update(Dx12Core* core, std::function<void()> onMenuAppend, Scene* currentScene) {
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

    if (currentScene) {
      if (ImGui::BeginMenu("Add")) {
        if (ImGui::BeginMenu("Mesh")) {
          if (ImGui::MenuItem("Cube")) {
            auto e = currentScene->CreateEntity("Cube");
            e->AddComponent<TransformComponent>();
            auto& pm = e->AddComponent<PrimitiveMeshComponent>();
            pm.type = PrimitiveType::Box;
            pm.meshHandle = RC::GenerateBox();
          }
          if (ImGui::MenuItem("Sphere")) {
            auto e = currentScene->CreateEntity("Sphere");
            e->AddComponent<TransformComponent>();
            auto& pm = e->AddComponent<PrimitiveMeshComponent>();
            pm.type = PrimitiveType::Sphere;
            pm.meshHandle = RC::GenerateSphere();
          }
          if (ImGui::MenuItem("Plane")) {
            auto e = currentScene->CreateEntity("Plane");
            e->AddComponent<TransformComponent>();
            auto& pm = e->AddComponent<PrimitiveMeshComponent>();
            pm.type = PrimitiveType::Plane;
            pm.meshHandle = RC::GeneratePlane();
          }
          if (ImGui::MenuItem("Cylinder")) {
            auto e = currentScene->CreateEntity("Cylinder");
            e->AddComponent<TransformComponent>();
            auto& pm = e->AddComponent<PrimitiveMeshComponent>();
            pm.type = PrimitiveType::Cylinder;
            pm.meshHandle = RC::GenerateCylinder();
          }
          if (ImGui::MenuItem("Cone")) {
            auto e = currentScene->CreateEntity("Cone");
            e->AddComponent<TransformComponent>();
            auto& pm = e->AddComponent<PrimitiveMeshComponent>();
            pm.type = PrimitiveType::Cone;
            pm.meshHandle = RC::GenerateCone();
          }
          ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Environment")) {
          if (ImGui::MenuItem("Skybox")) {
            auto e = currentScene->CreateEntity("Skybox");
            e->AddComponent<TransformComponent>();
            e->AddComponent<SkyboxComponent>();
          }
          if (ImGui::MenuItem("Skydome")) {
            auto e = currentScene->CreateEntity("Skydome");
            e->AddComponent<TransformComponent>();
            e->AddComponent<SkydomeComponent>();
          }
          ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Light")) {
          if (ImGui::MenuItem("Directional Light")) {
            auto e = currentScene->CreateEntity("Directional Light");
            e->AddComponent<TransformComponent>();
            e->AddComponent<DirectionalLightComponent>();
          }
          if (ImGui::MenuItem("Point Light")) {
            auto e = currentScene->CreateEntity("Point Light");
            e->AddComponent<TransformComponent>();
            e->AddComponent<PointLightComponent>();
          }
          if (ImGui::MenuItem("Spot Light")) {
            auto e = currentScene->CreateEntity("Spot Light");
            e->AddComponent<TransformComponent>();
            e->AddComponent<SpotLightComponent>();
          }
          if (ImGui::MenuItem("Area Light")) {
            auto e = currentScene->CreateEntity("Area Light");
            e->AddComponent<TransformComponent>();
            e->AddComponent<AreaLightComponent>();
          }
          ImGui::EndMenu();
        }
        ImGui::EndMenu();
      }
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

void EditorManager::DrawUI(D3D12_GPU_DESCRIPTOR_HANDLE viewportSrv, Dx12Core* core, float deltaTime, Scene* currentScene) {
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
      
      // ===== Drop Target =====
      if (ImGui::BeginDragDropTarget()) {
          if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
              std::string droppedPath((const char*)payload->Data);
              std::filesystem::path p(droppedPath);
              std::string ext = p.extension().string();
              std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
              
              if (currentScene) {
                  auto e = currentScene->CreateEntity(p.stem().string());
                  auto& tr = e->AddComponent<TransformComponent>();
                  
                  // ドラッグ先の座標計算 (Screen to World)
                  float mouseX = ImGui::GetMousePos().x - vMin.x;
                  float mouseY = ImGui::GetMousePos().y - vMin.y;
                  RC::Vector3 dropPos = {0, 0, 0};
                  
                  // エディタ（またはゲーム）の現在のカメラを取得
                  RC::CameraController* cam = RC::GetRenderContext().Ctx()->camera;
                  if (cam) {
                      // NDC座標 (-1.0 ～ 1.0)
                      float ndcX = (2.0f * mouseX) / width - 1.0f;
                      float ndcY = 1.0f - (2.0f * mouseY) / height;
                      
                      // ビュー・プロジェクション行列の計算
                      RC::Matrix4x4 view = cam->GetView();
                      RC::Matrix4x4 proj = cam->GetProjection();
                      RC::Matrix4x4 viewProj = ::Multiply(view, proj);
                      RC::Matrix4x4 invViewProj = ::Inverse(viewProj);
                      
                      // Far平面上の点を計算
                      RC::Vector3 farPoint = ::Vector3Transform({ndcX, ndcY, 1.0f}, invViewProj);
                      RC::Vector3 camPos = cam->GetWorldPos();
                      
                      // カメラからFar点へのレイ
                      RC::Vector3 rayDir = ::Normalize(::Subtract(farPoint, camPos));
                      
                      // Y=0 の平面（地面）との交差を求める
                      if (std::abs(rayDir.y) > 0.001f) {
                          float t = -camPos.y / rayDir.y;
                          if (t > 0.0f) {
                              dropPos = ::Add(camPos, ::Multiply(rayDir, t));
                          } else {
                              dropPos = ::Add(camPos, ::Multiply(rayDir, 10.0f)); // カメラの後ろ側を向いてる場合は適当に前に置く
                          }
                      } else {
                          dropPos = ::Add(camPos, ::Multiply(rayDir, 10.0f)); // 水平に見ている場合は適当に前に置く
                      }
                  }
                  tr.position = dropPos;
                  
                  if (ext == ".gltf" || ext == ".obj") {
                      auto& ren = e->AddComponent<ModelRendererComponent>();
                      ren.modelHandle = RC::LoadModel(p.string());
                  } else if (ext == ".png" || ext == ".jpg" || ext == ".dds") {
                      // Note: SpriteRendererComponent requires SceneContext to load correctly, so we skip auto-loading for now.
                  }
              }
          }
          ImGui::EndDragDropTarget();
      }
      // =======================

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
    if (currentScene) {
      for (const auto& e : currentScene->GetEntities()) {
        ImGui::PushID(e->Id());

        // --- 目アイコン（可視切り替え） ---
        {
          int texId = e->IsVisible() ? eyeVisibleTex_ : eyeHiddenTex_;
          auto srv = RC::GetRenderContext().Textures().GetSrv(texId);
          ImVec4 tint = e->IsVisible() ? ImVec4(1,1,1,1) : ImVec4(0.7f,0.7f,0.7f,0.9f);
          ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f,0.3f,0.3f,0.5f));
          ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f,0.2f,0.2f,0.7f));
          if (srv.ptr) {
            if (ImGui::ImageButton("##vis", (ImTextureID)srv.ptr, ImVec2(18, 18), ImVec2(0,0), ImVec2(1,1), ImVec4(0,0,0,0), tint)) {
              e->SetVisible(!e->IsVisible());
            }
          } else {
            if (ImGui::SmallButton(e->IsVisible() ? "V" : "-")) {
              e->SetVisible(!e->IsVisible());
            }
          }
          ImGui::PopStyleColor(3);
        }
        ImGui::SameLine(0, 2);

        // --- 鍵アイコン（ロック切り替え） ---
        {
          int texId = e->IsLocked() ? lockLockedTex_ : lockUnlockedTex_;
          auto srv = RC::GetRenderContext().Textures().GetSrv(texId);
          ImVec4 tint = e->IsLocked() ? ImVec4(1,0.8f,0.2f,1) : ImVec4(0.7f,0.7f,0.7f,0.9f);
          ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f,0.3f,0.3f,0.5f));
          ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f,0.2f,0.2f,0.7f));
          if (srv.ptr) {
            if (ImGui::ImageButton("##lock", (ImTextureID)srv.ptr, ImVec2(18, 18), ImVec2(0,0), ImVec2(1,1), ImVec4(0,0,0,0), tint)) {
              e->SetLocked(!e->IsLocked());
            }
          } else {
            if (ImGui::SmallButton(e->IsLocked() ? "L" : "U")) {
              e->SetLocked(!e->IsLocked());
            }
          }
          ImGui::PopStyleColor(3);
        }
        ImGui::SameLine(0, 4);

        // --- エンティティ名（垂直中央揃え） ---
        ImGui::AlignTextToFramePadding();
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Leaf;
        if (selectedEntity_.lock() == e) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        if (ImGui::TreeNodeEx("##node", flags, "%s", e->Name().c_str())) {
            if (ImGui::IsItemClicked() && !e->IsLocked()) {
                selectedEntity_ = e;
            }
            
            // 右クリックメニュー (Delete) - ロックされていない場合のみ
            if (!e->IsLocked()) {
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Delete")) {
                        e->Destroy();
                        if (selectedEntity_.lock() == e) {
                            selectedEntity_.reset();
                        }
                    }
                    ImGui::EndPopup();
                }
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
        ImGui::Separator();
      }
    }
  }
  ImGui::End();

  // Inspector パネル
  if (ImGui::Begin("Inspector")) {
    if (auto e = selectedEntity_.lock()) {
        ImGui::Text("Entity: %s", e->Name().c_str());
        ImGui::Separator();

        if (auto* tr = e->GetComponent<TransformComponent>()) {
            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
               ImGui::DragFloat3("Position", &tr->position.x, 0.1f);
               
               // Euler 変換 (deg <-> rad)
               RC::Vector3 eulerDegrees = { tr->rotation.x * 180.0f / 3.14159265f, tr->rotation.y * 180.0f / 3.14159265f, tr->rotation.z * 180.0f / 3.14159265f };
               if (ImGui::DragFloat3("Rotation", &eulerDegrees.x, 1.0f)) {
                   tr->rotation = { eulerDegrees.x * 3.14159265f / 180.0f, eulerDegrees.y * 3.14159265f / 180.0f, eulerDegrees.z * 3.14159265f / 180.0f };
               }
               ImGui::DragFloat3("Scale", &tr->scale.x, 0.1f);
            }
        }

        if (auto* ren = e->GetComponent<ModelRendererComponent>()) {
            if (ImGui::CollapsingHeader("Model Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
               ImGui::Checkbox("Visible", &ren->visible);
               ImGui::ColorEdit4("Color##Model", &ren->color.x);
               ImGui::DragFloat("Env Reflection", &ren->environmentCoeff, 0.01f, 0.0f, 1.0f);
               ImGui::Text("Model Handle: %d", ren->modelHandle);
            }
        }

        if (auto* pm = e->GetComponent<PrimitiveMeshComponent>()) {
            if (ImGui::CollapsingHeader("Primitive Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
               ImGui::Checkbox("Visible##PM", &pm->visible);
               static const char* typeNames[] = {
                   "Sphere", "Box", "Plane", "Cylinder", "Cone", "Torus", "Capsule"
               };
               int typeIdx = static_cast<int>(pm->type);
               ImGui::Text("Type: %s", (typeIdx >= 0 && typeIdx < 7) ? typeNames[typeIdx] : "Unknown");
               ImGui::DragFloat("Env Reflection##PM", &pm->environmentCoeff, 0.01f, 0.0f, 1.0f);
               ImGui::Text("Mesh Handle: %d", pm->meshHandle);
            }
        }

        if (auto* spr = e->GetComponent<SpriteRendererComponent>()) {
            if (ImGui::CollapsingHeader("Sprite Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
               ImGui::Checkbox("Visible##Spr", &spr->visible);
               ImGui::DragFloat2("Size", &spr->size.x, 1.0f, 0.0f, 4096.0f);
               ImGui::ColorEdit4("Color##Spr", &spr->color.x);
               ImGui::Text("Sprite Handle: %d", spr->spriteHandle);
            }
        }

        if (auto* anim = e->GetComponent<AnimationComponent>()) {
            if (ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
               ImGui::Checkbox("Playing", &anim->playing);
               ImGui::DragFloat("Speed", &anim->speed, 0.05f, 0.0f, 10.0f);
               // スキンデータがあるモデルのみ Show Skeleton を表示
               if (auto* ren2 = e->GetComponent<ModelRendererComponent>()) {
                   if (ren2->HasModel() && RC::HasModelSkinData(ren2->modelHandle)) {
                       ImGui::Checkbox("Show Skeleton", &anim->showSkeleton);
                   }
               }
               if (!anim->animationPath.empty()) {
                   ImGui::Text("Anim File: %s", anim->animationPath.c_str());
               } else {
                   ImGui::TextDisabled("Anim: Embedded");
               }
            }
        }

        if (auto* skybox = e->GetComponent<SkyboxComponent>()) {
            if (ImGui::CollapsingHeader("Skybox", ImGuiTreeNodeFlags_DefaultOpen)) {
               ImGui::Checkbox("Visible##Skybox", &skybox->visible);
               ImGui::ColorEdit4("Color##Skybox", &skybox->color.x);
               ImGui::Text("Skybox Handle: %d", skybox->skyboxHandle);
            }
        }

        if (auto* skydome = e->GetComponent<SkydomeComponent>()) {
            if (ImGui::CollapsingHeader("Skydome", ImGuiTreeNodeFlags_DefaultOpen)) {
               ImGui::Checkbox("Visible##Skydome", &skydome->visible);
               ImGui::Text("Skydome Handle: %d", skydome->skydomeHandle);
               if (ImGui::ColorEdit4("Color##Skydome", &skydome->color.x)) {
                   RC::SetSkydomeColor(skydome->skydomeHandle, skydome->color);
               }
            }
        }

        // ============================================
        // Lights
        // ============================================
        auto* tr = e->GetComponent<TransformComponent>();
        RC::Vector3 pos = tr ? tr->position : RC::Vector3{0, 0, 0};

        if (auto* dirLight = e->GetComponent<DirectionalLightComponent>()) {
            if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
               ImGui::Checkbox("Visible##DirLight", &dirLight->visible);
               ImGui::ColorEdit4("Color##DirLight", &dirLight->color.x);
               ImGui::DragFloat3("Direction##DirLight", &dirLight->direction.x, 0.05f);
               ImGui::DragFloat("Intensity##DirLight", &dirLight->intensity, 0.1f, 0.0f, 100.0f);
               ImGui::Text("Handle: %d", dirLight->lightHandle);
            }
        }

        if (auto* ptLight = e->GetComponent<PointLightComponent>()) {
            if (ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen)) {
               ImGui::Checkbox("Visible##PtLight", &ptLight->visible);
               ImGui::ColorEdit4("Color##PtLight", &ptLight->color.x);
               ImGui::DragFloat("Intensity##PtLight", &ptLight->intensity, 0.1f, 0.0f, 100.0f);
               ImGui::DragFloat("Radius##PtLight", &ptLight->radius, 0.5f, 0.0f, 1000.0f);
               ImGui::DragFloat("Decay##PtLight", &ptLight->decay, 0.1f, 0.0f, 10.0f);
               ImGui::Text("Handle: %d", ptLight->lightHandle);
            }
        }

        if (auto* spLight = e->GetComponent<SpotLightComponent>()) {
            if (ImGui::CollapsingHeader("Spot Light", ImGuiTreeNodeFlags_DefaultOpen)) {
               ImGui::Checkbox("Visible##SpLight", &spLight->visible);
               ImGui::ColorEdit4("Color##SpLight", &spLight->color.x);
               ImGui::DragFloat3("Direction##SpLight", &spLight->direction.x, 0.05f);
               ImGui::DragFloat("Intensity##SpLight", &spLight->intensity, 0.1f, 0.0f, 100.0f);
               ImGui::DragFloat("Distance##SpLight", &spLight->distance, 0.5f, 0.0f, 1000.0f);
               ImGui::DragFloat("Decay##SpLight", &spLight->decay, 0.1f, 0.0f, 10.0f);
               ImGui::DragFloat("CosAngle##SpLight", &spLight->cosAngle, 0.01f, 0.0f, 1.0f);
               ImGui::Text("Handle: %d", spLight->lightHandle);
            }
        }

        if (auto* arLight = e->GetComponent<AreaLightComponent>()) {
            if (ImGui::CollapsingHeader("Area Light", ImGuiTreeNodeFlags_DefaultOpen)) {
               ImGui::Checkbox("Visible##ArLight", &arLight->visible);
               ImGui::ColorEdit4("Color##ArLight", &arLight->color.x);
               ImGui::DragFloat("Intensity##ArLight", &arLight->intensity, 0.1f, 0.0f, 100.0f);
               ImGui::DragFloat("Range##ArLight", &arLight->range, 0.5f, 0.0f, 1000.0f);
               ImGui::DragFloat("Decay##ArLight", &arLight->decay, 0.1f, 0.0f, 10.0f);
               ImGui::DragFloat("Half Width##ArLight", &arLight->halfWidth, 0.1f, 0.0f, 100.0f);
               ImGui::DragFloat("Half Height##ArLight", &arLight->halfHeight, 0.1f, 0.0f, 100.0f);
               bool twoSided = arLight->twoSided;
               if (ImGui::Checkbox("Two Sided##ArLight", &twoSided)) arLight->twoSided = twoSided;
               ImGui::Text("Handle: %d", arLight->lightHandle);
            }
        }

        // ============================================
        // Camera
        // ============================================
        if (auto* cam = e->GetComponent<CameraComponent>()) {
            if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
               ImGui::Checkbox("Main Camera", &cam->isMain);
               // FOV を度数で表示・編集
               float fovDeg = cam->fovY * 180.0f / 3.14159265f;
               if (ImGui::SliderFloat("FOV (deg)", &fovDeg, 1.0f, 179.0f)) {
                   cam->fovY = fovDeg * 3.14159265f / 180.0f;
               }
               ImGui::DragFloat("Near Clip", &cam->nearZ, 0.01f, 0.001f, 100.0f);
               ImGui::DragFloat("Far Clip", &cam->farZ, 1.0f, 0.1f, 10000.0f);
            }
        }
    } else {
        ImGui::Text("No entity selected");
    }
  }
  ImGui::End();

  // Content Browser パネル
  if (ImGui::Begin("Content Browser")) {
    // フォルダのパスを表示
    std::string currentPathStr = currentDirectory_.string();
    // Windowsのバックスラッシュをスラッシュに置換
    std::replace(currentPathStr.begin(), currentPathStr.end(), '\\', '/');
    ImGui::Text("Assets: %s", currentPathStr.c_str());
    ImGui::SameLine();
    
    // 上の階層へ戻るボタン (Resourcesより上には行かないようにする簡易制御)
    if (ImGui::Button("Up") && currentDirectory_ != "Resources" && currentDirectory_ != "Resources/") {
      currentDirectory_ = currentDirectory_.parent_path();
    }
    ImGui::Separator();

    // グリッドレイアウトの計算
    float cellSize = 90.0f;
    float panelWidth = ImGui::GetContentRegionAvail().x;
    int columnCount = (int)(panelWidth / cellSize);
    if (columnCount < 1) columnCount = 1;

    if (ImGui::BeginTable("ContentTable", columnCount)) {
      if (std::filesystem::exists(currentDirectory_)) {
        for (const auto& entry : std::filesystem::directory_iterator(currentDirectory_)) {
          ImGui::TableNextColumn();
          const auto& path = entry.path();
          std::string filename = path.filename().string();
          
          if (entry.is_directory()) {
            // ディレクトリをアイコンで表示
            ImTextureID iconId = (ImTextureID)RC::GetRenderContext().Textures().GetSrv(folderIconTex_).ptr;
            ImGui::BeginGroup();
            bool clicked = ImGui::ImageButton(filename.c_str(), iconId, ImVec2(cellSize - 30, cellSize - 30));
            ImGui::TextWrapped("%s", filename.c_str());
            ImGui::EndGroup();
            
            if (clicked) {
              currentDirectory_ /= path.filename();
            }
          } else {
            // ファイルをアイコンで表示し、ドラッグ可能にする
            std::string ext = path.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            
            int iconTexId = fileIconTex_;
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") iconTexId = fileImageTex_;
            else if (ext == ".obj" || ext == ".blend" || ext == ".fbx" || ext == ".gltf") iconTexId = file3DTex_;
            else if (ext == ".mtl" || ext == ".mat") iconTexId = fileMaterialTex_;
            else if (ext == ".md" || ext == ".txt" || ext == ".json") iconTexId = fileDocTex_;
            else if (ext == ".ttf" || ext == ".otf") iconTexId = fileFontTex_;
            
            ImTextureID iconId = (ImTextureID)RC::GetRenderContext().Textures().GetSrv(iconTexId).ptr;
            ImGui::BeginGroup();
            ImGui::ImageButton(filename.c_str(), iconId, ImVec2(cellSize - 30, cellSize - 30));
            
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
              std::string payloadPath = path.string();
              // Windowsのバックスラッシュをスラッシュに置換してペイロードに渡す
              std::replace(payloadPath.begin(), payloadPath.end(), '\\', '/');
              ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", payloadPath.c_str(), payloadPath.size() + 1);
              ImGui::Text("Dragging: %s", filename.c_str());
              ImGui::EndDragDropSource();
            }
            
            ImGui::TextWrapped("%s", filename.c_str());
            ImGui::EndGroup();
          }
        }
      }
      ImGui::EndTable();
    }
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
