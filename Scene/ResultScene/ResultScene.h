#pragma once
#include "Scene.h"
#include <dinput.h>

class ResultScene final : public Scene {
public:
  const char *Name() const override { return "Result"; }
  void Update(SceneManager &sm, SceneContext &ctx) override;
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) override;
};
