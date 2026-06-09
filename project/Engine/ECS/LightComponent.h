#pragma once
#include "Math/MathTypes.h"
#include "IComponent.h"
#include <nlohmann/json.hpp>

struct DirectionalLightComponent : public IComponent {
    int lightHandle = -1;
    bool visible = true;
    RC::Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    RC::Vector3 direction = { 0.0f, -1.0f, 0.0f };
    float intensity = 1.0f;

    const char* TypeName() const override { return "DirectionalLightComponent"; }
    nlohmann::json Serialize() const override {
      return {
        {"visible", visible},
        {"color", {color.x, color.y, color.z, color.w}},
        {"direction", {direction.x, direction.y, direction.z}},
        {"intensity", intensity}
      };
    }
    void Deserialize(const nlohmann::json& j) override {
      if (j.contains("visible")) visible = j["visible"].get<bool>();
      if (j.contains("color")) { auto& c = j["color"]; color = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>()}; }
      if (j.contains("direction")) { auto& d = j["direction"]; direction = {d[0].get<float>(), d[1].get<float>(), d[2].get<float>()}; }
      if (j.contains("intensity")) intensity = j["intensity"].get<float>();
    }
};

struct PointLightComponent : public IComponent {
    int lightHandle = -1;
    bool visible = true;
    RC::Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    float intensity = 1.0f;
    float radius = 10.0f;
    float decay = 1.0f;

    const char* TypeName() const override { return "PointLightComponent"; }
    nlohmann::json Serialize() const override {
      return {
        {"visible", visible},
        {"color", {color.x, color.y, color.z, color.w}},
        {"intensity", intensity}, {"radius", radius}, {"decay", decay}
      };
    }
    void Deserialize(const nlohmann::json& j) override {
      if (j.contains("visible")) visible = j["visible"].get<bool>();
      if (j.contains("color")) { auto& c = j["color"]; color = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>()}; }
      if (j.contains("intensity")) intensity = j["intensity"].get<float>();
      if (j.contains("radius")) radius = j["radius"].get<float>();
      if (j.contains("decay")) decay = j["decay"].get<float>();
    }
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

    const char* TypeName() const override { return "SpotLightComponent"; }
    nlohmann::json Serialize() const override {
      return {
        {"visible", visible},
        {"color", {color.x, color.y, color.z, color.w}},
        {"direction", {direction.x, direction.y, direction.z}},
        {"intensity", intensity}, {"distance", distance}, {"decay", decay}, {"cosAngle", cosAngle}
      };
    }
    void Deserialize(const nlohmann::json& j) override {
      if (j.contains("visible")) visible = j["visible"].get<bool>();
      if (j.contains("color")) { auto& c = j["color"]; color = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>()}; }
      if (j.contains("direction")) { auto& d = j["direction"]; direction = {d[0].get<float>(), d[1].get<float>(), d[2].get<float>()}; }
      if (j.contains("intensity")) intensity = j["intensity"].get<float>();
      if (j.contains("distance")) distance = j["distance"].get<float>();
      if (j.contains("decay")) decay = j["decay"].get<float>();
      if (j.contains("cosAngle")) cosAngle = j["cosAngle"].get<float>();
    }
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

    const char* TypeName() const override { return "AreaLightComponent"; }
    nlohmann::json Serialize() const override {
      return {
        {"visible", visible},
        {"color", {color.x, color.y, color.z, color.w}},
        {"intensity", intensity}, {"range", range}, {"decay", decay},
        {"halfWidth", halfWidth}, {"halfHeight", halfHeight}, {"twoSided", twoSided}
      };
    }
    void Deserialize(const nlohmann::json& j) override {
      if (j.contains("visible")) visible = j["visible"].get<bool>();
      if (j.contains("color")) { auto& c = j["color"]; color = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>()}; }
      if (j.contains("intensity")) intensity = j["intensity"].get<float>();
      if (j.contains("range")) range = j["range"].get<float>();
      if (j.contains("decay")) decay = j["decay"].get<float>();
      if (j.contains("halfWidth")) halfWidth = j["halfWidth"].get<float>();
      if (j.contains("halfHeight")) halfHeight = j["halfHeight"].get<float>();
      if (j.contains("twoSided")) twoSided = j["twoSided"].get<bool>();
    }
};
