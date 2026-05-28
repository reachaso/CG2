#include "GameScene.h"
#include "RenderCommon.h"
#include "SceneManager.h"
#include "StageSelection/StageSelection.h"
#include "Graphics/PostProcess/PostProcess.h"

GameScene::~GameScene() {
  SceneContext dummy{};
  OnExit(dummy);
}

void GameScene::OnEnter(SceneContext &ctx) {
  // カメラは SceneManager が所有 (ctx.camera)

  // ======= ブロックモデル =======
  blockModel = RC::LoadModel("Resources/model/block");
  RC::SetModelLightingMode(blockModel, None);
  tx_block = RC::LoadTex("Resources/white1x1.png");

  // ======= マップ構築 =======
  map_.SetBlockSize(kBlockSize);
  map_.RegisterTileDef(1, MapChipField::TileDef{.model = blockModel,
                                                .scale = kBlockSize,
                                                .flags = MapChipField::kSolid});
  map_.LoadFromCSV(StageSelection::GetCsvPathOrDefault());
  map_.BuildInstances();

  // ======= プレイヤー生成 =======
  playerModel = RC::LoadModel("Resources/model/player/player.obj");
  player_ = std::make_unique<Player>();
  player_->Init(playerModel, ctx);
  player_->SetMap(&map_);

  blockInstances_.clear();
  if (const auto *def = map_.FindTileDef(MapChipField::kBlock)) {
    const float s = map_.BlockSize() * (def->scale <= 0.0f ? 1.0f : def->scale);
    const auto &spawns = map_.BlockSpawns();
    blockInstances_.reserve(spawns.size());
    for (const auto &idx : spawns) {
      Transform t{};
      t.scale = {s, s, s};
      t.rotation = {0.0f, 0.0f, 0.0f};
      t.translation = map_.IndexToCenter(idx);
      blockInstances_.push_back(t);
    }
  }

  // ======= コイン生成 =======
  {
    const char *kCoinModelPath = "Resources/model/coin/coin.obj";
    const auto &spawns = map_.CoinSpawns();
    coins_.reserve(spawns.size());

    for (const auto &idx : spawns) {
      int coinModel = RC::LoadModel(kCoinModelPath);

      auto coin = std::make_unique<Coin>();
      coin->Init(coinModel, ctx);

      RC::Vector3 pos = map_.IndexToCenter(idx);
      coin->SetWorldPos(pos);

      coins_.push_back(std::move(coin));
    }

    reachedGoal_ = false;

    // ゴール生成
    {
      const char *kGoalModelPath = "Resources/model/Goal/Goal.obj";

      const auto &spawns = map_.GoalSpawns();
      goals_.reserve(spawns.size());

      for (const auto &idx : spawns) {
        int goalModel = RC::LoadModel(kGoalModelPath);

        auto goal = std::make_unique<Goal>();
        goal->Init(goalModel);

        RC::Vector3 pos = map_.IndexToCenter(idx);
        goal->SetWorldPos(pos);

        goals_.push_back(std::move(goal));
      }
    }
  }

  // ======= スカイドーム生成 =======
  txSphere_ = RC::LoadTex("Resources/skydome.jpg");
  skydomeModel = RC::GenerateSkydomeEx(txSphere_, 100.0f);
  skydomeT_ = RC::GetSkydomeTransformPtr(skydomeModel);
  RC::SetSkydomeColor(skydomeModel, {0.6f, 1.0f, 1.0f, 1.0f});
  skydomeT_->translation = ctx.camera->GetWorldPos();
  skydomeT_->translation.y -= skydomeTranslateY_;
  skydomeT_->rotation.y += skydomeRotateSpeed_;

  // ======= 追従カメラ設定 =======
  if (Transform *pt = RC::GetModelTransformPtr(playerModel)) {
    ctx.camera->SetTarget(pt);
  }

  // ======= マップ境界設定 =======
  {
    const float s = map_.BlockSize();
    const float half = s * 0.5f;

    const float left = -half;
    const float right = (map_.Width() - 1) * s + half;
    const float bottom = -half;
    const float top = (map_.Height() - 1) * s + half;

    const bool enable = (map_.Width() > 0 && map_.Height() > 0);
    ctx.camera->SetFollowBounds(left, right, bottom, top, enable);
  }

  // ======= ポーズ用オーバーレイ初期化 =======
  isPaused_ = false;
  pauseOverlay_.Init(ctx, (float)ctx.app->width, (float)ctx.app->height);
  pauseSprite = RC::LoadSprite("Resources/UI/Pause.png", ctx);
  RC::SetSpriteScreenSize(pauseSprite, 1280, 720);

  keyGuideSprite_ = RC::LoadSprite("Resources/UI/key.png", ctx);
  RC::SetSpriteScreenSize(keyGuideSprite_, 1280, 720);

  // ======= ポストエフェクト初期設定 =======
  if (ctx.postProcess) {
    ctx.postProcess->AddEffect(PostEffectType::Vignette);
    ctx.postProcess->AddEffect(PostEffectType::DepthBasedOutline);
  }
}

