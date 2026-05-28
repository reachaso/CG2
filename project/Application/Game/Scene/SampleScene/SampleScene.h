#pragma once
#include "Scene.h"
#include "GPUParticle.h"
#include <dinput.h>
#include <RC.h>

class SampleScene final : public Scene {
public:
  const char *Name() const override { return "Sample"; }
  void OnEnter(SceneContext &ctx) override;
  void OnExit(SceneContext &) override;

  void Update(SceneManager &sm, SceneContext &ctx) override;
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) override;
  void DrawImGui();
  ~SampleScene() override;

private:
  RC::Matrix4x4 view_, proj_;

  // カメラ
  RC::CameraController camera_;


  // ===============
  // 3Dモデル
  // ===============

  int tx_model = -1;
  int tx_ball = -1;



  // ===============
  // スプライト
  // ===============

  int sprite = -1;

  float t = 0.0f;

  RC::Vector4 fogColor_ = {0.1f, 0.45f, 0.9f, 0.35f};
  bool isFogEnabled_ = false;

  Transform spriteTransform_;
  RC::Vector2 spriteSize_ = {100.0f, 100.0f};

  // ===============
  // GPU Particle
  // ===============
  std::unique_ptr<GPUParticle> gpuParticle_;
};
