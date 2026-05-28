#pragma once
#include "Scene.h"
#include <RC.h>
#include <dinput.h>

class SelectScene final : public Scene {
public:
  const char *Name() const override { return "Select"; }
  void OnEnter(SceneContext &);
  void OnExit(SceneContext &);
  void Update(SceneManager &sm, SceneContext &ctx) override;
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) override;
  ~SelectScene() override;

  static constexpr int kStageMax = 6;

private:
  // ==============
  // カメラ関連
  // ==============
  // ======= ビュー/プロジェクション行列 =======
  RC::Matrix4x4 view_, proj_;
  // ======= カメラコントローラ =======


  // ==============
  // スカイドーム関連
  // ==============
  // ======= モデルとテクスチャ =======
  int skydomeModel = -1;
  int txSphere_ = -1;
  // ======= トランスフォーム参照 =======
  Transform *skydomeT_ = nullptr;

  // ==============
  // ステージ選択用
  // ==============

  int selectStage = 0;

  int stageModels[kStageMax] = {-1, -1, -1, -1, -1, -1};
  Transform *stageTransforms[kStageMax] = {nullptr};

  float ringAngle_ = 0.0f;
  float ringTargetAngle_ = 0.0f;
  bool isRingRotating_ = false;

  int AModel = -1;
  Transform *ATransform = nullptr;

  int DModel = -1;
  Transform *DTransform = nullptr;
};
