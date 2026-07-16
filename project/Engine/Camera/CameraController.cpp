#include "CameraController.h"
#include "RC.h" // Transform 定義のため（プロジェクト側で提供されている想定）
#include "imGui/imGui.h"
#include "imgui/ImGuizmo.h"
#include <cmath> // tanf, sqrtf, fabs

namespace RC {

// 画角やクリップは共通、初期姿勢だけ可変にしておきます
void CameraController::Initialize(Input *input, const Vector3 &mainPos,
                                  const Vector3 &mainRot, float fovY,
                                  float aspect, float nearZ, float farZ) {
  input_ = input;
  fovY_ = fovY;
  aspect_ = aspect;

  debug_.Initialize(input_, fovY, aspect, nearZ, farZ);
  main_.Initialize(mainPos, mainRot, fovY, aspect, nearZ, farZ);
}

void CameraController::SetTarget(const ::Transform *target) {
  target_ = target;

  // 追従状態をリセット（次フレームでスナップ→以降は補間）
  follow_.initialized = false;
  follow_.lookAheadX = 0.0f;

  if (target_) {
    follow_.prevTargetPos = target_->translation;
  }
}

void CameraController::SetFollowOffsets(const Vector3 &camOffset,
                                        const Vector3 &targetOffset) {
  followParams_.camOffset = camOffset;
  followParams_.targetOffset = targetOffset;
}

void CameraController::SetFollowSharpness(float camSharpness) {
  followParams_.camSharpness = camSharpness;
}

void CameraController::SetLookAhead(float lookAhead, float lookAheadFactor,
                                    float lookAheadSharpness) {
  followParams_.lookAhead = lookAhead;
  followParams_.lookAheadFactor = lookAheadFactor;
  followParams_.lookAheadSharpness = lookAheadSharpness;
}

void CameraController::SetFocusSharpness(float focusSharpness) {
  followParams_.focusSharpness = focusSharpness;
}

void CameraController::SetFollowYSettings(float deadZoneY, float sharpnessUp,
                                          float sharpnessDown, float maxSpeed) {
  followParams_.deadZoneY = deadZoneY;
  followParams_.ySharpnessUp = sharpnessUp;
  followParams_.ySharpnessDown = sharpnessDown;
  followParams_.yMaxSpeed = maxSpeed;
}

void CameraController::SetFollowBounds(float left, float right, float bottom,
                                       float top, bool enable) {
  bounds_.left = left;
  bounds_.right = right;
  bounds_.bottom = bottom;
  bounds_.top = top;
  bounds_.enabled = enable;
}

// TAB 切替と各カメラの更新
void CameraController::Update(float dt) {
#ifdef _DEBUG
  // 切替
  if (input_ && (input_->IsKeyTrigger(DIK_TAB) || input_->IsKeyTrigger(DIK_F1))) {
    useDebug_ = !useDebug_;
  }
#endif

  if (useDebug_) {
    #if RC_ENABLE_IMGUI
    // ギズモ操作中またはマウスがギズモに乗っている時はカメラ操作をブロックする
    bool isGuizmoActive = ImGuizmo::IsUsing() || ImGuizmo::IsOver();

    if (!isGuizmoActive) {
        // Viewport上でマウスボタン（右/中）が押されたらドラッグ開始 (左クリックはギズモや選択と競合するため除外)
        if (input_->IsViewportHovered() && (input_->IsMousePressed(1) || input_->IsMousePressed(2))) {
          isDraggingCamera_ = true;
        }
    }
    
    // 右・中のボタンが離されたらドラッグ終了
    if (!input_->IsMousePressed(1) && !input_->IsMousePressed(2)) {
      isDraggingCamera_ = false;
    }

    // マウスがViewport上にあるか、カメラをドラッグ中ならマウスをカメラに渡す
    bool captureMouse = ImGui::GetIO().WantCaptureMouse && !(input_->IsViewportHovered() || isDraggingCamera_);
    
    // Viewport上にあるかドラッグ中なら、キーボード（WASD等）もカメラに渡す
    bool captureKeyboard = ImGui::GetIO().WantCaptureKeyboard && !(input_->IsViewportHovered() || isDraggingCamera_);

    if (!(captureMouse || captureKeyboard) && !isGuizmoActive) {
#else
if (true) {
#endif
      debug_.Update();
    }
  } else {
    // メインカメラ：ターゲットが設定されてたら追従を内部で更新
    if (target_) {
      UpdateFollow_(dt);
    }
    main_.Update();
  }

  if (useDebug_) {
    worldPos_ = debug_.GetPosition();
  } else {
    worldPos_ = main_.GetPosition();
  }
}

void CameraController::UpdateFollow_(float dt) {
  if (!target_) {
    return;
  }

  const Vector3 targetPos = target_->translation;

  // --- 先読みの「目標」（瞬間値） ---
  const float dx = targetPos.x - follow_.prevTargetPos.x;
  const float desiredLookAheadX =
      RC::Clamp(dx * followParams_.lookAheadFactor, -followParams_.lookAhead,
                followParams_.lookAhead);

  if (!follow_.initialized) {
    follow_.lookAheadX = desiredLookAheadX;
    follow_.focus = RC::Add(targetPos, followParams_.targetOffset);
    follow_.focus =
        RC::Add(follow_.focus, RC::Vector3{follow_.lookAheadX, 0.0f, 0.0f});
  } else {
    // ===== 先読みの現在値をなめらかに =====
    {
      const float tLA =
          RC::ExpSmoothingFactor(followParams_.lookAheadSharpness, dt);
      follow_.lookAheadX =
          follow_.lookAheadX + (desiredLookAheadX - follow_.lookAheadX) * tLA;
    }

    // ===== 追従したい注視点（目標） =====
    const RC::Vector3 desiredFocus =
        RC::Add(RC::Add(targetPos, followParams_.targetOffset),
                RC::Vector3{follow_.lookAheadX, 0.0f, 0.0f});

    // ===== X/Z は今まで通りなめらかに =====
    const float tF = RC::ExpSmoothingFactor(followParams_.focusSharpness, dt);
    follow_.focus.x = follow_.focus.x + (desiredFocus.x - follow_.focus.x) * tF;
    follow_.focus.z = follow_.focus.z + (desiredFocus.z - follow_.focus.z) * tF;

    // ===== Y は「デッドゾーン + 上下で追従速度を変える」 =====
    const float dy = desiredFocus.y - follow_.focus.y;

    float targetY = follow_.focus.y;
    if (std::fabs(dy) > followParams_.deadZoneY) {
      // デッドゾーン外に出たら、ゾーン端までだけ追いかける
      targetY = desiredFocus.y - (dy > 0.0f ? followParams_.deadZoneY
                                            : -followParams_.deadZoneY);
    }

    const float sharpY = (targetY > follow_.focus.y)
                             ? followParams_.ySharpnessUp
                             : followParams_.ySharpnessDown;
    const float tY = RC::ExpSmoothingFactor(sharpY, dt);
    float newY = follow_.focus.y + (targetY - follow_.focus.y) * tY;

    // 最大速度制限（酔い＆ガクガク対策）
    if (followParams_.yMaxSpeed > 0.0f) {
      const float maxStep = followParams_.yMaxSpeed * dt;
      const float step = newY - follow_.focus.y;
      if (step > maxStep)
        newY = follow_.focus.y + maxStep;
      if (step < -maxStep)
        newY = follow_.focus.y - maxStep;
    }

    follow_.focus.y = newY;
  }

  RC::Vector3 focus = follow_.focus;

  // ===== focus をマップ境界でクランプ =====
  if (bounds_.enabled) {
    const RC::Vector3 delta =
        RC::Sub(followParams_.camOffset, followParams_.targetOffset);
    const float dist =
        std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);

    const float halfH = std::tanf(fovY_ * 0.5f) * dist;
    const float halfW = halfH * aspect_;

    const float minX = bounds_.left + halfW;
    const float maxX = bounds_.right - halfW;
    const float minY = bounds_.bottom + halfH;
    const float maxY = bounds_.top - halfH;

    if (minX > maxX) {
      focus.x = (bounds_.left + bounds_.right) * 0.5f;
    } else {
      focus.x = RC::Clamp(focus.x, minX, maxX);
    }

    if (minY > maxY) {
      focus.y = (bounds_.bottom + bounds_.top) * 0.5f;
    } else {
      focus.y = RC::Clamp(focus.y, minY, maxY);
    }
  }

  // クランプ結果を状態にも戻す（壁際での安定が上がる）
  follow_.focus = focus;

  // ===== カメラ位置を focus から決める（offset維持） =====
  const RC::Vector3 delta =
      RC::Sub(followParams_.camOffset, followParams_.targetOffset);
  const RC::Vector3 desiredPos = RC::Add(focus, delta);

  if (!follow_.initialized) {
    follow_.camPos = desiredPos;
    follow_.initialized = true;
  } else {
    const float t = RC::ExpSmoothingFactor(followParams_.camSharpness, dt);
    follow_.camPos = RC::Lerp(follow_.camPos, desiredPos, t);
  }

  // 注視点へ向ける
  const RC::Vector3 rot = RC::CameraMath::LookAtYawPitch(follow_.camPos, focus);

  main_.SetPosition(follow_.camPos);
  main_.SetRotation(rot);

  follow_.prevTargetPos = targetPos;
}

void CameraController::DrawImGui() {
#if RC_ENABLE_IMGUI
#if RC_ENABLE_IMGUI
  ImGui::Begin("カメラモード : F1 / Tab");

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
    if (target_) {
      ImGui::Text("追従: ON");
    } else {
      ImGui::Text("追従: OFF");
    }
  }

