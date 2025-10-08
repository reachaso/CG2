#include "CameraController.h"
#include "imGui/imGui.h"

// 画角やクリップは共通、初期姿勢だけ可変にしておきます
void CameraController::Initialize(Input *input, const Vector3 &mainPos,
                                const Vector3 &mainRot,
                float fovY, float aspect, float nearZ, float farZ) {
  input_ = input;
  debug_.Initialize(input_, fovY, aspect, nearZ, farZ);
  main_.Initialize(mainPos, mainRot, fovY, aspect, nearZ, farZ);
}

// TAB 切替と各カメラの更新
void CameraController::Update() {
  // 切替
  if (input_ && input_->IsKeyTrigger(DIK_TAB)) {
    useDebug_ = !useDebug_;
  }
  if (useDebug_) {
    if (!(ImGui::GetIO().WantCaptureMouse ||
          ImGui::GetIO().WantCaptureKeyboard)) {
      debug_.Update();
    }
  } else {
    main_.Update();
  }
}

void CameraController::DrawImGui() {
  ImGui::Begin("カメラモード : Tab");

  if (useDebug_) {
    ImGui::Text("デバッグカメラモード");
    ImGui::Dummy(ImVec2(0.0f, 5.0f));

    if (ImGui::Button("リセット")) {
      debug_.Reset(); // 位置・回転を初期値へ
    }
    ImGui::SameLine();
    if (ImGui::Button("操作方法")) {
      showGuide_ = !showGuide_;
    }
  } else {
    ImGui::Text("メインカメラモード");
  }
  ImGui::End();

  // 操作ガイド
  if (showGuide_) {
    ImGui::Begin("操作方法", &showGuide_, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text(" WASD : 前後左右移動");
    ImGui::Text(" QE : 上下移動");
    ImGui::Text(" 十字キー : カメラの回転");
    ImGui::Text(" マウスホイール : 前後移動");
    ImGui::Text(" 左クリックしながらドラッグ : 回転");
    ImGui::Text(" マウスホイール押しながらドラッグ : 上下左右移動");
    ImGui::End();
  }
}
