#pragma once
#include <array>
#include <string>

struct AppConfig {
  int width = 1280;
  int height = 720;
  bool vsync = true;
  std::string title = "myEngine";
  std::array<float, 4> clearColor{0.1f, 0.25f, 0.5f, 1.0f};
};
