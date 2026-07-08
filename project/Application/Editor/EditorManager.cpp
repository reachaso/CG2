#include "EditorManager.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/ImGuizmo.h"
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
#include "ECS/WaterComponent.h"
#include "ECS/RigidbodyComponent.h"
#include "Render/RenderCommon.h"
#include "Math/Math.h"
#include "Math/MathUtils.h"
#include "Camera/CameraMath.h"
#include "ECS/ColliderComponent.h"
#include "ECS/NativeScriptComponent.h"
#include "ECS/ScriptRegistry.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <format>
#include <memory>
#include <nlohmann/json.hpp>
#include <set>

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

  // 設定のロード
  LoadConfig();
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
      ImGui::MenuItem("Render Queue", nullptr, &showRenderQueue_);
      ImGui::MenuItem("Particle Editor", nullptr, &showParticleEditor_);
      ImGui::MenuItem("Environment Settings", nullptr, &showEnvironmentWindow_);
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
            auto& sd = e->AddComponent<SkydomeComponent>();
            sd.skydomeHandle = RC::GenerateSkydomeEx(-1);
          }
          if (ImGui::MenuItem("Water")) {
            auto e = currentScene->CreateEntity("Water");
            e->AddComponent<TransformComponent>();
            auto& water = e->AddComponent<WaterComponent>();
            water.meshHandle = RC::GenerateWaterPlane(
                water.planeWidth, water.planeHeight, water.segments);
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
        if (ImGui::MenuItem("Camera")) {
          auto e = currentScene->CreateEntity("Camera");
          e->AddComponent<TransformComponent>();
          e->AddComponent<CameraComponent>();
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

    ImGui::SameLine();

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

void EditorManager::DrawEntityNode(std::shared_ptr<Entity> e, Scene* currentScene, const std::unordered_map<uint64_t, std::vector<std::shared_ptr<Entity>>>& childrenMap) {
#if RC_ENABLE_IMGUI
    if (!e || e->IsPendingDestroy()) return;

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

    // フォルダアイコンの表示（isFolderの場合）
    if (e->IsFolder()) {
        auto srv = RC::GetRenderContext().Textures().GetSrv(folderIconTex_);
        if (srv.ptr) {
            ImGui::Image((ImTextureID)srv.ptr, ImVec2(16, 16));
            ImGui::SameLine(0, 4);
        }
    }

    auto it = childrenMap.find(e->Guid());
    bool hasChildren = (it != childrenMap.end() && !it->second.empty());

    // --- エンティティ名（垂直中央揃え） ---
    ImGui::AlignTextToFramePadding();
    bool isOpen = false;

    if (renamingEntityId_ == e->Id()) {
        char nameBuf[256];
        strncpy_s(nameBuf, sizeof(nameBuf), e->Name().c_str(), _TRUNCATE);
        
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (focusRename_) {
            ImGui::SetKeyboardFocusHere();
            focusRename_ = false;
        }
        if (ImGui::InputText("##rename", nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            e->SetName(nameBuf);
            renamingEntityId_ = 0;
        } else if (ImGui::IsItemDeactivated()) {
            e->SetName(nameBuf);
            renamingEntityId_ = 0;
        }
    } else {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (!hasChildren) {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }
        if (selectedEntity_.lock() == e) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        
        // フォルダの場合はデフォルトで開く
        if (e->IsFolder()) flags |= ImGuiTreeNodeFlags_DefaultOpen;

        isOpen = ImGui::TreeNodeEx("##node", flags, "%s", e->Name().c_str());

        // ドラッグ元
        if (ImGui::BeginDragDropSource()) {
            uint64_t dragGuid = e->Guid();
            ImGui::SetDragDropPayload("HIERARCHY_ENTITY", &dragGuid, sizeof(uint64_t));
            ImGui::Text("Move %s", e->Name().c_str());
            ImGui::EndDragDropSource();
        }

        // ドロップ先 (フォルダの場合のみ)
        if (e->IsFolder()) {
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
                    uint64_t draggedGuid = *(const uint64_t*)payload->Data;
                    auto draggedE = currentScene->FindEntityByGuid(draggedGuid);
                    if (draggedE && draggedE != e) {
                        // 循環参照チェック (自分がドラッグされた要素の子孫でないか)
                        bool isDescendant = false;
                        auto curr = e;
                        while (curr && curr->ParentGuid() != 0) {
                            if (curr->ParentGuid() == draggedGuid) { isDescendant = true; break; }
                            curr = currentScene->FindEntityByGuid(curr->ParentGuid());
                        }
                        if (!isDescendant) {
                            draggedE->SetParentGuid(e->Guid());
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }

        if (ImGui::IsItemClicked(0) && !e->IsLocked()) {
            selectedEntity_ = e;
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && !e->IsLocked()) {
            renamingEntityId_ = e->Id();
            focusRename_ = true;
        }
        
        // 右クリックメニュー (Delete, Rename, Create Folder)
        if (!e->IsLocked()) {
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Rename")) {
                    renamingEntityId_ = e->Id();
                    focusRename_ = true;
                }
                if (e->IsFolder() || true) {
                    if (ImGui::MenuItem("Create Folder inside")) {
                        auto folder = currentScene->CreateEntity("New Folder");
                        folder->SetIsFolder(true);
                        folder->SetParentGuid(e->Guid());
                    }
                }
                if (ImGui::MenuItem("Delete")) {
                    currentScene->DestroyEntityRecursive(e);
                    if (selectedEntity_.lock() == e) {
                        selectedEntity_.reset();
                    }
                }
                ImGui::EndPopup();
            }
        }
    }

    if (isOpen) {
        if (hasChildren) {
            for (auto& child : it->second) {
                DrawEntityNode(child, currentScene, childrenMap);
            }
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
#endif
}

void EditorManager::DrawUI(D3D12_GPU_DESCRIPTOR_HANDLE viewportSrv, Dx12Core* core, PipelineManager* pm, float deltaTime, Scene* currentScene) {
#if RC_ENABLE_IMGUI
  ImGuizmo::BeginFrame();

  if (showDemoWindow_) {
    ImGui::ShowDemoWindow(&showDemoWindow_);
  }

  // Performance パネル
  if (showPerfWindow_) {
    if (ImGui::Begin("Performance", &showPerfWindow_)) {
      ImGuiIO &io = ImGui::GetIO();
      io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

      const float fps = io.Framerate;
      const float frameTime = 1000.0f / (fps > 0.0f ? fps : 1.0f);

      static float fpsDropTimer = 0.0f;
      if (fpsDropTimer > 0.0f) {
          fpsDropTimer -= io.DeltaTime;
      }
      // FPSが30を下回った場合に警告ログを出力 (起動直後などの0FPSは除外)
      if (fps > 0.0f && fps < 30.0f && fpsDropTimer <= 0.0f) {
          Log::Print(std::format("[Performance] Warning: FPS dropped to {:.1f} ({:.2f} ms)", fps, frameTime));
          fpsDropTimer = 5.0f; // 連続出力を防ぐためのクールダウン（5秒）
      }

      // フレームタイム・FPSの履歴バッファ
      static float fpsHistory[120] = {0};
      static float msHistory[120] = {0};
      static int historyIdx = 0;
      
      fpsHistory[historyIdx] = fps;
      msHistory[historyIdx] = frameTime;
      historyIdx = (historyIdx + 1) % 120;

      if (ImGui::CollapsingHeader("Timing & Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
          ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "FPS: %.1f", fps);
          ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Frame Time: %.3f ms", frameTime);
          
          ImGui::Separator();
          
          char overlayFps[32];
          sprintf_s(overlayFps, "Avg FPS: %.1f", fps);
          ImGui::PlotLines("##FPS", fpsHistory, 120, historyIdx, overlayFps, 0.0f, 120.0f, ImVec2(ImGui::GetContentRegionAvail().x, 60.0f));

          char overlayMs[32];
          sprintf_s(overlayMs, "Avg %.2f ms", frameTime);
          ImGui::PlotLines("##MS", msHistory, 120, historyIdx, overlayMs, 0.0f, 33.0f, ImVec2(ImGui::GetContentRegionAvail().x, 60.0f));
      }
      
      if (core) {
          if (ImGui::CollapsingHeader("Graphics Info", ImGuiTreeNodeFlags_DefaultOpen)) {
              ImGui::Text("Viewport: %.0f x %.0f", core->Viewport().Width, core->Viewport().Height);
              ImGui::Text("Frame Buffer Count: %u", core->FrameCount());
              ImGui::Text("Completed Fence: %llu", core->GetCompletedFenceValue());
          }
      }

      if (ImGui::CollapsingHeader("Editor Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
          if (core) {
              float currentFps = core->GetTargetFps();
              bool isFixed = currentFps > 0.0f;
              
              const char* fpsOptions[] = { "30 FPS", "60 FPS", "120 FPS", "144 FPS", "Uncapped" };
              float fpsValues[] = { 30.0f, 60.0f, 120.0f, 144.0f, 0.0f };
              
              int currentItem = 1; // default to 60 FPS
              if (!isFixed) {
                  currentItem = 4;
              } else {
                  for (int i = 0; i < 4; ++i) {
                      if (std::abs(fpsValues[i] - currentFps) < 0.1f) {
                          currentItem = i;
                          break;
                      }
                  }
              }

              if (ImGui::Combo("Target FPS", &currentItem, fpsOptions, 5)) {
                  float newFps = fpsValues[currentItem];
                  core->SetTargetFps(newFps);
                  core->EnableFixFps(newFps > 0.0f);
              }

              ImGui::Separator();
              
              // Resolution Settings
#if defined(_DEBUG) || defined(RC_DEVELOPMENT)
              const char* resOptions[] = {
                  "Borderless Fullscreen",
                  "1920 x 1080 (Borderless)",
                  "1600 x 900 (Borderless)",
                  "1280 x 720 (Borderless)"
              };
#else
              const char* resOptions[] = {
                  "Borderless Fullscreen",
                  "1920 x 1080 (Windowed)",
                  "1600 x 900 (Windowed)",
                  "1280 x 720 (Windowed)"
              };
#endif
              
              int currentResItem = 0;
              float w = core->Viewport().Width;
              float h = core->Viewport().Height;
              
              int screenW = GetSystemMetrics(SM_CXSCREEN);
              int screenH = GetSystemMetrics(SM_CYSCREEN);
              
              if (w == screenW && h == screenH) currentResItem = 0;
              else if (w == 1920 && h == 1080) currentResItem = 1;
              else if (w == 1600 && h == 900) currentResItem = 2;
              else if (w == 1280 && h == 720) currentResItem = 3;
              else currentResItem = 0; // fallback

              if (ImGui::Combo("Window Resolution", &currentResItem, resOptions, 4)) {
                  resizeRequest_.pending = true;
                  if (currentResItem == 0) {
                      resizeRequest_.width = screenW;
                      resizeRequest_.height = screenH;
                      resizeRequest_.fullscreen = true;
                  } else if (currentResItem == 1) {
                      resizeRequest_.width = 1920;
                      resizeRequest_.height = 1080;
                      resizeRequest_.fullscreen = false;
                  } else if (currentResItem == 2) {
                      resizeRequest_.width = 1600;
                      resizeRequest_.height = 900;
                      resizeRequest_.fullscreen = false;
                  } else if (currentResItem == 3) {
                      resizeRequest_.width = 1280;
                      resizeRequest_.height = 720;
                      resizeRequest_.fullscreen = false;
                  }
              }
              ImGui::Separator();
          }

          if (ImGui::Button("Save Window Layout")) { SaveConfig(); }
          ImGui::SameLine();
          if (ImGui::Button("Load Window Layout")) { LoadConfig(); }
      }
    }
    ImGui::End();
  }

  // Render Queue パネル
  if (showRenderQueue_) {
    if (ImGui::Begin("Render Queue", &showRenderQueue_)) {
      const auto& queue = RC::GetRenderContext().GetLastCommandHistory();
      ImGui::Text("Total Commands: %zu", queue.size());
      
      ImGui::SameLine();
      if (ImGui::Button("Export Dump to File")) {
        ExportRenderQueueDump();
      }

      ImGui::Separator();
      if (ImGui::CollapsingHeader("Command Execution Order (3D + 2D)", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("RenderQueueTable", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
          ImGui::TableSetupColumn("Order", ImGuiTableColumnFlags_WidthFixed, 40.0f);
          ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
          ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 50.0f);
          ImGui::TableSetupColumn("SortKey", ImGuiTableColumnFlags_WidthFixed, 150.0f);
          ImGui::TableHeadersRow();

          for (size_t i = 0; i < queue.size(); ++i) {
            const auto& cmd = queue[i];
            
            std::string displayName = cmd.debugName;
            if (cmd.debugIndex >= 0) {
              std::string resourceName = "";
              if (cmd.debugName.find("Model") != std::string::npos) {
                if (auto* m = RC::GetRenderContext().Models().Get(cmd.debugIndex)) {
                  resourceName = std::filesystem::path(m->GetFilePath()).filename().string();
                }
              } else if (cmd.debugName.find("Sprite") != std::string::npos) {
                if (auto* s = RC::GetRenderContext().Sprites().Get(cmd.debugIndex)) {
                  resourceName = std::filesystem::path(s->GetFilePath()).filename().string();
                }
              }
              
              if (!resourceName.empty()) {
                displayName += " [" + resourceName + "]";
              }
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("[%zu]", i);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", displayName.c_str());
            ImGui::TableSetColumnIndex(2);
            if (cmd.debugIndex >= 0) {
              ImGui::Text("%d", cmd.debugIndex);
            } else {
              ImGui::Text("-");
            }
            ImGui::TableSetColumnIndex(3);
            if (cmd.sortKey != 0) {
              ImGui::Text("%016llX", cmd.sortKey);
            } else {
              ImGui::Text("-");
            }
            ImGui::TableSetColumnIndex(4);
            if (cmd.sortKey != 0) {
              uint8_t layer = static_cast<uint8_t>(cmd.sortKey >> 56);
              std::string layerStr = (layer == 0) ? "Opaque" : (layer == 1) ? "Alpha" : (layer == 2) ? "Glass" : (layer == 3) ? "Overlay" : "?";
              ImGui::Text("%d(%s)", layer, layerStr.c_str());
            } else { ImGui::Text("-"); }
            ImGui::TableSetColumnIndex(5);
            if (cmd.sortKey != 0) {
              uint32_t depth24 = static_cast<uint32_t>(cmd.sortKey & 0x00FFFFFF);
              ImGui::Text("%u", depth24);
            } else { ImGui::Text("-"); }
            ImGui::TableSetColumnIndex(6);
            if (cmd.sortKey != 0) {
              uint16_t psoHash = static_cast<uint16_t>((cmd.sortKey >> 40) & 0xFFFF);
              ImGui::Text("%04X", psoHash);
            } else { ImGui::Text("-"); }
            ImGui::TableSetColumnIndex(7);
            if (cmd.sortKey != 0) {
              uint16_t texHash = static_cast<uint16_t>((cmd.sortKey >> 24) & 0xFFFF);
              ImGui::Text("%04X", texHash);
            } else { ImGui::Text("-"); }
          }
          ImGui::EndTable();
        }
      }
    }
    ImGui::End();
  }

  // Environment Settings パネル
  if (showEnvironmentWindow_) {
    if (ImGui::Begin("Environment Settings (環境設定)", &showEnvironmentWindow_)) {
      if (!currentScene) {
        ImGui::Text("No active scene.");
      } else {
        std::shared_ptr<Entity> skyEntity = nullptr;
        for (auto& e : currentScene->GetEntities()) {
          if (e->HasComponent<SkyboxComponent>() || e->HasComponent<SkydomeComponent>()) {
            skyEntity = e;
            break;
          }
        }
        
        if (skyEntity) {
          ImGui::Text("Current Environment Entity: %s", skyEntity->GetName().c_str());
          if (ImGui::Button("Select in Hierarchy")) {
            selectedEntity_ = skyEntity;
          }
          ImGui::Separator();
          ImGui::TextDisabled("Select the entity to edit details in the Inspector.");
        } else {
          ImGui::Text("No Skybox or Skydome in the scene.");
          ImGui::Separator();
          if (ImGui::Button("Create Skydome")) {
            auto e = currentScene->CreateEntity("Environment (Skydome)");
            e->AddComponent<TransformComponent>();
            auto& sd = e->AddComponent<SkydomeComponent>();
            sd.skydomeHandle = RC::GenerateSkydomeEx(-1);
            selectedEntity_ = e;
          }
          if (ImGui::Button("Create Skybox")) {
            auto e = currentScene->CreateEntity("Environment (Skybox)");
            e->AddComponent<TransformComponent>();
            e->AddComponent<SkyboxComponent>();
            selectedEntity_ = e;
          }
        }
      }
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
      bool isHoveringImage = ImGui::IsItemHovered();
      
      // ===== Mouse Position Update for Game =====
      float currentMouseX = ImGui::GetMousePos().x - vMin.x;
      float currentMouseY = ImGui::GetMousePos().y - vMin.y;
      if (core) {
          float gameW = core->Viewport().Width;
          float gameH = core->Viewport().Height;
          float scaledX = (currentMouseX / width) * gameW;
          float scaledY = (currentMouseY / height) * gameH;
          if (auto input = Input::GetInstance()) {
              input->SetGameMousePosition(scaledX, scaledY);
          }
      }

      // ===== Mouse Picking =====
      if (ImGui::IsMouseClicked(0) && isHoveringImage && !ImGui::IsMouseDragging(0) && currentScene && 
          (!currentScene->GetContext() || !currentScene->GetContext()->isPlaying()) &&
          !ImGuizmo::IsOver()) {
          float mouseX = ImGui::GetMousePos().x - vMin.x;
          float mouseY = ImGui::GetMousePos().y - vMin.y;
          RC::CameraController* cam = RC::GetRenderContext().Ctx()->camera;
          if (cam) {
              RC::Vector2 mousePosVec = { mouseX, mouseY };
              RC::Vector2 screenSize = { width, height };
              RC::Matrix4x4 view = cam->GetView();
              RC::Matrix4x4 proj = cam->GetProjection();
              RC::Ray ray = RC::CameraMath::ScreenPointToRay(mousePosVec, screenSize, view, proj);
              
              float minHitDistance = 999999.0f;
              std::shared_ptr<Entity> hitEntity = nullptr;
              
              for (const auto& e : currentScene->GetEntities()) {
                  if (!e->IsVisible()) continue;
                  auto* tr = e->GetComponent<TransformComponent>();
                  if (!tr) continue;
                  
                  float dist = 0.0f;
                  bool hit = false;
                  
                  if (auto* col = e->GetComponent<ColliderComponent>()) {
                      if (col->IsEnabled()) {
                          RC::Vector3 scaledCenter = {
                              col->center.x * tr->scale.x,
                              col->center.y * tr->scale.y,
                              col->center.z * tr->scale.z
                          };
                          RC::Vector3 worldCenter = {
                              tr->position.x + scaledCenter.x,
                              tr->position.y + scaledCenter.y,
                              tr->position.z + scaledCenter.z
                          };
                          if (col->shape == ColliderComponent::Shape::Sphere) {
                              float maxScale = (std::max)((std::max)(std::abs(tr->scale.x), std::abs(tr->scale.y)), std::abs(tr->scale.z));
                              hit = RC::IntersectRaySphere(ray, worldCenter, col->radius * maxScale, dist);
                          } else if (col->shape == ColliderComponent::Shape::AABB) {
                              RC::Vector3 scaledSize = {
                                  std::abs(col->size.x * tr->scale.x),
                                  std::abs(col->size.y * tr->scale.y),
                                  std::abs(col->size.z * tr->scale.z)
                              };
                              RC::Vector3 halfSize = { scaledSize.x * 0.5f, scaledSize.y * 0.5f, scaledSize.z * 0.5f };
                              hit = RC::IntersectRayAABB(ray, ::Subtract(worldCenter, halfSize), ::Add(worldCenter, halfSize), dist);
                          }
                      }
                  } else if (auto* mr = e->GetComponent<ModelRendererComponent>()) {
                      if (mr->visible && mr->IsEnabled()) {
                          hit = RC::IntersectRaySphere(ray, tr->position, 1.0f, dist);
                      }
                  } else if (auto* pm = e->GetComponent<PrimitiveMeshComponent>()) {
                      if (pm->visible && pm->IsEnabled()) {
                          hit = RC::IntersectRaySphere(ray, tr->position, 1.0f, dist);
                      }
                  }
                  
                  if (hit && dist >= 0.0f && dist < minHitDistance) {
                      minHitDistance = dist;
                      hitEntity = e;
                  }
              }
              
              if (hitEntity && !hitEntity->IsLocked()) {
                  selectedEntity_ = hitEntity;
              } else if (!hitEntity) {
                  selectedEntity_.reset();
              }
          }
      }
      
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
                      ren.modelPath = droppedPath;
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
      
      // 再生中以外の場合のみ、ギズモ描画と各種オーバーレイUIを表示する
      if (playState_ != PlayState::Playing) {
          // ボタン群（高さ24px）の背景として、上下左右に余白を持たせた半透明の枠を描画
          // cursorPos は最初のボタンの左上絶対座標。枠は少し左・上にずらして描画する。
          ImGui::GetWindowDrawList()->AddRectFilled(
              ImVec2(cursorPos.x - 6.0f, cursorPos.y - 4.0f), 
              ImVec2(cursorPos.x - 6.0f + overlaySize.x, cursorPos.y - 4.0f + overlaySize.y), 
              IM_COL32(20, 20, 20, 160), 
              6.0f
          );

          // シェーディングモード切替ボタンの描画を実行
          RC::DrawViewShadingModeImGui("");

          // ギズモ操作モード用UIを左上に描画
          ImGui::SetCursorPos(ImVec2(10.0f, 24.0f));
          ImVec2 leftCursorPos = ImGui::GetCursorScreenPos();
          ImVec2 leftOverlaySize = ImVec2(156.0f, 32.0f); // 枠の幅を少し広げてはみ出しを修正
          ImGui::GetWindowDrawList()->AddRectFilled(
              ImVec2(leftCursorPos.x - 6.0f, leftCursorPos.y - 4.0f), 
              ImVec2(leftCursorPos.x - 6.0f + leftOverlaySize.x, leftCursorPos.y - 4.0f + leftOverlaySize.y), 
              IM_COL32(20, 20, 20, 160), 
              6.0f
          );

          ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 0.0f));
          if (ImGui::Button("T", ImVec2(24, 24))) gizmoOperation_ = 7; // TRANSLATE
          if (ImGui::IsItemHovered()) ImGui::SetTooltip("Translate");
          ImGui::SameLine();
          if (ImGui::Button("R", ImVec2(24, 24))) gizmoOperation_ = 120; // ROTATE
          if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rotate");
          ImGui::SameLine();
          if (ImGui::Button("S", ImVec2(24, 24))) gizmoOperation_ = 896; // SCALE
          if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scale");
          ImGui::SameLine();
          ImGui::Text("|");
          ImGui::SameLine();
          if (ImGui::Button("L", ImVec2(24, 24))) gizmoMode_ = 0; // LOCAL
          if (ImGui::IsItemHovered()) ImGui::SetTooltip("Local");
          ImGui::SameLine();
          if (ImGui::Button("W", ImVec2(24, 24))) gizmoMode_ = 1; // WORLD
          if (ImGui::IsItemHovered()) ImGui::SetTooltip("World");
          ImGui::PopStyleVar();

          // ImGuizmo のセットアップ
          ImGuizmo::SetDrawlist();
          ImGuizmo::SetRect(vMin.x, vMin.y, width, height);

          // 選択されているオブジェクトがあればギズモを表示
          if (auto hitEntity = selectedEntity_.lock()) {
              if (auto* tr = hitEntity->GetComponent<TransformComponent>()) {
                  RC::CameraController* cam = RC::GetRenderContext().Ctx()->camera;
                  if (cam) {
                      RC::Matrix4x4 view = cam->GetView();
                      RC::Matrix4x4 proj = cam->GetProjection();
                      
                      float* viewPtr = reinterpret_cast<float*>(&view);
                      float* projPtr = reinterpret_cast<float*>(&proj);
                      
                      RC::Matrix4x4 worldMat = MakeAffineMatrix(tr->scale, tr->rotation, tr->position);
                      float* matrixPtr = reinterpret_cast<float*>(&worldMat);
                      
                      ImGuizmo::Manipulate(viewPtr, projPtr, (ImGuizmo::OPERATION)gizmoOperation_, (ImGuizmo::MODE)gizmoMode_, matrixPtr);
                      
                      if (ImGuizmo::IsUsing()) {
                          float translation[3], rotation[3], scale[3];
                          ImGuizmo::DecomposeMatrixToComponents(matrixPtr, translation, rotation, scale);
                          
                          tr->position = {translation[0], translation[1], translation[2]};
                          tr->rotation = {rotation[0] * 3.14159265f / 180.0f, rotation[1] * 3.14159265f / 180.0f, rotation[2] * 3.14159265f / 180.0f};
                          tr->scale = {scale[0], scale[1], scale[2]};
                      }
                  }
              }
          }
      }
    } else {
      ImGui::Text("No Viewport Texture");
    }
  }
  ImGui::End();
  ImGui::PopStyleVar();

  // Hierarchy パネル
  if (ImGui::Begin("Hierarchy")) {
    if (currentScene) {
      // 親子関係のマップを構築
      std::unordered_map<uint64_t, std::vector<std::shared_ptr<Entity>>> childrenMap;
      std::vector<std::shared_ptr<Entity>> rootEntities;
      
      for (const auto& e : currentScene->GetEntities()) {
        if (!e || e->IsPendingDestroy()) continue;
        if (e->ParentGuid() == 0 || currentScene->FindEntityByGuid(e->ParentGuid()) == nullptr) {
            rootEntities.push_back(e);
        } else {
            childrenMap[e->ParentGuid()].push_back(e);
        }
      }

      ImGui::BeginChild("HierarchyList", ImVec2(0, 0), false);
      for (const auto& e : rootEntities) {
          DrawEntityNode(e, currentScene, childrenMap);
      }

      // 何もない領域での右クリックメニュー（新規フォルダ作成など）
      if (ImGui::BeginPopupContextWindow("HierarchyBgContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
          if (ImGui::MenuItem("Create Folder")) {
              auto folder = currentScene->CreateEntity("New Folder");
              folder->SetIsFolder(true);
              folder->SetParentGuid(0);
          }
          ImGui::EndPopup();
      }

      // ルート領域へのドロップ対応（ルート階層に移動）
      ImGui::Dummy(ImGui::GetContentRegionAvail());
      if (ImGui::BeginDragDropTarget()) {
          if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
              uint64_t draggedGuid = *(const uint64_t*)payload->Data;
              auto draggedE = currentScene->FindEntityByGuid(draggedGuid);
              if (draggedE) {
                  draggedE->SetParentGuid(0); // ルートに移動
              }
          }
          ImGui::EndDragDropTarget();
      }
      ImGui::EndChild();
    }
  }
  ImGui::End();

  // Inspector パネル
  if (ImGui::Begin("Inspector")) {
    if (auto e = selectedEntity_.lock()) {
        std::function<void()> pendingRemove;
        char nameBuf[256];
        strncpy_s(nameBuf, sizeof(nameBuf), e->Name().c_str(), _TRUNCATE);
        ImGui::Text("Entity:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::InputText("##InspectorRename", nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            e->SetName(nameBuf);
        } else if (ImGui::IsItemDeactivatedAfterEdit()) {
            e->SetName(nameBuf);
        }
        ImGui::Separator();

        if (ImGui::CollapsingHeader("Tags (タグ)", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent(8.0f);
            auto& tags = e->GetTagsRef();
            
            std::string tagToRemove = "";
            for (auto& [key, val] : tags) {
                ImGui::PushID(key.c_str());
                
                // タグ名と削除ボタンのみを表示 (値は隠蔽してシンプルにする)
                ImGui::AlignTextToFramePadding();
                ImGui::Text(" %s ", key.c_str());
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 22.0f);
                if (ImGui::Button("X", ImVec2(22, 0))) {
                    tagToRemove = key;
                }
                ImGui::PopID();
            }
            if (!tagToRemove.empty()) {
                tags.erase(tagToRemove);
            }

            ImGui::Separator();
            
            // Collect known tags
            std::set<std::string> knownTags = { "is_enemy", "is_player", "is_terrain", "pending_damage", "impact_factor", "reused", "Shark", "Enemy" };
            if (currentScene) {
                for (const auto& sceneEntity : currentScene->GetEntities()) {
                    if (!sceneEntity) continue;
                    for (const auto& [k, v] : sceneEntity->GetTags()) {
                        knownTags.insert(k);
                    }
                }
            }
            
            std::vector<std::string> tagList;
            tagList.push_back("--- Select a tag ---");
            for (const auto& k : knownTags) {
                tagList.push_back(k);
            }
            tagList.push_back("+ New Tag...");

            static int selectedTagIdx = 0;
            static char newTagKey[64] = "";

            if (selectedTagIdx >= tagList.size()) selectedTagIdx = 0;

            const char* currentLabel = tagList[selectedTagIdx].c_str();
            
            bool isNewTagMode = (selectedTagIdx == tagList.size() - 1);
            float comboWidth = isNewTagMode ? ImGui::GetContentRegionAvail().x * 0.45f : ImGui::GetContentRegionAvail().x - 50.0f;
            
            ImGui::SetNextItemWidth(comboWidth);
            if (ImGui::BeginCombo("##TagSelector", currentLabel)) {
                for (int i = 0; i < tagList.size(); ++i) {
                    bool isSelected = (selectedTagIdx == i);
                    if (ImGui::Selectable(tagList[i].c_str(), isSelected)) {
                        selectedTagIdx = i;
                        if (i > 0 && i < tagList.size() - 1) {
                            strncpy_s(newTagKey, sizeof(newTagKey), tagList[i].c_str(), _TRUNCATE);
                        }
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine();
            
            if (isNewTagMode) {
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 50.0f);
                ImGui::InputText("##NewTagKey", newTagKey, sizeof(newTagKey));
                ImGui::SameLine();
            }
            
            if (ImGui::Button("Add", ImVec2(40.0f, 0.0f))) {
                std::string tagToAdd = isNewTagMode ? newTagKey : (selectedTagIdx > 0 ? tagList[selectedTagIdx] : "");
                if (!tagToAdd.empty() && tagToAdd != "--- Select a tag ---" && tagToAdd != "+ New Tag...") {
                    tags[tagToAdd] = 1; // 値は1固定
                    newTagKey[0] = '\0';
                    selectedTagIdx = 0;
                }
            }
            ImGui::Unindent(8.0f);
        }

        if (auto* tr = e->GetComponent<TransformComponent>()) {
            if (ImGui::CollapsingHeader("Transform (変形)", ImGuiTreeNodeFlags_DefaultOpen)) {
               ImGui::Indent(8.0f);
               bool enabled = tr->IsEnabled();
               if (ImGui::Checkbox("Enabled (有効化)##TR", &enabled)) tr->SetEnabled(enabled);
               ImGui::SameLine(ImGui::GetContentRegionAvail().x - 40.0f);
               if (ImGui::Button("Reset (リセット)##TR")) {
                   tr->position = {0.0f, 0.0f, 0.0f};
                   tr->rotation = {0.0f, 0.0f, 0.0f};
                   tr->scale = {1.0f, 1.0f, 1.0f};
               }
               ImGui::DragFloat3("Position (位置)", &tr->position.x, 0.1f);
               
               // Euler 変換 (deg <-> rad)
               RC::Vector3 eulerDegrees = { tr->rotation.x * 180.0f / 3.14159265f, tr->rotation.y * 180.0f / 3.14159265f, tr->rotation.z * 180.0f / 3.14159265f };
               if (ImGui::DragFloat3("Rotation (回転)", &eulerDegrees.x, 1.0f)) {
                   tr->rotation = { eulerDegrees.x * 3.14159265f / 180.0f, eulerDegrees.y * 3.14159265f / 180.0f, eulerDegrees.z * 3.14159265f / 180.0f };
               }
               ImGui::DragFloat3("Scale (スケール)", &tr->scale.x, 0.1f);
               ImGui::Unindent(8.0f);
            }
        }

        if (auto* ren = e->GetComponent<ModelRendererComponent>()) {
            bool headerOpen = ImGui::CollapsingHeader("Model Renderer (モデル描画)", ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) pendingRemove = [e](){ e->RemoveComponent<ModelRendererComponent>(); };
                ImGui::EndPopup();
            }
            if (headerOpen) {
               ImGui::Indent(8.0f);
               bool enabled = ren->IsEnabled();
               if (ImGui::Checkbox("Enabled (有効化)##Model", &enabled)) ren->SetEnabled(enabled);
               ImGui::Checkbox("Visible (表示)##Model", &ren->visible);
               ImGui::Text("Model Handle: %d", ren->modelHandle);
               ImGui::Unindent(8.0f);
            }
            // ── Material セクション ──
            if (ren->HasModel() && ImGui::CollapsingHeader("Material (マテリアル)##Model", ImGuiTreeNodeFlags_DefaultOpen)) {
               ImGui::Indent(8.0f);

               // -- Base Color --
               ImGui::ColorEdit4("Base Color (基本色)##Model", &ren->color.x);

               // -- Texture --
               ImGui::Text("Texture (テクスチャ)");
               ImGui::SameLine();
               std::string texLabelStr = ren->texturePath.empty() ? "(None)##TexM" : std::filesystem::path(ren->texturePath).filename().string() + "##TexM";
               ImGui::Button(texLabelStr.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 60.0f, 0));
               if (ImGui::BeginDragDropTarget()) {
                   if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                       std::string droppedPath((const char*)payload->Data);
                       std::filesystem::path p(droppedPath);
                       std::string ext = p.extension().string();
                       std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                       if (ext == ".png" || ext == ".jpg" || ext == ".dds") {
                           ren->texturePath = droppedPath;
                           ren->texOverride = RC::LoadTex(droppedPath);
                       }
                   }
                   ImGui::EndDragDropTarget();
               }
               if (!ren->texturePath.empty()) {
                   ImGui::SameLine();
                   if (ImGui::Button("X##TexModel", ImVec2(22, 0))) {
                       ren->texturePath.clear();
                       ren->texOverride = -1;
                   }
               }

               // -- Normal Map --
               ImGui::Text("Normal Map (法線マップ)");
               ImGui::SameLine();
               std::string normalLabelStr = ren->normalMapPath.empty() ? "(None)##NmapM" : std::filesystem::path(ren->normalMapPath).filename().string() + "##NmapM";
               ImGui::Button(normalLabelStr.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 60.0f, 0));
               if (ImGui::BeginDragDropTarget()) {
                   if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                       std::string droppedPath((const char*)payload->Data);
                       std::filesystem::path p(droppedPath);
                       std::string ext = p.extension().string();
                       std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                       if (ext == ".png" || ext == ".jpg" || ext == ".dds") {
                           ren->normalMapPath = droppedPath;
                           ren->normalMapOverride = RC::LoadTex(droppedPath);
                           RC::SetModelNormalMap(ren->modelHandle, ren->normalMapOverride);
                       }
                   }
                   ImGui::EndDragDropTarget();
               }
               if (!ren->normalMapPath.empty()) {
                   ImGui::SameLine();
                   if (ImGui::Button("X##NmapModel", ImVec2(22, 0))) {
                       ren->normalMapPath.clear();
                       ren->normalMapOverride = -1;
                       RC::SetModelNormalMap(ren->modelHandle, -1);
                   }
               }

               // -- Roughness Map --
               ImGui::Text("Roughness Map (粗さマップ)");
               ImGui::SameLine();
               std::string roughnessLabelStr = ren->roughnessMapPath.empty() ? "(None)##RmapM" : std::filesystem::path(ren->roughnessMapPath).filename().string() + "##RmapM";
               ImGui::Button(roughnessLabelStr.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 60.0f, 0));
               if (ImGui::BeginDragDropTarget()) {
                   if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                       std::string droppedPath((const char*)payload->Data);
                       std::filesystem::path p(droppedPath);
                       std::string ext = p.extension().string();
                       std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                       if (ext == ".png" || ext == ".jpg" || ext == ".dds") {
                           ren->roughnessMapPath = droppedPath;
                           ren->roughnessMapOverride = RC::LoadTex(droppedPath);
                           RC::SetModelRoughnessMap(ren->modelHandle, ren->roughnessMapOverride);
                       }
                   }
                   ImGui::EndDragDropTarget();
               }
               if (!ren->roughnessMapPath.empty()) {
                   ImGui::SameLine();
                   if (ImGui::Button("X##RmapModel", ImVec2(22, 0))) {
                       ren->roughnessMapPath.clear();
                       ren->roughnessMapOverride = -1;
                       RC::SetModelRoughnessMap(ren->modelHandle, -1);
                   }
               }

               ImGui::Separator();

               // -- GPU Material properties --
               if (Material* mat = RC::GetModelMaterialPtr(ren->modelHandle)) {
                   // Lighting Mode
                   const char* lightingModes[] = { "None", "Lambert", "Half Lambert" };
                   int lightMode = mat->lightingMode;
                   if (lightMode < 0) lightMode = 0;
                   if (lightMode > 2) lightMode = 2;
                   if (ImGui::Combo("Lighting (ライティング)##Model", &lightMode, lightingModes, 3)) {
                       mat->lightingMode = lightMode;
                   }

                   // Shininess
                   ImGui::DragFloat("Shininess (光沢)##Model", &mat->shininess, 1.0f, 0.0f, 512.0f);

                   // Environment Reflection
                   if (ImGui::DragFloat("Env Reflection (環境反射)##Model", &mat->environmentCoefficient, 0.01f, 0.0f, 1.0f)) {
                       ren->environmentCoeff = mat->environmentCoefficient;
                   }

                   ImGui::Separator();

                   // -- UV Transform (Tiling & Offset) --
                   ImGui::Text("UV Transform (UV変換)");
                   // UV Tiling (scale)
                   float tilingX = mat->uvTransform.m[0][0];
                   float tilingY = mat->uvTransform.m[1][1];
                   float tiling[2] = { tilingX, tilingY };
                   if (ImGui::DragFloat2("Tiling (タイリング)##Model", tiling, 0.01f)) {
                       mat->uvTransform.m[0][0] = tiling[0];
                       mat->uvTransform.m[1][1] = tiling[1];
                   }
                   // UV Offset (translation)
                   float offsetX = mat->uvTransform.m[3][0];
                   float offsetY = mat->uvTransform.m[3][1];
                   float offset[2] = { offsetX, offsetY };
                   if (ImGui::DragFloat2("Offset (オフセット)##Model", offset, 0.01f)) {
                       mat->uvTransform.m[3][0] = offset[0];
                       mat->uvTransform.m[3][1] = offset[1];
                   }
               }

               ImGui::Unindent(8.0f);
            }
        }

        if (auto* pm = e->GetComponent<PrimitiveMeshComponent>()) {
            bool headerOpen = ImGui::CollapsingHeader("Primitive Mesh (基本図形)", ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) pendingRemove = [e](){ e->RemoveComponent<PrimitiveMeshComponent>(); };
                ImGui::EndPopup();
            }
            if (headerOpen) {
               ImGui::Indent(8.0f);
               bool enabled = pm->IsEnabled();
               if (ImGui::Checkbox("Enabled (有効化)##PM", &enabled)) pm->SetEnabled(enabled);
               ImGui::Checkbox("Visible (表示)##PM", &pm->visible);
               static const char* typeNames[] = {
                   "Sphere", "Box", "Plane", "Cylinder", "Cone", "Torus", "Capsule"
               };
               int typeIdx = static_cast<int>(pm->type);
               ImGui::Text("Type (種類): %s", (typeIdx >= 0 && typeIdx < 7) ? typeNames[typeIdx] : "Unknown");
               ImGui::Text("Mesh Handle: %d", pm->meshHandle);
               ImGui::Unindent(8.0f);
            }
            // ── Material セクション ──
            if (pm->HasMesh() && ImGui::CollapsingHeader("Material (マテリアル)##PM", ImGuiTreeNodeFlags_DefaultOpen)) {
               ImGui::Indent(8.0f);

               // -- Texture --
               ImGui::Text("Texture (テクスチャ)");
               ImGui::SameLine();
               std::string texLabelStrPM = pm->texturePath.empty() ? "(None)##TexPM" : std::filesystem::path(pm->texturePath).filename().string() + "##TexPM";
               ImGui::Button(texLabelStrPM.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 60.0f, 0));
               if (ImGui::BeginDragDropTarget()) {
                   if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                       std::string droppedPath((const char*)payload->Data);
                       std::filesystem::path p(droppedPath);
                       std::string ext = p.extension().string();
                       std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                       if (ext == ".png" || ext == ".jpg" || ext == ".dds") {
                           pm->texturePath = droppedPath;
                           pm->texOverride = RC::LoadTex(droppedPath);
                       }
                   }
                   ImGui::EndDragDropTarget();
               }
               if (!pm->texturePath.empty()) {
                   ImGui::SameLine();
                   if (ImGui::Button("X##TexPM", ImVec2(22, 0))) {
                       pm->texturePath.clear();
                       pm->texOverride = -1;
                   }
               }

               // -- Normal Map --
               ImGui::Text("Normal Map (法線マップ)");
               ImGui::SameLine();
               std::string normalLabelStrPM = pm->normalMapPath.empty() ? "(None)##NmapPM" : std::filesystem::path(pm->normalMapPath).filename().string() + "##NmapPM";
               ImGui::Button(normalLabelStrPM.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 60.0f, 0));
               if (ImGui::BeginDragDropTarget()) {
                   if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                       std::string droppedPath((const char*)payload->Data);
                       std::filesystem::path p(droppedPath);
                       std::string ext = p.extension().string();
                       std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                       if (ext == ".png" || ext == ".jpg" || ext == ".dds") {
                           pm->normalMapPath = droppedPath;
                           pm->normalMapOverride = RC::LoadTex(droppedPath);
                           RC::SetPrimitiveMeshNormalMap(pm->meshHandle, pm->normalMapOverride);
                       }
                   }
                   ImGui::EndDragDropTarget();
               }
               if (!pm->normalMapPath.empty()) {
                   ImGui::SameLine();
                   if (ImGui::Button("X##NmapPM", ImVec2(22, 0))) {
                       pm->normalMapPath.clear();
                       pm->normalMapOverride = -1;
                       RC::SetPrimitiveMeshNormalMap(pm->meshHandle, -1);
                   }
               }

               // -- Roughness Map --
               ImGui::Text("Roughness Map (粗さマップ)");
               ImGui::SameLine();
               std::string roughnessLabelStrPM = pm->roughnessMapPath.empty() ? "(None)##RmapPM" : std::filesystem::path(pm->roughnessMapPath).filename().string() + "##RmapPM";
               ImGui::Button(roughnessLabelStrPM.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 60.0f, 0));
               if (ImGui::BeginDragDropTarget()) {
                   if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                       std::string droppedPath((const char*)payload->Data);
                       std::filesystem::path p(droppedPath);
                       std::string ext = p.extension().string();
                       std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                       if (ext == ".png" || ext == ".jpg" || ext == ".dds") {
                           pm->roughnessMapPath = droppedPath;
                           pm->roughnessMapOverride = RC::LoadTex(droppedPath);
                           RC::SetPrimitiveMeshRoughnessMap(pm->meshHandle, pm->roughnessMapOverride);
                       }
                   }
                   ImGui::EndDragDropTarget();
               }
               if (!pm->roughnessMapPath.empty()) {
                   ImGui::SameLine();
                   if (ImGui::Button("X##RmapPM", ImVec2(22, 0))) {
                       pm->roughnessMapPath.clear();
                       pm->roughnessMapOverride = -1;
                       RC::SetPrimitiveMeshRoughnessMap(pm->meshHandle, -1);
                   }
               }

               ImGui::Separator();

               // -- GPU Material properties --
               if (Material* mat = RC::GetPrimitiveMeshMaterialPtr(pm->meshHandle)) {
                   // Base Color
                   ImGui::ColorEdit4("Base Color (基本色)##PM", &mat->color.x);

                   // Lighting Mode
                   const char* lightingModes[] = { "None", "Lambert", "Half Lambert" };
                   int lightMode = mat->lightingMode;
                   if (lightMode < 0) lightMode = 0;
                   if (lightMode > 2) lightMode = 2;
                   if (ImGui::Combo("Lighting (ライティング)##PM", &lightMode, lightingModes, 3)) {
                       mat->lightingMode = lightMode;
                   }

                   // Shininess
                   ImGui::DragFloat("Shininess (光沢)##PM", &mat->shininess, 1.0f, 0.0f, 512.0f);

                   // Environment Reflection
                   if (ImGui::DragFloat("Env Reflection (環境反射)##PM", &mat->environmentCoefficient, 0.01f, 0.0f, 1.0f)) {
                       pm->environmentCoeff = mat->environmentCoefficient;
                   }

                   ImGui::Separator();

                   // -- UV Transform (Tiling & Offset) --
                   ImGui::Text("UV Transform (UV変換)");
                   float tilingX = mat->uvTransform.m[0][0];
                   float tilingY = mat->uvTransform.m[1][1];
                   float tiling[2] = { tilingX, tilingY };
                   if (ImGui::DragFloat2("Tiling (タイリング)##PM", tiling, 0.01f)) {
                       mat->uvTransform.m[0][0] = tiling[0];
                       mat->uvTransform.m[1][1] = tiling[1];
                   }
                   float offsetX = mat->uvTransform.m[3][0];
                   float offsetY = mat->uvTransform.m[3][1];
                   float offset[2] = { offsetX, offsetY };
                   if (ImGui::DragFloat2("Offset (オフセット)##PM", offset, 0.01f)) {
                       mat->uvTransform.m[3][0] = offset[0];
                       mat->uvTransform.m[3][1] = offset[1];
                   }
               }

               ImGui::Unindent(8.0f);
            }
        }

        if (auto* spr = e->GetComponent<SpriteRendererComponent>()) {
            bool headerOpen = ImGui::CollapsingHeader("Sprite Renderer (スプライト描画)", ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) pendingRemove = [e](){ e->RemoveComponent<SpriteRendererComponent>(); };
                ImGui::EndPopup();
            }
            if (headerOpen) {
               ImGui::Indent(8.0f);
               bool enabled = spr->IsEnabled();
               if (ImGui::Checkbox("Enabled (有効化)##Spr", &enabled)) spr->SetEnabled(enabled);
               ImGui::Checkbox("Visible (表示)##Spr", &spr->visible);
               ImGui::DragFloat2("Size (サイズ)", &spr->size.x, 1.0f, 0.0f, 4096.0f);
               ImGui::ColorEdit4("Color (色)##Spr", &spr->color.x);
               ImGui::Text("Sprite Handle: %d", spr->spriteHandle);
               ImGui::Unindent(8.0f);
            }
        }

        if (auto* anim = e->GetComponent<AnimationComponent>()) {
            bool headerOpen = ImGui::CollapsingHeader("Animation (アニメーション)", ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) pendingRemove = [e](){ e->RemoveComponent<AnimationComponent>(); };
                ImGui::EndPopup();
            }
            if (headerOpen) {
               ImGui::Indent(8.0f);
               bool enabled = anim->IsEnabled();
               if (ImGui::Checkbox("Enabled (有効化)##Anim", &enabled)) anim->SetEnabled(enabled);
               ImGui::Checkbox("Playing (再生中)", &anim->playing);
               ImGui::DragFloat("Speed (再生速度)", &anim->speed, 0.05f, 0.0f, 10.0f);
               // スキンデータがあるモデルのみ Show Skeleton を表示
               if (auto* ren2 = e->GetComponent<ModelRendererComponent>()) {
                   if (ren2->HasModel() && RC::HasModelSkinData(ren2->modelHandle)) {
                       ImGui::Checkbox("Show Skeleton (骨格表示)", &anim->showSkeleton);
                   }
               }
               if (!anim->animationPath.empty()) {
                   ImGui::Text("Anim File: %s", anim->animationPath.c_str());
               } else {
                   ImGui::TextDisabled("Anim: Embedded");
               }
               ImGui::Unindent(8.0f);
            }
        }

        if (auto* skybox = e->GetComponent<SkyboxComponent>()) {
            bool headerOpen = ImGui::CollapsingHeader("Skybox (スカイボックス)", ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) pendingRemove = [e](){ e->RemoveComponent<SkyboxComponent>(); };
                ImGui::EndPopup();
            }
            if (headerOpen) {
               ImGui::Indent(8.0f);
               bool enabled = skybox->IsEnabled();
               if (ImGui::Checkbox("Enabled (有効化)##Skybox", &enabled)) skybox->SetEnabled(enabled);
               ImGui::Checkbox("Visible (表示)##Skybox", &skybox->visible);
               ImGui::Text("Skybox Handle: %d", skybox->skyboxHandle);
               ImGui::Unindent(8.0f);
            }
            // ── Material セクション ──
            if (ImGui::CollapsingHeader("Material (マテリアル)##Skybox", ImGuiTreeNodeFlags_DefaultOpen)) {
               ImGui::Indent(8.0f);

               ImGui::ColorEdit4("Color (色)##Skybox", &skybox->color.x);

               // -- Cubemap Texture --
               ImGui::Text("Cubemap (キューブマップ)");
               ImGui::SameLine();
               std::string texLabelStr = skybox->skyboxPath.empty() ? "(None - Drop .dds)" : std::filesystem::path(skybox->skyboxPath).filename().string() + "##TexSkybox";
               ImGui::Button(texLabelStr.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 60.0f, 0));
               if (ImGui::BeginDragDropTarget()) {
                   if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                       std::string droppedPath((const char*)payload->Data);
                       std::filesystem::path p(droppedPath);
                       std::string ext = p.extension().string();
                       std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                       if (ext == ".dds") {
                           skybox->skyboxPath = droppedPath;
                           if (skybox->skyboxHandle >= 0) RC::UnloadSkyBox(skybox->skyboxHandle);
                           skybox->skyboxHandle = RC::CreateSkyBox(droppedPath);
                       }
                   }
                   ImGui::EndDragDropTarget();
               }
               if (!skybox->skyboxPath.empty()) {
                   ImGui::SameLine();
                   if (ImGui::Button("X##TexSkybox", ImVec2(22, 0))) {
                       skybox->skyboxPath.clear();
                       if (skybox->skyboxHandle >= 0) RC::UnloadSkyBox(skybox->skyboxHandle);
                       skybox->skyboxHandle = -1;
                   }
               }

               ImGui::Unindent(8.0f);
            }
        }

        if (auto* skydome = e->GetComponent<SkydomeComponent>()) {
            bool headerOpen = ImGui::CollapsingHeader("Skydome (スカイドーム)", ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) pendingRemove = [e](){ e->RemoveComponent<SkydomeComponent>(); };
                ImGui::EndPopup();
            }
            if (headerOpen) {
               ImGui::Indent(8.0f);
               bool enabled = skydome->IsEnabled();
               if (ImGui::Checkbox("Enabled (有効化)##Skydome", &enabled)) skydome->SetEnabled(enabled);
               ImGui::Checkbox("Visible (表示)##Skydome", &skydome->visible);
               ImGui::Text("Skydome Handle: %d", skydome->skydomeHandle);
               ImGui::Unindent(8.0f);
            }
            // ── Material セクション ──
            if (ImGui::CollapsingHeader("Material (マテリアル)##Skydome", ImGuiTreeNodeFlags_DefaultOpen)) {
               ImGui::Indent(8.0f);

               if (ImGui::ColorEdit4("Color (色)##Skydome", &skydome->color.x)) {
                   if (skydome->skydomeHandle < 0) {
                       skydome->skydomeHandle = RC::GenerateSkydomeEx(-1);
                   }
                   RC::SetSkydomeColor(skydome->skydomeHandle, skydome->color);
               }

               // -- Texture --
               ImGui::Text("Texture (テクスチャ)");
               ImGui::SameLine();
               std::string texLabelStr = skydome->texturePath.empty() ? "(None)##TexSkydome" : std::filesystem::path(skydome->texturePath).filename().string() + "##TexSkydome";
               ImGui::Button(texLabelStr.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 60.0f, 0));
               if (ImGui::BeginDragDropTarget()) {
                   if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                       std::string droppedPath((const char*)payload->Data);
                       std::filesystem::path p(droppedPath);
                       std::string ext = p.extension().string();
                       std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                       if (ext == ".png" || ext == ".jpg" || ext == ".dds") {
                           skydome->texturePath = droppedPath;
                           skydome->texOverride = RC::LoadTex(droppedPath);
                           if (skydome->skydomeHandle < 0) {
                               skydome->skydomeHandle = RC::GenerateSkydomeEx(skydome->texOverride);
                           }
                       }
                   }
                   ImGui::EndDragDropTarget();
               }
               if (!skydome->texturePath.empty()) {
                   ImGui::SameLine();
                   if (ImGui::Button("X##TexSkydome", ImVec2(22, 0))) {
                       skydome->texturePath.clear();
                       skydome->texOverride = -1;
                   }
               }

               ImGui::Unindent(8.0f);
            }
        }

        if (auto* water = e->GetComponent<WaterComponent>()) {
            bool headerOpen = ImGui::CollapsingHeader("Water (水面)", ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) pendingRemove = [e](){ e->RemoveComponent<WaterComponent>(); };
                ImGui::EndPopup();
            }
            if (headerOpen) {
               ImGui::Indent(8.0f);
               bool enabled = water->IsEnabled();
               if (ImGui::Checkbox("Enabled (有効化)##Water", &enabled)) water->SetEnabled(enabled);
               ImGui::Checkbox("Visible (表示)##Water", &water->visible);
               ImGui::Separator();
               ImGui::Text("Wave Parameters (波のパラメータ)");
               ImGui::DragFloat("Wave Height##W1", &water->waveHeight, 0.01f, 0.0f, 5.0f);
               ImGui::DragFloat("Wave Speed##W1", &water->waveSpeed, 0.05f, 0.0f, 10.0f);
               ImGui::DragFloat("Wave Freq##W1", &water->waveFreq, 0.05f, 0.0f, 5.0f);
               ImGui::DragFloat("Wave Height 2##W2", &water->waveHeight2, 0.01f, 0.0f, 5.0f);
               ImGui::DragFloat("Wave Speed 2##W2", &water->waveSpeed2, 0.05f, 0.0f, 10.0f);
               ImGui::DragFloat("Wave Freq 2##W2", &water->waveFreq2, 0.05f, 0.0f, 5.0f);
               ImGui::DragFloat("Steepness", &water->waveSteepness, 0.01f, 0.0f, 1.0f);
               ImGui::Separator();
               ImGui::Text("Water Color (水の色)");
               ImGui::ColorEdit4("Shallow (浅瀬)##Water", &water->shallowColor.x);
               ImGui::ColorEdit4("Deep (深海)##Water", &water->deepColor.x);
               ImGui::Separator();
               ImGui::Text("Material (マテリアル)");
               ImGui::DragFloat("Fresnel Power", &water->fresnelPower, 0.1f, 0.5f, 10.0f);
               ImGui::DragFloat("Specular Power", &water->specularPower, 1.0f, 1.0f, 512.0f);
               ImGui::DragFloat("Normal Scroll", &water->normalScrollSpeed, 0.001f, 0.0f, 0.5f);
               ImGui::DragFloat("Normal Strength", &water->normalStrength, 0.01f, 0.0f, 2.0f);
               ImGui::DragFloat("Env Reflection (環境反射)##Water", &water->environmentCoeff, 0.01f, 0.0f, 1.0f);
               ImGui::Separator();
               ImGui::Text("Mesh Handle: %d", water->meshHandle);
               ImGui::Unindent(8.0f);
            }
        }

        // ============================================
        // Lights
        // ============================================
        auto* tr = e->GetComponent<TransformComponent>();
        RC::Vector3 pos = tr ? tr->position : RC::Vector3{0, 0, 0};

        if (auto* dirLight = e->GetComponent<DirectionalLightComponent>()) {
            bool headerOpen = ImGui::CollapsingHeader("Directional Light (平行光源)", ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) pendingRemove = [e](){ e->RemoveComponent<DirectionalLightComponent>(); };
                ImGui::EndPopup();
            }
            if (headerOpen) {
               ImGui::Indent(8.0f);
               bool enabled = dirLight->IsEnabled();
               if (ImGui::Checkbox("Enabled (有効化)##DirLight", &enabled)) dirLight->SetEnabled(enabled);
               ImGui::Checkbox("Visible (表示)##DirLight", &dirLight->visible);
               ImGui::ColorEdit4("Color (色)##DirLight", &dirLight->color.x);
               ImGui::DragFloat3("Direction (方向)##DirLight", &dirLight->direction.x, 0.05f);
               ImGui::DragFloat("Intensity (強度)##DirLight", &dirLight->intensity, 0.1f, 0.0f, 100.0f);
               ImGui::Text("Handle: %d", dirLight->lightHandle);
               ImGui::Unindent(8.0f);
            }
        }

        if (auto* ptLight = e->GetComponent<PointLightComponent>()) {
            bool headerOpen = ImGui::CollapsingHeader("Point Light (点光源)", ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) pendingRemove = [e](){ e->RemoveComponent<PointLightComponent>(); };
                ImGui::EndPopup();
            }
            if (headerOpen) {
               ImGui::Indent(8.0f);
               bool enabled = ptLight->IsEnabled();
               if (ImGui::Checkbox("Enabled (有効化)##PtLight", &enabled)) ptLight->SetEnabled(enabled);
               ImGui::Checkbox("Visible (表示)##PtLight", &ptLight->visible);
               ImGui::ColorEdit4("Color (色)##PtLight", &ptLight->color.x);
               ImGui::DragFloat("Intensity (強度)##PtLight", &ptLight->intensity, 0.1f, 0.0f, 100.0f);
               ImGui::DragFloat("Radius (半径)##PtLight", &ptLight->radius, 0.5f, 0.0f, 1000.0f);
               ImGui::DragFloat("Decay (減衰)##PtLight", &ptLight->decay, 0.1f, 0.0f, 10.0f);
               ImGui::Text("Handle: %d", ptLight->lightHandle);
               ImGui::Unindent(8.0f);
            }
        }

        if (auto* spLight = e->GetComponent<SpotLightComponent>()) {
            bool headerOpen = ImGui::CollapsingHeader("Spot Light (スポットライト)", ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) pendingRemove = [e](){ e->RemoveComponent<SpotLightComponent>(); };
                ImGui::EndPopup();
            }
            if (headerOpen) {
               ImGui::Indent(8.0f);
               bool enabled = spLight->IsEnabled();
               if (ImGui::Checkbox("Enabled (有効化)##SpLight", &enabled)) spLight->SetEnabled(enabled);
               ImGui::Checkbox("Visible (表示)##SpLight", &spLight->visible);
               ImGui::ColorEdit4("Color (色)##SpLight", &spLight->color.x);
               ImGui::DragFloat3("Direction (方向)##SpLight", &spLight->direction.x, 0.05f);
               ImGui::DragFloat("Intensity (強度)##SpLight", &spLight->intensity, 0.1f, 0.0f, 100.0f);
               ImGui::DragFloat("Distance (距離)##SpLight", &spLight->distance, 0.5f, 0.0f, 1000.0f);
               ImGui::DragFloat("Decay (減衰)##SpLight", &spLight->decay, 0.1f, 0.0f, 10.0f);
               ImGui::DragFloat("CosAngle (角度)##SpLight", &spLight->cosAngle, 0.01f, 0.0f, 1.0f);
               ImGui::Text("Handle: %d", spLight->lightHandle);
               ImGui::Unindent(8.0f);
            }
        }

        if (auto* arLight = e->GetComponent<AreaLightComponent>()) {
            bool headerOpen = ImGui::CollapsingHeader("Area Light (エリアライト)", ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) pendingRemove = [e](){ e->RemoveComponent<AreaLightComponent>(); };
                ImGui::EndPopup();
            }
            if (headerOpen) {
               ImGui::Indent(8.0f);
               bool enabled = arLight->IsEnabled();
               if (ImGui::Checkbox("Enabled (有効化)##ArLight", &enabled)) arLight->SetEnabled(enabled);
               ImGui::Checkbox("Visible (表示)##ArLight", &arLight->visible);
               ImGui::ColorEdit4("Color (色)##ArLight", &arLight->color.x);
               ImGui::DragFloat("Intensity (強度)##ArLight", &arLight->intensity, 0.1f, 0.0f, 100.0f);
               ImGui::DragFloat("Range (範囲)##ArLight", &arLight->range, 0.5f, 0.0f, 1000.0f);
               ImGui::DragFloat("Decay (減衰)##ArLight", &arLight->decay, 0.1f, 0.0f, 10.0f);
               ImGui::DragFloat("Half Width (幅/2)##ArLight", &arLight->halfWidth, 0.1f, 0.0f, 100.0f);
               ImGui::DragFloat("Half Height (高さ/2)##ArLight", &arLight->halfHeight, 0.1f, 0.0f, 100.0f);
               bool twoSided = arLight->twoSided;
               if (ImGui::Checkbox("Two Sided (両面)##ArLight", &twoSided)) arLight->twoSided = twoSided;
               ImGui::Text("Handle: %d", arLight->lightHandle);
               ImGui::Unindent(8.0f);
            }
        }

        // ============================================
        // Camera
        // ============================================
        if (auto* cam = e->GetComponent<CameraComponent>()) {
            bool headerOpen = ImGui::CollapsingHeader("Camera (カメラ)", ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) pendingRemove = [e](){ e->RemoveComponent<CameraComponent>(); };
                ImGui::EndPopup();
            }
            if (headerOpen) {
               ImGui::Indent(8.0f);
               bool enabled = cam->IsEnabled();
               if (ImGui::Checkbox("Enabled (有効化)##Cam", &enabled)) cam->SetEnabled(enabled);
               ImGui::Checkbox("Main Camera (メインカメラ)", &cam->isMain);
               // FOV を度数で表示・編集
               float fovDeg = cam->fovY * 180.0f / 3.14159265f;
               if (ImGui::SliderFloat("FOV (視野角) (deg)", &fovDeg, 1.0f, 179.0f)) {
                   cam->fovY = fovDeg * 3.14159265f / 180.0f;
               }
               ImGui::DragFloat("Near Clip (近クリップ)", &cam->nearZ, 0.01f, 0.001f, 100.0f);
               ImGui::DragFloat("Far Clip (遠クリップ)", &cam->farZ, 1.0f, 0.1f, 10000.0f);
               ImGui::Unindent(8.0f);
            }
        }
        
        // ============================================
        // Collider
        // ============================================
        if (auto* col = e->GetComponent<ColliderComponent>()) {
            bool headerOpen = ImGui::CollapsingHeader("Collider (当たり判定)", ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) pendingRemove = [e](){ e->RemoveComponent<ColliderComponent>(); };
                ImGui::EndPopup();
            }
            if (headerOpen) {
               ImGui::Indent(8.0f);
               bool enabled = col->IsEnabled();
               if (ImGui::Checkbox("Enabled (有効化)##Col", &enabled)) col->SetEnabled(enabled);
               ImGui::Checkbox("Is Trigger (物理反発しない)##Col", &col->isTrigger);
               static const char* colTypeNames[] = { "AABB", "Sphere", "Capsule" };
               int typeIdx = static_cast<int>(col->shape);
               if (ImGui::Combo("Shape (形状)##Col", &typeIdx, colTypeNames, 3)) {
                   col->shape = static_cast<ColliderComponent::Shape>(typeIdx);
               }
               if (col->shape == ColliderComponent::Shape::AABB) {
                   ImGui::DragFloat3("Center (中心)##Col", &col->center.x, 0.1f);
                   ImGui::DragFloat3("Size (サイズ)##Col", &col->size.x, 0.1f);
               } else if (col->shape == ColliderComponent::Shape::Sphere) {
                   ImGui::DragFloat3("Center (中心)##Col", &col->center.x, 0.1f);
                   ImGui::DragFloat("Radius (半径)##Col", &col->radius, 0.1f);
               }
               ImGui::Unindent(8.0f);
            }
        }

        if (auto* rb = e->GetComponent<RigidbodyComponent>()) {
            bool headerOpen = ImGui::CollapsingHeader("Rigidbody (物理演算)", ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) pendingRemove = [e](){ e->RemoveComponent<RigidbodyComponent>(); };
                ImGui::EndPopup();
            }
            if (headerOpen) {
               ImGui::Indent(8.0f);
               bool enabled = rb->IsEnabled();
               if (ImGui::Checkbox("Enabled (有効化)##RB", &enabled)) rb->SetEnabled(enabled);
               ImGui::Checkbox("Use Gravity (重力を使用)", &rb->useGravity);
               ImGui::Checkbox("Is Kinematic (プログラムで動かす)", &rb->isKinematic);
               ImGui::DragFloat("Mass (質量)", &rb->mass, 0.1f, 0.001f, 10000.0f);
               ImGui::DragFloat3("Velocity (速度)", &rb->velocity.x, 0.1f);
               ImGui::Unindent(8.0f);
            }
        }

        if (auto* script = e->GetComponent<NativeScriptComponent>()) {
            bool headerOpen = ImGui::CollapsingHeader("Native Script (自作スクリプト)", ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) pendingRemove = [e](){ e->RemoveComponent<NativeScriptComponent>(); };
                ImGui::EndPopup();
            }
            if (headerOpen) {
                ImGui::Indent(8.0f);
                bool enabled = script->IsEnabled();
                if (ImGui::Checkbox("Enabled (有効化)##Script", &enabled)) script->SetEnabled(enabled);
                
                for (size_t i = 0; i < script->scripts.size(); ++i) {
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::Separator();
                    ImGui::Text("Script: %s", script->scripts[i].scriptTypeName.c_str());
                    ImGui::SameLine();
                    if (ImGui::Button("X##RemoveScript")) {
                        script->RemoveScriptAtIndex(i);
                        ImGui::PopID();
                        break; // 削除時はループを抜けて安全にする
                    }
                    if (script->scripts[i].instance) {
                        ImGui::Indent(8.0f);
                        script->scripts[i].instance->OnImGui();
                        ImGui::Unindent(8.0f);
                    }
                    ImGui::PopID();
                }

                ImGui::Separator();
                ImGui::Text("Add Script:");
                ImGui::SameLine();
                const auto& names = ScriptRegistry::GetScriptNames();
                if (ImGui::BeginCombo("##AddScriptCombo", "(Select Script...)")) {
                    for (const auto& n : names) {
                        if (ImGui::Selectable(n.c_str(), false)) {
                            script->AddScript(n);
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::Unindent(8.0f);
            }
        }
        
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::Button("Add Component", ImVec2(-1, 30))) {
            ImGui::OpenPopup("AddComponentPopup");
        }
        if (ImGui::BeginPopup("AddComponentPopup")) {
            if (ImGui::MenuItem("Camera") && !e->GetComponent<CameraComponent>()) e->AddComponent<CameraComponent>();
            if (ImGui::MenuItem("Collider (当たり判定)") && !e->GetComponent<ColliderComponent>()) e->AddComponent<ColliderComponent>();
            if (ImGui::MenuItem("Rigidbody (物理演算)") && !e->GetComponent<RigidbodyComponent>()) e->AddComponent<RigidbodyComponent>();
            if (ImGui::MenuItem("Native Script") && !e->GetComponent<NativeScriptComponent>()) e->AddComponent<NativeScriptComponent>();
            ImGui::EndPopup();
        }

    if (pendingRemove) pendingRemove();
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
      ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
      std::string lowerMsg = msg;
      std::transform(lowerMsg.begin(), lowerMsg.end(), lowerMsg.begin(), ::tolower);

      if (lowerMsg.find("error") != std::string::npos || lowerMsg.find("fail") != std::string::npos || 
          lowerMsg.find("exception") != std::string::npos || lowerMsg.find("fatal") != std::string::npos) {
        color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); // 赤 (エラー)
      } else if (lowerMsg.find("warn") != std::string::npos) {
        color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f); // 黄 (警告)
      } else if (lowerMsg.find("success") != std::string::npos || lowerMsg.find("init") != std::string::npos || 
                 lowerMsg.find("create") != std::string::npos || lowerMsg.find("load") != std::string::npos || 
                 lowerMsg.find("save") != std::string::npos || lowerMsg.find("start") != std::string::npos) {
        color = ImVec4(0.5f, 1.0f, 0.5f, 1.0f); // 緑 (成功/初期化系)
      } else if (msg.find("[") == 0) {
        color = ImVec4(0.3f, 0.7f, 1.0f, 1.0f); // 水色 (システム/カテゴリタグあり)
      }

      ImGui::PushStyleColor(ImGuiCol_Text, color);
      ImGui::TextUnformatted(msg.c_str());
      ImGui::PopStyleColor();
    }
    
    // オートスクロール（一番下にいる場合のみ追従）
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
      ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
  }
  ImGui::End();


  // ============================
  // Particle Editor パネル
  // ============================
  if (showParticleEditor_) {
    if (ImGui::Begin("Particle Editor Parameters", &showParticleEditor_)) {
      if (!peParticle_) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Particle Editor Preview");
        ImGui::Separator();
        ImGui::TextWrapped("Initialize the preview particle system to start editing.");
        if (ImGui::Button("Initialize Preview Particle", ImVec2(-1, 40))) {
          peParticle_ = std::make_unique<GPUParticle>();
          if (core) {
            // リソース初期化 (背景色: 暗いグレー)
            peRenderTexture_.Initialize(core, 512, 512, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, {0.1f, 0.1f, 0.1f, 1.0f});

            // カメラ初期化
            peCamera_.Initialize(nullptr, {0.0f, 2.0f, -15.0f}, {0.0f, 0.0f, 0.0f}, 0.45f, 1.0f, 0.1f, 100.0f);

            SceneContext ctx{};
            ctx.core = core;
            ctx.pipelineManager = pm;
            peParticle_->Initialize(ctx);
            peParticle_->SetPreviewMode(true);
            peInitialized_ = true;
          }
        }
      } else {
        // --- パラメータ編集 UI ---
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), "Emitter Parameters");
        ImGui::Separator();

        // エミッタ形状
        const char* shapeNames[] = {"Point", "Sphere", "Box", "Cone"};
        int shapeInt = static_cast<int>(peParticle_->emitterShape_);
        if (ImGui::Combo("Emitter Shape", &shapeInt, shapeNames, IM_ARRAYSIZE(shapeNames))) {
          peParticle_->emitterShape_ = static_cast<EmitterShape>(shapeInt);
        }

        // 形状別パラメータ
        if (peParticle_->emitterShape_ == EmitterShape::Sphere || peParticle_->emitterShape_ == EmitterShape::Cone) {
          ImGui::DragFloat("Shape Radius", &peParticle_->shapeRadius_, 0.1f, 0.0f, 50.0f);
        }
        if (peParticle_->emitterShape_ == EmitterShape::Cone) {
          float angleDeg = peParticle_->coneAngle_ * 180.0f / 3.14159265f;
          if (ImGui::DragFloat("Cone Angle (deg)", &angleDeg, 1.0f, 0.0f, 90.0f)) {
            peParticle_->coneAngle_ = angleDeg * 3.14159265f / 180.0f;
          }
        }
        if (peParticle_->emitterShape_ == EmitterShape::Box) {
          ImGui::DragFloat3("Box Size", &peParticle_->shapeBoxSize_.x, 0.1f, 0.0f, 50.0f);
        }

        ImGui::Separator();
        ImGui::Text("Emission");
        int emit = static_cast<int>(peParticle_->GetEmitCount());
        if (ImGui::SliderInt("Emit Count", &emit, 0, 100)) {
          peParticle_->SetEmitCount(static_cast<uint32_t>(emit));
        }
        int maxP = static_cast<int>(peParticle_->GetMaxParticles());
        if (ImGui::SliderInt("Max Particles", &maxP, 256, 16384)) {
          peParticle_->SetMaxParticles(static_cast<uint32_t>(maxP));
        }

        ImGui::Separator();
        ImGui::Text("Lifetime & Scale");
        ImGui::DragFloat("Min Lifetime", &peParticle_->minLifeTime_, 0.1f, 0.1f, 30.0f);
        ImGui::DragFloat("Max Lifetime", &peParticle_->maxLifeTime_, 0.1f, 0.1f, 30.0f);
        ImGui::DragFloat("Min Scale", &peParticle_->minScale_, 0.01f, 0.01f, 5.0f);
        ImGui::DragFloat("Max Scale", &peParticle_->maxScale_, 0.01f, 0.01f, 5.0f);

        ImGui::Separator();
        ImGui::Text("Velocity & Gravity");
        ImGui::DragFloat3("Base Velocity", &peParticle_->baseVelocity_.x, 0.001f);
        ImGui::DragFloat("Velocity Variance", &peParticle_->velocityVariance_, 0.001f, 0.0f, 1.0f);
        ImGui::DragFloat("Gravity", &peParticle_->gravity_, 0.01f, 0.0f, 20.0f);

        ImGui::Separator();
        ImGui::Text("Position");
        ImGui::DragFloat3("Emitter Position", &peParticle_->emitterPosition_.x, 0.1f);

        ImGui::Separator();
        ImGui::Text("Color");
        ImGui::ColorEdit4("Start Color", &peParticle_->startColor_.x);
        ImGui::ColorEdit4("End Color", &peParticle_->endColor_.x);

        // ParticleType と BlendMode
        ImGui::Separator();
        ImGui::Text("Rendering");
        const char* typeNames[] = {"Default", "Explosion", "Rain"};
        int currentTypeInt = static_cast<int>(peParticle_->GetParticleType());
        if (ImGui::Combo("Particle Type", &currentTypeInt, typeNames, IM_ARRAYSIZE(typeNames))) {
          peParticle_->SetParticleType(static_cast<ParticleType>(currentTypeInt));
        }
        const char* blendNames[] = {"None", "Normal", "Add", "Subtract", "Multiply", "Screen", "Premultiplied"};
        int blendInt = static_cast<int>(peParticle_->GetBlendMode());
        if (ImGui::Combo("Blend Mode", &blendInt, blendNames, IM_ARRAYSIZE(blendNames))) {
          peParticle_->SetBlendMode(static_cast<BlendMode>(blendInt));
        }

        // テクスチャ変更
        ImGui::Separator();
        ImGui::Text("Texture");
        ImGui::InputText("Texture Path", peTexPath_, sizeof(peTexPath_));
        ImGui::SameLine();
        if (ImGui::Button("Apply##Tex")) {
          peParticle_->SetTexture(std::string(peTexPath_));
        }

        // JSON Save / Load
        ImGui::Separator();
        ImGui::Text("File I/O");
        ImGui::InputText("JSON Path", peJsonPath_, sizeof(peJsonPath_));
        if (ImGui::Button("Save", ImVec2(100, 30))) {
          peParticle_->SaveToJson(std::string(peJsonPath_));
        }
        ImGui::SameLine();
        if (ImGui::Button("Load", ImVec2(100, 30))) {
          peParticle_->LoadFromJson(std::string(peJsonPath_));
          // テクスチャパスの同期
          #ifdef _MSC_VER
          strncpy_s(peTexPath_, sizeof(peTexPath_), peParticle_->texturePath_.c_str(), _TRUNCATE);
          #endif
        }
      }
    }
    ImGui::End();

    // プレビュー用ウィンドウ
    if (peInitialized_) {
      ImGui::SetNextWindowSizeConstraints(ImVec2(256, 256), ImVec2(2048, 2048));
      if (ImGui::Begin("Particle Editor Preview", &showParticleEditor_)) {
        ImVec2 contentSize = ImGui::GetContentRegionAvail();
        if (contentSize.x > 0 && contentSize.y > 0) {
          float size = std::min(contentSize.x, contentSize.y);
          // 中央に配置
          ImGui::SetCursorPos(ImVec2((contentSize.x - size) * 0.5f + ImGui::GetCursorPosX(), 
                                     (contentSize.y - size) * 0.5f + ImGui::GetCursorPosY()));
          ImGui::Image((ImTextureID)peRenderTexture_.GetSRVGPU().ptr, ImVec2(size, size));
        }
      }
      ImGui::End();
    }
  }

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

