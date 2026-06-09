#include "Scene.h"
#include "ECS/Entity.h"
#include "ECS/TransformComponent.h"
#include "ECS/LightComponent.h"
#include "ECS/CameraComponent.h"
#include "ECS/ColliderComponent.h"
#include "RenderCommon.h"
#include <cmath>

void Scene::DrawLightGizmos(uint32_t selectedEntityId) {
#if RC_ENABLE_IMGUI
  if (selectedEntityId == 0) return; // 未選択時は描画しない
  for (auto& e : entities_) {
    if (e->Id() != selectedEntityId) continue;
    if (!e->IsVisible()) continue;
    auto* tr = e->GetComponent<TransformComponent>();
    if (!tr) continue;
    RC::Vector3 pos = tr->position;

    // ==========================================================
    // Directional Light — 平行矢印の束（太陽光のイメージ）
    // ==========================================================
    if (auto* dirLight = e->GetComponent<DirectionalLightComponent>()) {
      if (!dirLight->visible) continue;
      RC::Vector3 d = Normalize(dirLight->direction);
      float arrowLen = 3.0f;
      float spread  = 1.0f; // 矢印の間隔

      // direction に直交するベクトルを2本作る
      RC::Vector3 up = {0.0f, 1.0f, 0.0f};
      float dot = d.x * up.x + d.y * up.y + d.z * up.z;
      if (std::abs(dot) > 0.99f) up = {1.0f, 0.0f, 0.0f};

      RC::Vector3 right = {
        d.y * up.z - d.z * up.y,
        d.z * up.x - d.x * up.z,
        d.x * up.y - d.y * up.x
      };
      float rLen = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
      if (rLen > 1e-6f) { right.x /= rLen; right.y /= rLen; right.z /= rLen; }

      RC::Vector3 upLocal = {
        right.y * d.z - right.z * d.y,
        right.z * d.x - right.x * d.z,
        right.x * d.y - right.y * d.x
      };

      // 中央 + 上下左右の5本の平行矢印
      float offsets[][2] = {{0,0}, {1,0}, {-1,0}, {0,1}, {0,-1}};
      for (auto& off : offsets) {
        RC::Vector3 origin = {
          pos.x + right.x * off[0] * spread + upLocal.x * off[1] * spread,
          pos.y + right.y * off[0] * spread + upLocal.y * off[1] * spread,
          pos.z + right.z * off[0] * spread + upLocal.z * off[1] * spread
        };
        RC::Vector3 tip = {
          origin.x + d.x * arrowLen,
          origin.y + d.y * arrowLen,
          origin.z + d.z * arrowLen
        };
        RC::DrawLine3D(origin, tip, dirLight->color, true);

        // 矢じり（先端から斜め2本）
        float headLen = 0.5f;
        RC::Vector3 back = {-d.x, -d.y, -d.z};
        RC::Vector3 h1 = {
          tip.x + (back.x + right.x * 0.4f) * headLen,
          tip.y + (back.y + right.y * 0.4f) * headLen,
          tip.z + (back.z + right.z * 0.4f) * headLen
        };
        RC::Vector3 h2 = {
          tip.x + (back.x - right.x * 0.4f) * headLen,
          tip.y + (back.y - right.y * 0.4f) * headLen,
          tip.z + (back.z - right.z * 0.4f) * headLen
        };
        RC::DrawLine3D(tip, h1, dirLight->color, true);
        RC::DrawLine3D(tip, h2, dirLight->color, true);
      }
    }

    // ==========================================================
    // Point Light — 3軸リング球
    // ==========================================================
    if (auto* ptLight = e->GetComponent<PointLightComponent>()) {
      if (!ptLight->visible) continue;
      RC::DrawSphereRings3D(pos, ptLight->radius, ptLight->color, 32, true);
    }

    // ==========================================================
    // Spot Light — コーン（三角錐）
    // ==========================================================
    if (auto* spLight = e->GetComponent<SpotLightComponent>()) {
      if (!spLight->visible) continue;

      RC::Vector3 d = Normalize(spLight->direction);
      float dist = spLight->distance;

      // コーンの底面半径 = distance * tan(acos(cosAngle))
      float cosA = spLight->cosAngle;
      if (cosA < 0.001f) cosA = 0.001f;
      if (cosA > 0.999f) cosA = 0.999f;
      float sinA = std::sqrt(1.0f - cosA * cosA);
      float baseRadius = dist * (sinA / cosA);

      // コーン先端
      RC::Vector3 tip = {
        pos.x + d.x * dist,
        pos.y + d.y * dist,
        pos.z + d.z * dist
      };

      // 中心線
      RC::DrawLine3D(pos, tip, spLight->color, true);

      // 直交ベクトル
      RC::Vector3 up = {0.0f, 1.0f, 0.0f};
      float dot = d.x * up.x + d.y * up.y + d.z * up.z;
      if (std::abs(dot) > 0.99f) up = {1.0f, 0.0f, 0.0f};

      RC::Vector3 right = {
        d.y * up.z - d.z * up.y,
        d.z * up.x - d.x * up.z,
        d.x * up.y - d.y * up.x
      };
      float rLen = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
      if (rLen > 1e-6f) { right.x /= rLen; right.y /= rLen; right.z /= rLen; }

      RC::Vector3 upLocal = {
        right.y * d.z - right.z * d.y,
        right.z * d.x - right.x * d.z,
        right.x * d.y - right.y * d.x
      };

      // 底面円 + 4本の側面線
      constexpr int kSeg = 24;
      constexpr float kPi2 = 6.2831853f;
      RC::Vector4 coneColor = {spLight->color.x, spLight->color.y, spLight->color.z, 0.5f};

      RC::Vector3 prevPt = {};
      for (int i = 0; i <= kSeg; ++i) {
        float angle = kPi2 * static_cast<float>(i) / static_cast<float>(kSeg);
        float cx = std::cos(angle);
        float cy = std::sin(angle);

        RC::Vector3 pt = {
          tip.x + (right.x * cx + upLocal.x * cy) * baseRadius,
          tip.y + (right.y * cx + upLocal.y * cy) * baseRadius,
          tip.z + (right.z * cx + upLocal.z * cy) * baseRadius
        };

        if (i > 0) {
          RC::DrawLine3D(prevPt, pt, coneColor, true);
        }
        // 4本の側面線（0°, 90°, 180°, 270°）
        if (i % (kSeg / 4) == 0 && i < kSeg) {
          RC::DrawLine3D(pos, pt, spLight->color, true);
        }
        prevPt = pt;
      }
    }

    // ==========================================================
    // Area Light — 四角形 + 法線方向の矢印
    // ==========================================================
    if (auto* arLight = e->GetComponent<AreaLightComponent>()) {
      if (!arLight->visible) continue;

      float hw = arLight->halfWidth;
      float hh = arLight->halfHeight;
      float hr = arLight->range * 0.5f; // rangeの半分をZ方向に

      // AABB（ライトの照射ボリューム）
      RC::Vector3 mn = {pos.x - hw, pos.y - hh, pos.z - hr};
      RC::Vector3 mx = {pos.x + hw, pos.y + hh, pos.z + hr};
      RC::DrawAABB3D(mn, mx, arLight->color, true);

      // 法線方向の矢印（Z+ 方向）
      float arrowLen = 1.5f;
      RC::Vector3 nTip = {pos.x, pos.y, pos.z + arrowLen};
      RC::DrawLine3D(pos, nTip, arLight->color, true);

      // 矢じり
      float hl = 0.3f;
      RC::DrawLine3D(nTip, {nTip.x + hl, nTip.y, nTip.z - hl}, arLight->color, true);
      RC::DrawLine3D(nTip, {nTip.x - hl, nTip.y, nTip.z - hl}, arLight->color, true);
      RC::DrawLine3D(nTip, {nTip.x, nTip.y + hl, nTip.z - hl}, arLight->color, true);
      RC::DrawLine3D(nTip, {nTip.x, nTip.y - hl, nTip.z - hl}, arLight->color, true);
    }
  }
#endif
}

