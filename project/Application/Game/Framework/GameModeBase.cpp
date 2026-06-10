#include "GameModeBase.h"
#include "../Scene/Scene.h"

GameModeBase::GameModeBase() {
    gameState_ = std::make_unique<GameStateBase>();
}

void GameModeBase::BeginPlay(SceneContext& ctx) {
    if (gameState_) {
        gameState_->BeginPlay();
    }
}

void GameModeBase::Tick(SceneContext& ctx) {
    if (gameState_) {
        gameState_->Tick(ctx.deltaTime);
    }
}
