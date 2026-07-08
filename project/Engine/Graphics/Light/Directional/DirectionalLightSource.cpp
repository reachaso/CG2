#include "DirectionalLightSource.h"
#include "imgui/imgui.h"
#include <cmath>
#include <string>

namespace RC {

DirectionalLightSource::DirectionalLightSource() {
  // デフォルトは上からの白いライト
  data_.color = {1.0f, 1.0f, 1.0f, 1.0f};
  data_.direction = {0.0f, -1.0f, 0.0f};
  data_.intensity = 0.0f; // デフォルトで照らさないようにする
}

void DirectionalLightSource::SetDirection(const Vector3 &dir, bool normalize) {
  Vector3 d = dir;
  if (normalize) {
    float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    if (len > 1e-6f) {
      d.x /= len;
      d.y /= len;
      d.z /= len;
    }
  }
  data_.direction = d;
}

void DirectionalLightSource::SetColor(const Vector3 &rgb, float alpha) {
  data_.color = {rgb.x, rgb.y, rgb.z, alpha};
}

void DirectionalLightSource::SetColor(const Vector4 &rgba) {
  data_.color = rgba;
}

void DirectionalLightSource::SetIntensity(float intensity) {
  data_.intensity = intensity;
}

DirectionalLight DirectionalLightSource::DataForGPU() const {
  DirectionalLight out = data_;
  if (!enabled_) {
    out.intensity = 0.0f;
  }
  return out;
}

void DirectionalLightSource::DrawImGui(const char *name) {
#if RC_ENABLE_IMGUI


  std::string label = name ? std::string(name) : std::string("Light");
  if (!ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  // 有効/無効
  ImGui::Checkbox((std::string("有効##") + label).c_str(), &enabled_);
  if (!enabled_) {
    ImGui::SameLine();
    ImGui::TextDisabled("(OFF: intensity=0)");
  }
  ImGui::Separator();

  if (enabled_) {
    ImGui::TextUnformatted("Lighting");

    static const char *kModes[] = {"None", "Lambert", "HalfLambert"};

    int mode = lightingMode_;
    if (mode < 0)
      mode = 0;
    if (mode > 2)
      mode = 2;

    if (ImGui::Combo((std::string("モード##") + label).c_str(), &mode, kModes,
                     IM_ARRAYSIZE(kModes))) {
      lightingMode_ = mode;
    }

    // 光カラー
    ImGui::ColorEdit3((std::string("光カラー##") + label).c_str(),
                      &data_.color.x, ImGuiColorEditFlags_Float);

    // 方向（-1..1）＋正規化
    bool dirChanged =
        ImGui::DragFloat3((std::string("光方向(x,y,z)##") + label).c_str(),
                          &data_.direction.x, 0.01f, -1.0f, 1.0f, "%.2f");
    if (dirChanged) {
      Vector3 d = data_.direction;
      float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
      if (len > 1e-6f) {
        d.x /= len;
        d.y /= len;
        d.z /= len;
        data_.direction = d;
      }
    }

    // 強さ
    ImGui::DragFloat((std::string("強さ##") + label).c_str(), &data_.intensity,
                     0.01f, 0.0f, 16.0f, "%.2f");

    ImGui::Dummy(ImVec2(0, 6));

    if (lightingMode_ != 0) {
      ImGui::DragFloat((std::string("光沢度(shininess)##") + label).c_str(),
                       &shininess_, 0.5f, 0.0f, 256.0f, "%.1f");
      ImGui::SameLine();
      ImGui::TextDisabled("(0で鏡面なし)");
    }
  }

#endif
}

} // namespace RC
