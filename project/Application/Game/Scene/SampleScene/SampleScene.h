#pragma once
#include "Scene.h"
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

  // Light
  int directionalLight = -1;

  int pointLight = -1;
  int pointLight2 = -1;

  int spotLight = -1;
  int spotLight2 = -1;

  // ===============
  // 3Dモデル
  // ===============

  int plane = -1;
  Transform *planeTransform_ = nullptr;

  int blockModel = -1;
  RC::Vector4 blockColor_ = {0.8f, 0.9f, 1.0f, 0.12f};

  int model = -1;
  int tx_model = -1;

  int terrain = -1;
  Transform *terrainT_ = nullptr;

  // ===== AnimatedCube =====
  int animatedCube_ = -1;

  // ===== Skeletonテストモデル =====
  int walkModel_ = -1;
  int simpleSkinModel_ = -1;

  // ===== 天球 (Skydome) =====
  int skydome = -1;
  int tx_Skydome_ = -1;
  Transform *skydomeT_ = nullptr;

  // ===== プリミティブメッシュ (PrimitiveMesh) =====
  int primitiveSphere = -1;
  int tx_ball = -1;
  Transform *primitiveSphereT_ = nullptr;

  int testBox = -1;
  int testPlane = -1;
  int testCylinder = -1;
  int testCone = -1;
  int testTorus = -1;
  int testCapsule = -1;

  int skybox = -1;
  Transform *skyboxT_ = nullptr;

  // ===============
  // スプライト
  // ===============

  int sprite = -1;

  float t = 0.0f;

  RC::Vector4 fogColor_ = {0.1f, 0.45f, 0.9f, 0.35f};
  bool isFogEnabled_ = false;

  Transform spriteTransform_;
  RC::Vector2 spriteSize_ = {100.0f, 100.0f};
};