void EditorManager::ExportRenderQueueDump() {
  const auto& queue = RC::GetRenderContext().GetLastCommandHistory();
  auto now = std::chrono::system_clock::now();
  std::string timeStr = std::format("{:%Y-%m-%d_%H-%M-%S}", std::chrono::current_zone()->to_local(now));
  
  std::error_code ec;
  std::filesystem::create_directories("../logs/render_queue", ec);
  
  std::string filename = "../logs/render_queue/dump_" + timeStr + ".txt";
  std::ofstream ofs(filename);
  if (ofs) {
    ofs << "--- Render Queue Dump ---\n";
    ofs << "Total Commands: " << queue.size() << "\n\n";
    for (size_t i = 0; i < queue.size(); ++i) {
      const auto& cmd = queue[i];
      std::string displayName = cmd.debugName;
      if (cmd.debugIndex >= 0) {
        std::string resourceName = "";
        if (cmd.debugName.find("Model") != std::string::npos) {
          if (auto* m = RC::GetRenderContext().Models().Get(cmd.debugIndex)) {
            resourceName = std::filesystem::path(m->GetFilePath()).filename().string();
          }
        } else if (cmd.debugName.find("Sprite") != std::string::npos) {
          if (auto* s = RC::GetRenderContext().Sprites().Get(cmd.debugIndex)) {
            resourceName = std::filesystem::path(s->GetFilePath()).filename().string();
          }
        }
        if (!resourceName.empty()) {
          displayName += " [" + resourceName + "]";
        }
      }

      if (cmd.sortKey == 0) {
        ofs << std::format("[{}] {} (Index: {})\n", i, displayName, cmd.debugIndex);
      } else {
        uint8_t layer = static_cast<uint8_t>(cmd.sortKey >> 56);
        std::string layerStr = (layer == 0) ? "Opaque" : (layer == 1) ? "Alpha" : (layer == 2) ? "Glass" : (layer == 3) ? "Overlay" : "?";
        uint32_t depth24 = static_cast<uint32_t>(cmd.sortKey & 0x00FFFFFF);
        uint16_t psoHash = static_cast<uint16_t>((cmd.sortKey >> 40) & 0xFFFF);
        uint16_t texHash = static_cast<uint16_t>((cmd.sortKey >> 24) & 0xFFFF);
        
        ofs << std::format("[{}] {} (Index: {}) - Layer: {}({}), Depth24: {}, PSO: {:04X}, Tex: {:04X}, SortKey: {:016X}\n",
          i, displayName, cmd.debugIndex, layer, layerStr, depth24, psoHash, texHash, cmd.sortKey);
      }
    }
    Log::Print("[Editor] Exported Render Queue to: " + filename);
  } else {
    Log::Print("[Editor] Failed to open file for export: " + filename);
  }
}