void GameScene::OnExit(SceneContext &ctx) {
  if (ctx.postProcess) {
    ctx.postProcess->ClearEffects();
  }

  RC::UnloadModel(playerModel);
  playerModel = -1;
  RC::UnloadModel(blockModel);
  blockModel = -1;
  RC::UnloadSkydome(skydomeModel);
  skydomeModel = -1;
  for (auto &c : coins_) {
    if (!c)
      continue;
    RC::UnloadModel(c->ModelHandle());
  }
  coins_.clear();

  for (auto &g : goals_) {
    if (!g)
      continue;
    RC::UnloadModel(g->ModelHandle());
  }
  goals_.clear();

  // ポーズ用スプライト破棄
  pauseOverlay_.Term();

  if (pauseSprite >= 0) {
    RC::UnloadSprite(pauseSprite);
    pauseSprite = -1;
  }
  if (keyGuideSprite_ >= 0) {
    RC::UnloadSprite(keyGuideSprite_);
    keyGuideSprite_ = -1;
  }
}

void GameScene::Update(SceneManager &sm, SceneContext &ctx) {
#if RC_ENABLE_IMGUI
  ctx.camera->DrawImGui();
  RC::DrawImGui3D(blockModel, "block");
#endif

  // =========================================================
  // ポーズ切り替え（ESC）
  // =========================================================
  if (ctx.input->IsKeyTrigger(DIK_ESCAPE)) {
    isPaused_ = !isPaused_;

    if (isPaused_) {
      // 黒い半透明を貼る
      pauseOverlay_.Start(Fade::Status::kOverlay, 0.0f, pauseOverlayAlpha_);
      if (ctx.postProcess) {
        ctx.postProcess->AddEffect(PostEffectType::BoxFilter);
      }
    } else {
      // 解除（描画しないので Stop は必須じゃないけど、気持ちよく）
      pauseOverlay_.Stop();
      if (ctx.postProcess) {
        ctx.postProcess->RemoveEffect(PostEffectType::BoxFilter);
      }
    }
  }

  // =========================================================
  // ポーズ中：ゲーム更新を止める
  // =========================================================
  if (isPaused_) {
    // overlayは固定表示だけど、呼んでもOK
    pauseOverlay_.Update();

    // カメラは止める（Updateしない）
    // ただし描画用に SetCamera は毎フレーム流しておくと安心
    view_ = ctx.camera->GetView();
    proj_ = ctx.camera->GetProjection();
    RC::SetCamera(view_, proj_, ctx.camera->GetWorldPos());
    return;
  }

  // ===========================================
  // 更新処理
  // ===========================================

  // ======= スカイドーム更新 =======
  if (skydomeT_) {
    // カメラ座標に追従
    skydomeT_->translation = ctx.camera->GetWorldPos();
    // 高さオフセット
    skydomeT_->translation.y -= skydomeTranslateY_;
    // 自転処理
    skydomeT_->rotation.y += skydomeRotateSpeed_;
  }

  // ======= エンティティ更新 =======
  // プレイヤー更新
  player_->Update();
  // コイン更新
  for (auto &c : coins_) {
    if (c)
      c->Update();
  }
  // ゴール更新
  for (auto &g : goals_) {
    if (g)
      g->Update();
  }

  // ======= 落下判定 =======
  if (map_.Height() > 0) {
    const float s = map_.BlockSize();
    const float half = s * 0.5f;
    const float bottom = -half;
    const float fallOutMargin = s * 2.0f;
    const float fallOutY = bottom - fallOutMargin;

    if (player_->GetWorldPos().y < fallOutY) {
      sm.RequestChange("GameOver");
      return;
    }
  }

  // ======= コイン取得判定 =======
  {
    // プレイヤー座標
    const RC::Vector3 p = player_->GetWorldPos();

    for (auto &c : coins_) {
      if (!c)
        continue;
      if (!c->IsAlive() || c->IsCollected())
        continue;

      // コイン座標
      const RC::Vector3 cp = c->GetWorldPos();
      // 2D距離
      const float dx = std::abs(p.x - cp.x);
      const float dy = std::abs(p.y - cp.y);

      // 当たり判定
      if (dx < 0.6f && dy < 0.6f) {
        player_->GetCoin(1);
        c->GetCoin();
      }
    }

    // 消失コイン整理
    for (auto it = coins_.begin(); it != coins_.end();) {
      if (*it && !(*it)->IsAlive()) {
        RC::UnloadModel((*it)->ModelHandle());
        it = coins_.erase(it);
      } else {
        ++it;
      }
    }
  }

  // ゴール到達判定
  if (!reachedGoal_) {
    const RC::Vector3 p = player_->GetWorldPos();

    for (auto &g : goals_) {
      if (!g)
        continue;
      if (g->IsReached())
        continue;

      const RC::Vector3 gp = g->GetWorldPos();
      const float dx = std::abs(p.x - gp.x);
      const float dy = std::abs(p.y - gp.y);

      if (dx < 0.6f && dy < 0.6f) {
        reachedGoal_ = true;
        if (ctx.postProcess) {
          ctx.postProcess->AddEffect(PostEffectType::RadialBlur);
        }
        g->Reach();
        sm.RequestChange("Result");
        break;
      }
    }
  }

  // ======= カメラ更新 =======
  // 固定デルタタイム
  const float dt = 1.0f / 60.0f;
  ctx.camera->Update(dt);

  // ======= ビュー・プロジェクション更新 =======
  view_ = ctx.camera->GetView();
  proj_ = ctx.camera->GetProjection();
  RC::SetCamera(view_, proj_, ctx.camera->GetWorldPos());
}

void GameScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  // ==============
  // 描画処理
  // ==============

  // ======= 3D描画 =======
  RC::PreDraw3D(ctx, cl);
  
  // ======= スカイドーム描画 =======
  RC::DrawSkydome(skydomeModel);

  player_->Draw();
  for (auto &c : coins_) {
    if (c)
      c->Draw();
  }

  for (auto &g : goals_) {
    if (g)
      g->Draw();
  }

  if (!blockInstances_.empty()) {
    RC::DrawModelGlassTwoPassBatchColored(blockModel, blockInstances_,
                                          blockColor_, tx_block);
  }

  // ======= 2D描画準備 =======
  RC::PreDraw2D(ctx, cl);

  // ======= キーガイド表示 =======
  RC::DrawSprite(keyGuideSprite_);

  // ポーズ中だけ黒い半透明を上に貼る
  if (isPaused_) {
    pauseOverlay_.Draw(cl);
    RC::DrawSprite(pauseSprite);
  }
}