void Scene::DrawCameraGizmos(uint32_t selectedEntityId, float aspect) {
#if RC_ENABLE_IMGUI
  if (selectedEntityId == 0) return;
  for (auto& e : entities_) {
    if (e->Id() != selectedEntityId) continue;
    if (!e->IsVisible()) continue;
    auto* tr = e->GetComponent<TransformComponent>();
    auto* cam = e->GetComponent<CameraComponent>();
    if (!tr || !cam) continue;

    RC::Vector4 frustumColor = {0.5f, 0.8f, 1.0f, 1.0f}; // 水色
    RC::DrawFrustum3D(tr->position, tr->rotation,
                      cam->fovY, aspect, cam->nearZ, cam->farZ,
                      frustumColor, true);
  }
#endif
}

void Scene::DrawColliderGizmos(uint32_t selectedEntityId) {
#if RC_ENABLE_IMGUI
  if (selectedEntityId == 0) return;
  for (auto& e : entities_) {
    if (e->Id() != selectedEntityId) continue;
    if (!e->IsVisible()) continue;
    auto* tr = e->GetComponent<TransformComponent>();
    auto* col = e->GetComponent<ColliderComponent>();
    if (!tr || !col || !col->IsEnabled()) continue;

    RC::Vector4 colColor = {0.2f, 1.0f, 0.2f, 1.0f}; // 黄緑色
    RC::Vector3 worldCenter = {
      tr->position.x + col->center.x,
      tr->position.y + col->center.y,
      tr->position.z + col->center.z
    };

    if (col->shape == ColliderComponent::Shape::Sphere) {
        RC::DrawSphereRings3D(worldCenter, col->radius, colColor, 16, true);
    } else if (col->shape == ColliderComponent::Shape::AABB) {
        RC::Vector3 halfSize = { col->size.x * 0.5f, col->size.y * 0.5f, col->size.z * 0.5f };
        RC::Vector3 minPos = { worldCenter.x - halfSize.x, worldCenter.y - halfSize.y, worldCenter.z - halfSize.z };
        RC::Vector3 maxPos = { worldCenter.x + halfSize.x, worldCenter.y + halfSize.y, worldCenter.z + halfSize.z };
        RC::DrawAABB3D(minPos, maxPos, colColor, true);
    }
  }
#endif
}
