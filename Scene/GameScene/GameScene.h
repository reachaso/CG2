#pragma once
#include "Scene.h"
#include "model3D/model3D.h"
#include <dinput.h>

class GameScene final : public Scene {
public:
  const char *Name() const override { return "Game"; }
  void OnEnter(SceneContext &) override {}
  void OnExit(SceneContext &) override {}

  void Update(SceneManager &sm, SceneContext &ctx) override;
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) override;

private:
  Model3D *teapot;
  int tx_teapot;
};
