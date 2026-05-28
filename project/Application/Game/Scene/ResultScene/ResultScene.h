#pragma once
#include "Scene.h"
#include <RC.h>
#include <dinput.h>

class ResultScene final : public Scene {
public:
  const char *Name() const override { return "Result"; }
  void OnEnter(SceneContext &);
  void OnExit(SceneContext &);
  void Update(SceneManager &sm, SceneContext &ctx) override;
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) override;
  ~ResultScene() override;

private:
  // ==============
  // カメラ関連
  // ==============
  RC::Matrix4x4 view_, proj_;

  // ==============
  // スカイドーム関連
  // ==============
  int skydomeModel = -1;
  int txSphere_ = -1;
  Transform *skydomeT_ = nullptr;

  int clearModel = -1;
  Transform *clearModelT_ = nullptr;
  int tx_white = -1;
};
