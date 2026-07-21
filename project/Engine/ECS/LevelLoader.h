#pragma once
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <filesystem>
#include <cassert>
#include <nlohmann/json.hpp>
#include "Common/Log/Log.h"

#include "ECS/Entity.h"
#include "ECS/TransformComponent.h"
#include "ECS/ModelRendererComponent.h"
#include "ECS/ColliderComponent.h"

/// @class LevelLoader
/// @brief Blenderレベルエディタから出力されたJSONレベルデータを読み込み、
///        Entityのリストに変換するユーティリティクラス。
class LevelLoader {
public:
  /// @brief モデルファイルの基準ディレクトリを設定する
  /// @param dir ベースディレクトリパス（例: "Resources/model/"）
  void SetModelBaseDirectory(const std::string& dir) {
    modelBaseDir_ = dir;
    // 末尾にスラッシュがなければ追加
    if (!modelBaseDir_.empty() && modelBaseDir_.back() != '/' && modelBaseDir_.back() != '\\') {
      modelBaseDir_ += '/';
    }
  }

  /// @brief JSONファイルからレベルデータを読み込む
  /// @param filePath JSONファイルのパス
  /// @return 読み込み成功時 true
  bool LoadFromFile(const std::string& filePath) {
    entities_.clear();

    if (!std::filesystem::exists(filePath)) {
      Log::Print("[LevelLoader] File not found: " + filePath);
      return false;
    }

    std::ifstream file(filePath);
    if (!file.is_open()) {
      Log::Print("[LevelLoader] Failed to open: " + filePath);
      return false;
    }

    nlohmann::json deserialized;
    try {
      file >> deserialized;
    } catch (const nlohmann::json::exception& e) {
      Log::Print("[LevelLoader] JSON parse error: " + std::string(e.what()));
      return false;
    }

    assert(deserialized.is_object());
    assert(deserialized.contains("name"));
    assert(deserialized["name"].is_string());

    std::string name = deserialized["name"].get<std::string>();
    if (name != "scene") {
      Log::Print("[LevelLoader] Invalid level data: name is '" + name + "', expected 'scene'");
      return false;
    }

    if (!deserialized.contains("objects")) {
      Log::Print("[LevelLoader] No 'objects' array found");
      return false;
    }

    for (auto& object : deserialized["objects"]) {
      assert(object.contains("type"));
      std::string type = object["type"].get<std::string>();

      if (type == "MESH") {
        ParseObjectRecursive(object, 0);
      }
    }

    Log::Print("[LevelLoader] Loaded: " + filePath +
               " (" + std::to_string(entities_.size()) + " entities)");
    return true;
  }

  /// @brief 読み込んだEntityのリストを取得する
  const std::vector<std::shared_ptr<Entity>>& GetEntities() const {
    return entities_;
  }

  /// @brief 読み込んだEntityのリストをムーブで取得する（所有権移転）
  std::vector<std::shared_ptr<Entity>> TakeEntities() {
    return std::move(entities_);
  }

private:
  std::vector<std::shared_ptr<Entity>> entities_;
  std::string modelBaseDir_ = "Resources/model/"; ///< モデルファイルの基準ディレクトリ

  void ParseObjectRecursive(const nlohmann::json& object, uint64_t parentGuid) {
    std::string name = object.contains("name") ? object["name"].get<std::string>() : "Unnamed";
    auto entity = std::make_shared<Entity>(name);

    if (parentGuid != 0) {
      entity->SetParentGuid(parentGuid);
    }

    // --- TransformComponent ---
    auto& transform = entity->AddComponent<TransformComponent>();
    if (object.contains("transform")) {
      const auto& tr = object["transform"];
      if (tr.contains("translation")) {
        transform.position.x = tr["translation"][0].get<float>();
        transform.position.y = tr["translation"][1].get<float>();
        transform.position.z = tr["translation"][2].get<float>();
      }
      if (tr.contains("rotation")) {
        transform.rotation.x = tr["rotation"][0].get<float>();
        transform.rotation.y = tr["rotation"][1].get<float>();
        transform.rotation.z = tr["rotation"][2].get<float>();
      }
      if (tr.contains("scaling")) {
        transform.scale.x = tr["scaling"][0].get<float>();
        transform.scale.y = tr["scaling"][1].get<float>();
        transform.scale.z = tr["scaling"][2].get<float>();
      }
    }

    // --- ModelRendererComponent ---
    if (object.contains("file_name")) {
      auto& renderer = entity->AddComponent<ModelRendererComponent>();
      std::string fileName = object["file_name"].get<std::string>();
      std::string baseName = fileName.substr(0, fileName.find_last_of('.'));
      renderer.modelPath = modelBaseDir_ + baseName + "/" + fileName;
    }

    // --- ColliderComponent ---
    if (object.contains("collider")) {
      const auto& col = object["collider"];
      auto& collider = entity->AddComponent<ColliderComponent>();

      if (col.contains("type")) {
        std::string colType = col["type"].get<std::string>();
        if (colType == "BOX") {
          collider.shape = ColliderComponent::Shape::AABB;
        } else if (colType == "SPHERE") {
          collider.shape = ColliderComponent::Shape::Sphere;
        }
      }
      if (col.contains("center")) {
        collider.center.x = col["center"][0].get<float>();
        collider.center.y = col["center"][1].get<float>();
        collider.center.z = col["center"][2].get<float>();
      }
      if (col.contains("size")) {
        collider.size.x = col["size"][0].get<float>();
        collider.size.y = col["size"][1].get<float>();
        collider.size.z = col["size"][2].get<float>();
      }
    }

    entities_.push_back(entity);

    // --- children ---
    if (object.contains("children")) {
      for (auto& child : object["children"]) {
        if (child.contains("type")) {
          std::string childType = child["type"].get<std::string>();
          if (childType == "MESH") {
            ParseObjectRecursive(child, entity->Guid());
          }
        }
      }
    }
  }
};
