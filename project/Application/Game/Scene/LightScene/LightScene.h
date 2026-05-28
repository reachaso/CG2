#pragma once
#include "Scene.h"
#include <RC.h>

class LightScene final : public Scene {
public:
  const char *Name() const override { return "Light"; }
  void OnEnter(SceneContext &ctx) override;
  void OnExit(SceneContext &ctx) override;
  void Update(SceneManager &sm, SceneContext &ctx) override;
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) override;

  void DrawImGui();

private:
  RC::Matrix4x4 view_, proj_;

  // カメラ



  int terrain_ = -1;
  Transform *terrainT_ = nullptr;

  int ball_ = -1;
  Transform *ballT_ = nullptr;

  int plane_gltf_ = -1;
  Transform *planeT_ = nullptr;

  int DirectionalLight_ = -1;

  int PointLight1_ = -1;
  RC::PointLightSource *pl1_source_;

  int PointLight2_ = -1;
  RC::PointLightSource *pl2_source_;

  int SpotLight1_ = -1;
  RC::SpotLightSource *sl1_source_;

  int SpotLight2_ = -1;
  RC::SpotLightSource *sl2_source_;

  int AreaLight1_ = -1;
  RC::AreaLightSource *al1_source_;

  int AreaLight2_ = -1;
  RC::AreaLightSource *al2_source_;
};
