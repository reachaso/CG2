#include "ECS/ScriptableEntity.h"
#include "ECS/ScriptRegistry.h"
#include "ECS/TransformComponent.h"
#include <iostream>

/// @brief サンプル用: 毎フレーム回転するスクリプト
class TestRotatorScript : public ScriptableEntity {
protected:
    void OnCreate() override {
        std::cout << "[TestRotatorScript] OnCreate: アタッチされました！\n";
    }

    void OnUpdate(float deltaTime) override {
        if (auto* tr = GetComponent<TransformComponent>()) {
            // Y軸を中心に回転させる (1秒間に約1ラジアン)
            tr->rotation.y += 1.0f * deltaTime;
        }
    }

    void OnDestroy() override {
        std::cout << "[TestRotatorScript] OnDestroy: 破棄されました！\n";
    }
};

// これを書くことで、自動的に ScriptRegistry に "TestRotatorScript" として登録され、
// インスペクタの Native Script のプルダウンに表示されます。
REGISTER_SCRIPT(TestRotatorScript)
