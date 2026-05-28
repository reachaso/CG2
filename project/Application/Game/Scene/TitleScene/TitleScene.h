#pragma once
#include "Scene.h"
#include <RC.h>
#include <dinput.h>

class TitleScene final : public Scene {
public:
  const char *Name() const override { return "Title"; }
  void OnEnter(SceneContext &);
  void OnExit(SceneContext &);

  void Update(SceneManager &sm, SceneContext &ctx) override;
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) override;
  ~TitleScene() override;

private:
  // ==============
  // カメラ関連
  // ==============
  // ======= ビュー/プロジェクション行列 =======
  RC::Matrix4x4 view_, proj_;
  // ======= カメラコントローラ =======

  
};