void EditorManager::SaveConfig() {
  nlohmann::json j;
  j["showPerfWindow"] = showPerfWindow_;
  j["showRenderQueue"] = showRenderQueue_;
  j["showDemoWindow"] = showDemoWindow_;
  j["showParticleEditor"] = showParticleEditor_;
  
  std::ofstream ofs("../project/EditorConfig.json");
  if (ofs) {
    ofs << j.dump(4);
  }
}

void EditorManager::LoadConfig() {
  std::ifstream ifs("../project/EditorConfig.json");
  if (ifs) {
    try {
      nlohmann::json j;
      ifs >> j;
      if (j.contains("showPerfWindow")) showPerfWindow_ = j["showPerfWindow"];
      if (j.contains("showRenderQueue")) showRenderQueue_ = j["showRenderQueue"];
      if (j.contains("showDemoWindow")) showDemoWindow_ = j["showDemoWindow"];
      if (j.contains("showParticleEditor")) showParticleEditor_ = j["showParticleEditor"];
    } catch (...) {
      Log::Print("[Editor] Failed to parse EditorConfig.json");
    }
  }
}


void EditorManager::Term() {
  if (peParticle_) {
    peParticle_->Finalize();
    peParticle_.reset();
  }
  peRenderTexture_.Release();
  peInitialized_ = false;
}

