#pragma once
#include "Scene.h"
#include <dinput.h>
#include "RC.h"

class GameOverScene final : public Scene {
public:
  const char *Name() const override { return "GameOver"; }
  void OnEnter(SceneContext &);
  void OnExit(SceneContext &);
  void Update(SceneManager &sm, SceneContext &ctx) override;
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) override;
  ~GameOverScene() override;

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

  int gameOverModel = -1;
  Transform *gameOverModelT_ = nullptr;
  int tx_white = -1;
  
  float noiseIntensity_ = 0.0f;
};
