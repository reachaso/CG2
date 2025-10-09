#pragma once
#include "Scene.h"
#include "model3D/model3D.h"
#include <dinput.h>
#include "Texture/TextureManager/TextureManager.h"
#include "Camera/CameraController.h"

class GameScene final : public Scene {
public:
  const char *Name() const override { return "Game"; }
  void OnEnter(SceneContext &ctx) override;
  void OnExit(SceneContext &) override;

  void Update(SceneManager &sm, SceneContext &ctx) override;
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) override;

private:
  // テクスチャ
  TextureManager texMgr_;

  Model3D *teapot = nullptr;
  int tx_teapot = -1;
  
  // カメラ
  CameraController camera_;
};
