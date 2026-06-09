#pragma once

#include "IComponent.h"
#include "Math/MathTypes.h"
#include <string>
#include <nlohmann/json.hpp>

/// @brief Water (ocean/sea) surface rendering component.
/// Holds parameters for wave simulation, water color, and rendering.
/// The actual mesh is a high-subdivision plane generated at runtime.
class WaterComponent : public IComponent {
public:
  int meshHandle = -1;     ///< Internal mesh handle (high-subdiv plane)
  int normalMapHandle = -1; ///< Normal map texture handle
  bool visible = true;     ///< Visibility flag

  // --- Wave Parameters ---
  float waveHeight  = 0.3f;  ///< Primary wave amplitude
  float waveSpeed   = 1.5f;  ///< Primary wave speed
  float waveFreq    = 0.8f;  ///< Primary wave frequency
  float waveHeight2 = 0.15f; ///< Secondary wave amplitude
  float waveSpeed2  = 1.0f;  ///< Secondary wave speed
  float waveFreq2   = 1.2f;  ///< Secondary wave frequency
  float waveSteepness = 0.4f; ///< Gerstner steepness (0..1)

  // --- Water Color ---
  RC::Vector4 shallowColor = {0.1f, 0.5f, 0.6f, 0.85f}; ///< Shallow water color (RGBA)
  RC::Vector4 deepColor    = {0.02f, 0.1f, 0.25f, 0.95f}; ///< Deep water color (RGBA)

  // --- Material Parameters ---
  float fresnelPower = 3.0f;      ///< Fresnel exponent
  float specularPower = 128.0f;   ///< Specular highlight sharpness
  float normalScrollSpeed = 0.03f; ///< Normal map scroll speed
  float normalStrength = 0.6f;    ///< Normal map intensity

  float environmentCoeff = 0.5f;  ///< Environment map reflection coefficient

  // --- Mesh Generation ---
  float planeWidth  = 100.0f;  ///< Water plane width
  float planeHeight = 100.0f;  ///< Water plane depth
  uint32_t segments = 128;     ///< Plane subdivision count (per axis)

  std::string normalMapPath;  ///< Normal map texture path for serialization

  /// @brief Check if a valid mesh is assigned
  bool HasMesh() const { return meshHandle >= 0; }

  const char* TypeName() const override { return "WaterComponent"; }

  nlohmann::json Serialize() const override {
    return {
      {"visible", visible},
      {"waveHeight", waveHeight},
      {"waveSpeed", waveSpeed},
      {"waveFreq", waveFreq},
      {"waveHeight2", waveHeight2},
      {"waveSpeed2", waveSpeed2},
      {"waveFreq2", waveFreq2},
      {"waveSteepness", waveSteepness},
      {"shallowColor", {shallowColor.x, shallowColor.y, shallowColor.z, shallowColor.w}},
      {"deepColor", {deepColor.x, deepColor.y, deepColor.z, deepColor.w}},
      {"fresnelPower", fresnelPower},
      {"specularPower", specularPower},
      {"normalScrollSpeed", normalScrollSpeed},
      {"normalStrength", normalStrength},
      {"environmentCoeff", environmentCoeff},
      {"planeWidth", planeWidth},
      {"planeHeight", planeHeight},
      {"segments", segments},
      {"normalMapPath", normalMapPath}
    };
  }

  void Deserialize(const nlohmann::json& j) override {
    if (j.contains("visible")) visible = j["visible"].get<bool>();
    if (j.contains("waveHeight")) waveHeight = j["waveHeight"].get<float>();
    if (j.contains("waveSpeed")) waveSpeed = j["waveSpeed"].get<float>();
    if (j.contains("waveFreq")) waveFreq = j["waveFreq"].get<float>();
    if (j.contains("waveHeight2")) waveHeight2 = j["waveHeight2"].get<float>();
    if (j.contains("waveSpeed2")) waveSpeed2 = j["waveSpeed2"].get<float>();
    if (j.contains("waveFreq2")) waveFreq2 = j["waveFreq2"].get<float>();
    if (j.contains("waveSteepness")) waveSteepness = j["waveSteepness"].get<float>();
    if (j.contains("shallowColor")) {
      auto& c = j["shallowColor"];
      shallowColor = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>()};
    }
    if (j.contains("deepColor")) {
      auto& c = j["deepColor"];
      deepColor = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>()};
    }
    if (j.contains("fresnelPower")) fresnelPower = j["fresnelPower"].get<float>();
    if (j.contains("specularPower")) specularPower = j["specularPower"].get<float>();
    if (j.contains("normalScrollSpeed")) normalScrollSpeed = j["normalScrollSpeed"].get<float>();
    if (j.contains("normalStrength")) normalStrength = j["normalStrength"].get<float>();
    if (j.contains("environmentCoeff")) environmentCoeff = j["environmentCoeff"].get<float>();
    if (j.contains("planeWidth")) planeWidth = j["planeWidth"].get<float>();
    if (j.contains("planeHeight")) planeHeight = j["planeHeight"].get<float>();
    if (j.contains("segments")) segments = j["segments"].get<uint32_t>();
    if (j.contains("normalMapPath")) normalMapPath = j["normalMapPath"].get<std::string>();
  }
};