  // 現在位置表示
  ImGui::Dummy(ImVec2(0.0f, 5.0f));
  ImGui::Text("位置: (%.2f, %.2f, %.2f)", worldPos_.x, worldPos_.y,
              worldPos_.z);
  ImGui::Dummy(ImVec2(0.0f, 5.0f));

  ImGui::End();

  // 操作ガイド
  if (showGuide_) {
    ImGui::Begin("操作方法", &showGuide_, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text(" WASD : 前後左右移動");
    ImGui::Text(" QE : 上下移動");
    ImGui::Text(" 十字キー : カメラの回転");
    ImGui::Text(" マウスホイール : 前後移動");
    ImGui::Text(" 右クリックしながらドラッグ : 回転");
    ImGui::Text(" マウスホイール押しながらドラッグ : 上下左右移動");
    ImGui::End();
  }

#endif

#endif
}

void CameraController::SetMainPosition(const Vector3 &pos) {
  main_.SetPosition(pos);
}

void CameraController::SetMainRotation(const Vector3 &rot) {
  main_.SetRotation(rot);
}

void CameraController::SetDebugPosition(const Vector3 &pos) {
  debug_.SetPosition(pos);
  if (useDebug_) {
    worldPos_ = pos;
  }
}

void CameraController::SetDebugRotation(const Vector3 &rot) {
  debug_.SetRotation(rot);
}

void CameraController::SetDebugTransform(const Vector3 &pos,
                                         const Vector3 &rot) {
  debug_.SetTransform(pos, rot);
  if (useDebug_) {
    worldPos_ = pos;
  }
}

void CameraController::SetPosition(const Vector3 &pos) {
  if (useDebug_) {
    debug_.SetPosition(pos);
  } else {
    main_.SetPosition(pos);
  }
  worldPos_ = pos; // アクティブ側の位置を即反映
}

void CameraController::SetRotation(const Vector3 &rot) {
  if (useDebug_) {
    debug_.SetRotation(rot);
  } else {
    main_.SetRotation(rot);
  }
}

void CameraController::SetTransform(const Vector3 &pos, const Vector3 &rot) {
  if (useDebug_) {
    debug_.SetTransform(pos, rot);
  } else {
    main_.SetPosition(pos);
    main_.SetRotation(rot);
  }
  worldPos_ = pos;
}

} // namespace RC