void EditorManager::RenderParticlePreview(ID3D12GraphicsCommandList* cl, Dx12Core* core, PipelineManager* pm, float deltaTime) {
  if (!showParticleEditor_ || !peParticle_ || !peInitialized_) return;

  // RenderTargetへ状態遷移
  peRenderTexture_.TransitionToRenderTarget(cl);

  D3D12_CPU_DESCRIPTOR_HANDLE rtv = peRenderTexture_.GetRTV();
  cl->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

  // DescriptorHeapのセット（SRV, UAVを使うため）
  ID3D12DescriptorHeap* heaps[] = { core->SRV().Heap() };
  cl->SetDescriptorHeaps(1, heaps);

  // 背景のクリア
  const float clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f }; // 暗いグレー
  cl->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

  // ビューポートとシザー
  D3D12_VIEWPORT viewport{};
  viewport.Width = 512.0f;
  viewport.Height = 512.0f;
  viewport.MaxDepth = 1.0f;
  cl->RSSetViewports(1, &viewport);

  D3D12_RECT scissor{};
  scissor.right = 512;
  scissor.bottom = 512;
  cl->RSSetScissorRects(1, &scissor);

  // カメラ更新とSceneContext構築
  peCamera_.Update();
  SceneContext ctx{};
  ctx.core = core;
  ctx.pipelineManager = pm;
  ctx.camera = &peCamera_;
  ctx.deltaTime = deltaTime;

  // パーティクルの更新と描画
  peParticle_->Update(peCamera_.GetView(), peCamera_.GetProjection(), deltaTime);
  peParticle_->Render(ctx, cl);

  // SRVへ状態遷移（ImGuiで描画するため）
  peRenderTexture_.TransitionToShaderResource(cl);
}
