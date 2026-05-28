#pragma once
#include "Math/MathTypes.h"
#include "IComponent.h"

struct DirectionalLightComponent : public IComponent {
    int lightHandle = -1;
    bool visible = true;
    RC::Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    RC::Vector3 direction = { 0.0f, -1.0f, 0.0f };
    float intensity = 1.0f;
};

struct PointLightComponent : public IComponent {
    int lightHandle = -1;
    bool visible = true;
    RC::Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    float intensity = 1.0f;
    float radius = 10.0f;
    float decay = 1.0f;
};

struct SpotLightComponent : public IComponent {
    int lightHandle = -1;
    bool visible = true;
    RC::Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    RC::Vector3 direction = { 0.0f, -1.0f, 0.0f };
    float intensity = 1.0f;
    float distance = 20.0f;
    float decay = 1.0f;
    float cosAngle = 0.866f; // approx cos(30 deg)
};

struct AreaLightComponent : public IComponent {
    int lightHandle = -1;
    bool visible = true;
    RC::Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    float intensity = 1.0f;
    float range = 10.0f;
    float decay = 1.0f;
    float halfWidth = 1.0f;
    float halfHeight = 1.0f;
    bool twoSided = false;
};
