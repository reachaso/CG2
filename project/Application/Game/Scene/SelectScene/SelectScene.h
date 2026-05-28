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


};
