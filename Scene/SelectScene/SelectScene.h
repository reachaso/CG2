#pragma once
#include "Scene.h"
#include <dinput.h>

class SelectScene final : public Scene {
public:
  const char *Name() const override { return "Select"; }
  void Update(SceneManager &sm, SceneContext &ctx) override;
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) override;
};
