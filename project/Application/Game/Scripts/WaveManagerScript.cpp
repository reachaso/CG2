#include "ECS/ScriptableEntity.h"
#include "ECS/ScriptRegistry.h"
#include "Scene.h"
#include "RenderCommon.h"
#include "Engine/Render/RenderContext.h"
#include "Application/Framework/App.h"
#include "Common/Log/Log.h"

#if RC_ENABLE_IMGUI
#include "imgui/imgui.h"
#endif

#include <string>
#include <vector>

// =====================================================================
// A-03: ウェーブ戦闘の管理
// =====================================================================
// シーンに 1 つだけ置く。既定のエンティティ名は "WaveManager"。
//
// スクリプト同士は型ではなく Entity のタグで通信する。互いのヘッダを
// include せずに済み、片方だけをシーンに置いても壊れないため。
//
//   このエンティティのタグ（＝レールとスポナーが見る「掲示板」）
//     wave_request      : レールが立てる。開始してほしいウェーブ id。受理すると 0 へ戻す
//     wave_active       : 進行中のウェーブ id（Idle なら 0）。スポナーはこれを見て湧かせる
//     wave_cleared_id   : 直近にクリアしたウェーブ id。ウェーブ開始時に 0 へ戻す
//     waves_cleared     : 累計クリア数。レールの分岐条件 tag_at_least から参照できる
//     wave_force_clear  : デバッグ用。進行中のウェーブ id を書くと即クリアする
//
//   スポナー側のタグ（WaveSpawnerScript が書く）
//     wave_spawner      : 1 固定。スポナーの目印
//     spawner_wave_id   : そのスポナーが担当するウェーブ id
//     spawn_done        : 湧かせ終えたウェーブ id
//
//   敵側のタグ（WaveSpawnerScript が生成時に付ける）
//     wave_id           : 所属するウェーブ id
//
// 生存数はエンティティのポインタを保持せず、毎フレーム数え直す。
// Scene::CreateEntity で作った敵はそのフレームは pending にいて GetEntities()
// に現れないため、ポインタで台帳を持つと「湧く前に全滅」と誤判定しやすい。
// 代わりに「担当スポナーが全員 spawn_done を出す」まではクリア判定へ進まない。

/// @brief ウェーブ戦闘の進行を管理するスクリプト
class WaveManagerScript : public ScriptableEntity {
public:
  // --- インスペクタ / JSON から設定する項目 ---
  /// @brief ウェーブ開始を告げてから敵を湧かせるまでの時間（秒）
  float introDuration = 0.8f;
  /// @brief 全滅してからレールを再開するまでの時間（秒）。クリア表示の長さでもある
  float clearDuration = 1.6f;
  /// @brief 画面上部のウェーブ表示を出すか
  bool showHud = true;
  /// @brief クリア時に clear.png を重ねるか
  bool showClearBanner = true;
  /// @brief HUD の下敷きパネルの色
  RC::Vector4 panelColor = {0.05f, 0.09f, 0.14f, 0.78f};
  /// @brief ウェーブ表示のアクセント色
  RC::Vector4 accentColor = {1.0f, 0.55f, 0.15f, 1.0f};

  // -------------------------------------------------------------------
  // 外部から参照する状態
  // -------------------------------------------------------------------
  /// @brief 進行中のウェーブ id（0 なら非戦闘中）
  int ActiveWaveId() const { return activeWaveId_; }
  /// @brief 累計クリア数
  int ClearedCount() const { return wavesCleared_; }

  nlohmann::json Serialize() override {
    return {
        {"introDuration", introDuration},
        {"clearDuration", clearDuration},
        {"showHud", showHud},
        {"showClearBanner", showClearBanner},
        {"panelColor", {panelColor.x, panelColor.y, panelColor.z, panelColor.w}},
        {"accentColor", {accentColor.x, accentColor.y, accentColor.z, accentColor.w}},
    };
  }

  void Deserialize(const nlohmann::json &j) override {
    if (j.contains("introDuration")) introDuration = j["introDuration"].get<float>();
    if (j.contains("clearDuration")) clearDuration = j["clearDuration"].get<float>();
    if (j.contains("showHud")) showHud = j["showHud"].get<bool>();
    if (j.contains("showClearBanner")) showClearBanner = j["showClearBanner"].get<bool>();
    if (j.contains("panelColor")) ReadColor(j["panelColor"], panelColor);
    if (j.contains("accentColor")) ReadColor(j["accentColor"], accentColor);
  }

protected:
  /// @brief ウェーブの進行段階
  enum class State {
    Idle,     ///< 開始要求待ち
    Intro,    ///< 開始を告げている（まだ湧かせない）
    Spawning, ///< スポナーが湧かせている最中
    Fighting, ///< 全滅待ち
    Cleared,  ///< クリア表示中。終わったらレールを再開させる
  };

  void OnCreate() override {
    // ランタイム専用タグの消し込み。
    // タグは Entity::Serialize で JSON に往復するため、プレイ中に保存されたシーンでは
    // waves_cleared などが焼き付いている可能性がある。残っているとリトライ時に
    // 「もうクリア済み」と誤判定してウェーブが素通りする。
    if (Entity *self = GetEntity()) {
      self->ClearTag("wave_request");
      self->ClearTag("wave_force_clear");
      self->SetTag("wave_active", 0);
      self->SetTag("wave_cleared_id", 0);
      self->SetTag("waves_cleared", 0);
      self->SetTag("wave_busy", 0);
    }
    state_ = State::Idle;
    activeWaveId_ = 0;
    wavesCleared_ = 0;
    stateTimer_ = 0.0f;

    SceneContext *ctx = GetSceneContext();
    if (!ctx) return;

    // Sprite2D は 1 ハンドルにつき定数バッファ 1 つで、Draw はコマンドリストへの
    // 記録でしかない。同じハンドルを 1 フレームに複数回描くと GPU 実行時には
    // 最後に書いた座標と UV しか残らないため、桁ごとにハンドルを分ける。
    for (int i = 0; i < kWaveDigits; ++i) {
      waveDigitHandles_[i] = RC::LoadSprite("Resources/UI/number.png", *ctx);
    }
    for (int i = 0; i < kRemainDigits; ++i) {
      remainDigitHandles_[i] = RC::LoadSprite("Resources/UI/number.png", *ctx);
    }
    if (showClearBanner) {
      clearBannerHandle_ = RC::LoadSprite("Resources/UI/clear.png", *ctx);
    }
  }

  void OnDestroy() override {
    for (int i = 0; i < kWaveDigits; ++i) {
      if (waveDigitHandles_[i] >= 0) { RC::UnloadSprite(waveDigitHandles_[i]); waveDigitHandles_[i] = -1; }
    }
    for (int i = 0; i < kRemainDigits; ++i) {
      if (remainDigitHandles_[i] >= 0) { RC::UnloadSprite(remainDigitHandles_[i]); remainDigitHandles_[i] = -1; }
    }
    if (clearBannerHandle_ >= 0) { RC::UnloadSprite(clearBannerHandle_); clearBannerHandle_ = -1; }
  }

  void OnUpdate(float deltaTime) override {
    Entity *self = GetEntity();
    if (!self) return;

    stateTimer_ += deltaTime;

    // デバッグ用の強制クリア（レール側の Skip Wave ボタンからも書かれる）
    const int forced = self->GetTagInt("wave_force_clear", 0);
    if (forced != 0) {
      self->ClearTag("wave_force_clear");
      if (activeWaveId_ != 0 && forced == activeWaveId_) {
        DespawnRemaining(activeWaveId_);
        EnterCleared();
      }
    }

    switch (state_) {
    case State::Idle: {
      const int requested = self->GetTagInt("wave_request", 0);
      if (requested != 0) {
        self->SetTag("wave_request", 0);
        StartWave(requested);
      }
      break;
    }

    case State::Intro:
      if (stateTimer_ >= introDuration) {
        // スポナーはこのタグを見て湧かせるので、Intro が終わってから公開する
        self->SetTag("wave_active", activeWaveId_);
        SetState(State::Spawning);
      }
      break;

    case State::Spawning: {
      int spawnerCount = 0;
      const bool allDone = AreSpawnersDone(activeWaveId_, spawnerCount);
      if (spawnerCount == 0) {
        // 担当スポナーが 1 つも無い＝データの設定漏れ。
        // ここで待ち続けるとレールが永久に止まって原因が見えなくなるため、
        // 警告を出して素通りさせる。
        Log::Print("[WaveManagerScript] no spawner for wave " +
                   std::to_string(activeWaveId_) + ". skipping.");
        EnterCleared();
        break;
      }
      if (allDone) {
        SetState(State::Fighting);
      }
      break;
    }

    case State::Fighting: {
      const int alive = CountAliveEnemies(activeWaveId_);
      remainingEnemies_ = alive;
      if (alive > peakEnemies_) peakEnemies_ = alive;
      if (alive == 0) {
        EnterCleared();
      }
      break;
    }

    case State::Cleared:
      if (stateTimer_ >= clearDuration) {
        // ここで初めてレールへ「終わった」と伝える。
        // クリア表示が出ている間はレールを止めたままにしたいので、
        // 全滅した瞬間ではなく表示が終わってから立てる。
        ++wavesCleared_;
        self->SetTag("wave_cleared_id", activeWaveId_);
        self->SetTag("waves_cleared", wavesCleared_);
        self->SetTag("wave_active", 0);
        activeWaveId_ = 0;
        remainingEnemies_ = 0;
        peakEnemies_ = 0;
        SetState(State::Idle);
      }
      break;
    }
  }

  void OnRender() override {
    if (!showHud) return;
    if (state_ == State::Idle) return;

    float screenW = 1280.0f;
    float screenH = 720.0f;
    auto &rc = RC::GetRenderContext();
    if (rc.Ctx() && rc.Ctx()->app) {
      screenW = static_cast<float>(rc.Ctx()->app->width);
      screenH = static_cast<float>(rc.Ctx()->app->height);
    }

    if (state_ == State::Cleared) {
      DrawClear(screenW, screenH);
    } else {
      DrawWaveBar(screenW, screenH);
    }
  }

public:
#if RC_ENABLE_IMGUI
  void OnImGui() override {
    ImGui::Text("State: %s", StateName(state_));
    ImGui::Text("Active Wave: %d", activeWaveId_);
    ImGui::Text("Remaining: %d / %d", remainingEnemies_, peakEnemies_);
    ImGui::Text("Cleared Count: %d", wavesCleared_);
    ImGui::Text("State Timer: %.2f", stateTimer_);

    ImGui::Separator();
    ImGui::DragFloat("Intro Duration", &introDuration, 0.05f, 0.0f, 5.0f);
    ImGui::DragFloat("Clear Duration", &clearDuration, 0.05f, 0.0f, 8.0f);
    ImGui::Checkbox("Show HUD", &showHud);
    ImGui::Checkbox("Show Clear Banner", &showClearBanner);
    ImGui::ColorEdit4("Panel Color", &panelColor.x);
    ImGui::ColorEdit4("Accent Color", &accentColor.x);

    ImGui::Separator();
    // 実機でウェーブの前後を行き来しながら調整するための操作
    ImGui::DragInt("Debug Wave Id", &debugWaveId_, 1.0f, 1, 99);
    if (ImGui::Button("Request Wave")) {
      if (Entity *self = GetEntity()) self->SetTag("wave_request", debugWaveId_);
    }
    ImGui::SameLine();
    if (ImGui::Button("Force Clear")) {
      if (activeWaveId_ != 0) {
        if (Entity *self = GetEntity()) self->SetTag("wave_force_clear", activeWaveId_);
      }
    }
  }
#endif

private:
  static void ReadColor(const nlohmann::json &j, RC::Vector4 &out) {
    if (j.is_array() && j.size() >= 4) {
      out = {j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>()};
    }
  }

  static const char *StateName(State s) {
    switch (s) {
    case State::Idle:     return "Idle";
    case State::Intro:    return "Intro";
    case State::Spawning: return "Spawning";
    case State::Fighting: return "Fighting";
    case State::Cleared:  return "Cleared";
    }
    return "?";
  }

  void SetState(State s) {
    state_ = s;
    stateTimer_ = 0.0f;
    // Idle 以外は「新しい開始要求をまだ受け付けられない」ことを外へ出す。
    // wave_active だけでは足りない。Intro 中と Cleared 中は湧かせていないので
    // wave_active が 0 のままで、外からは Idle と区別が付かないため、
    // クリア演出中に次のウェーブを要求できてしまう。
    if (Entity *self = GetEntity()) {
      self->SetTag("wave_busy", s != State::Idle ? 1 : 0);
    }
  }

  void StartWave(int waveId) {
    activeWaveId_ = waveId;
    remainingEnemies_ = 0;
    peakEnemies_ = 0;
    if (Entity *self = GetEntity()) {
      // 待ち合わせは wave_cleared_id の「一致」で見るので、開始時に必ず 0 へ戻す。
      // 同じ id のウェーブを 2 度回したときに、前回の値で即クリア扱いになるのを防ぐ。
      self->SetTag("wave_cleared_id", 0);
      self->SetTag("wave_active", 0); // Intro の間はまだ湧かせない
    }
    SetState(State::Intro);
    Log::Print("[WaveManagerScript] wave " + std::to_string(waveId) + " start");
  }

  void EnterCleared() {
    remainingEnemies_ = 0;
    if (Entity *self = GetEntity()) {
      // 湧かせ直しを防ぐため、クリア表示に入った時点で募集を締め切る
      self->SetTag("wave_active", 0);
    }
    SetState(State::Cleared);
    Log::Print("[WaveManagerScript] wave " + std::to_string(activeWaveId_) + " cleared");
  }

  /// @brief 指定ウェーブを担当するスポナーが全員湧かせ終えたか
  /// @param waveId 対象ウェーブ
  /// @param spawnerCountOut 見つかったスポナーの数（0 ならデータの設定漏れ）
  bool AreSpawnersDone(int waveId, int &spawnerCountOut) {
    spawnerCountOut = 0;
    Scene *scene = GetScene();
    if (!scene) return false;

    bool allDone = true;
    for (const auto &e : scene->GetEntities()) {
      if (!e) continue;
      if (e->GetTagInt("wave_spawner", 0) != 1) continue;
      if (e->GetTagInt("spawner_wave_id", 0) != waveId) continue;
      ++spawnerCountOut;
      if (e->GetTagInt("spawn_done", 0) != waveId) allDone = false;
    }
    return allDone;
  }

  /// @brief 指定ウェーブの生存敵数
  /// @details 撃破された敵は enemy_defeated が立ち、2 秒後に実体が消える。
  ///          どちらの状態でも生存には数えない。
  int CountAliveEnemies(int waveId) {
    Scene *scene = GetScene();
    if (!scene) return 0;

    int alive = 0;
    for (const auto &e : scene->GetEntities()) {
      if (!e) continue;
      if (e->GetTagInt("wave_id", 0) != waveId) continue;
      if (e->HasTag("enemy_defeated")) continue;
      ++alive;
    }
    return alive;
  }

  /// @brief 残っている敵を消す（デバッグの強制クリア用）
  /// @details 通常の撃破処理を通すとスコアが加算されてしまうため、実体だけを落とす。
  void DespawnRemaining(int waveId) {
    Scene *scene = GetScene();
    if (!scene) return;
    for (const auto &e : scene->GetEntities()) {
      if (!e) continue;
      if (e->GetTagInt("wave_id", 0) != waveId) continue;
      if (e->HasTag("enemy_defeated")) continue;
      e->Destroy();
    }
  }

  // -------------------------------------------------------------------
  // 描画
  // -------------------------------------------------------------------
  // フォント描画の仕組みがエンジンに無く、使える素材も Resources/UI の数点だけ
  // なので、文字は number.png の数字と図形（DrawBox）で表している。

  /// @brief 画面上部にウェーブ番号と残り数を出す
  void DrawWaveBar(float screenW, float screenH) {
    (void)screenH;

    const float barH = 54.0f;
    const float barY = 12.0f;
    const float barW = 320.0f;
    const float barX = (screenW - barW) * 0.5f;

    // Intro の間はせり出してくるように見せる（0→1 で高さが伸びる）
    float reveal = 1.0f;
    if (state_ == State::Intro && introDuration > 0.0f) {
      reveal = stateTimer_ / introDuration;
      if (reveal > 1.0f) reveal = 1.0f;
    }
    const float h = barH * reveal;
    if (h < 2.0f) return;

    RC::DrawBox({barX, barY}, {barX + barW, barY + h}, panelColor);
    RC::DrawBox({barX, barY}, {barX + barW, barY + h},
                {accentColor.x, accentColor.y, accentColor.z, 0.9f}, kWire);
    // 左端のアクセント帯（「WAVE」の文字素材が無いぶんの目印）
    RC::DrawBox({barX, barY}, {barX + 10.0f, barY + h}, accentColor);

    if (reveal < 1.0f) return;

    // ウェーブ番号
    float x = barX + 26.0f;
    const float digitW = 22.0f;
    const float digitH = 34.0f;
    const float digitY = barY + (barH - digitH) * 0.5f;
    x = DrawNumber(waveDigitHandles_, kWaveDigits, activeWaveId_, x, digitY, digitW, digitH);

    // 残り数のピップ表示。数が多いときは数字へ切り替える。
    if (state_ == State::Fighting) {
      const float pipX = barX + barW - 20.0f;
      const float pipY = barY + barH * 0.5f;
      if (remainingEnemies_ > 0 && remainingEnemies_ <= kMaxPips) {
        const float pipR = 7.0f;
        const float gap = 20.0f;
        for (int i = 0; i < remainingEnemies_; ++i) {
          RC::DrawCircle({pipX - gap * static_cast<float>(i), pipY}, pipR, accentColor);
        }
      } else if (remainingEnemies_ > kMaxPips) {
        const float nx = barX + barW - 20.0f - digitW * 2.0f;
        DrawNumber(remainDigitHandles_, kRemainDigits, remainingEnemies_, nx, digitY,
                   digitW, digitH);
      }
    }
  }

  /// @brief クリア表示（clear.png のフェードイン／アウト）
  void DrawClear(float screenW, float screenH) {
    // 前後 25% をフェードに使い、真ん中は出したままにする
    float alpha = 1.0f;
    if (clearDuration > 0.0f) {
      const float t = stateTimer_ / clearDuration;
      const float fade = 0.25f;
      if (t < fade)             alpha = t / fade;
      else if (t > 1.0f - fade) alpha = (1.0f - t) / fade;
      if (alpha < 0.0f) alpha = 0.0f;
      if (alpha > 1.0f) alpha = 1.0f;
    }
    if (alpha <= 0.01f) return;

    const float panelH = screenH * 0.22f;
    const float panelY = screenH * 0.30f;
    RC::DrawBox({0.0f, panelY}, {screenW, panelY + panelH},
                {0.92f, 0.95f, 1.0f, 0.82f * alpha});

    if (clearBannerHandle_ >= 0) {
      // clear.png は 1280x720 のキャンバス中央に黒文字が置かれた素材なので、
      // 画面全体へ引き伸ばして明るいパネルの上に重ねる
      Transform t{};
      t.scale = {1.0f, 1.0f, 1.0f};
      t.rotation = {0.0f, 0.0f, 0.0f};
      t.translation = {0.0f, 0.0f, 0.0f};
      RC::SetSpriteTransform(clearBannerHandle_, t);
      RC::SetSpriteScreenSize(clearBannerHandle_, screenW, screenH);
      RC::SetSpriteColor(clearBannerHandle_, {1.0f, 1.0f, 1.0f, alpha});
      RC::DrawSprite(clearBannerHandle_);
    }

    // 再進行を示す帯。右へ流れて「この先へ進む」ことを伝える。
    const float lineY = panelY + panelH + 24.0f;
    const float sweep = (clearDuration > 0.0f) ? (stateTimer_ / clearDuration) : 1.0f;
    const float lineW = screenW * 0.35f;
    const float lineX = (screenW - lineW) * 0.5f;
    RC::DrawBox({lineX, lineY}, {lineX + lineW * sweep, lineY + 6.0f},
                {accentColor.x, accentColor.y, accentColor.z, alpha});
  }

  /// @brief number.png の数字で value を描き、次に描ける x を返す
  /// @details number.png は 32x32 の数字を 5 列 x 2 行に並べたシート（上段 0〜4 / 下段 5〜9）。
  ///          ハンドル数を超える桁は描けないので上位から切り捨てる。
  float DrawNumber(const int *handles, int handleCount, int value, float x, float y,
                   float w, float h) {
    if (!handles || handleCount <= 0) return x;
    if (value < 0) value = 0;

    // 桁数がハンドル数を超えると下位桁だけが残り、100 体を "00" と表示してしまう。
    // 誤読させるくらいなら上限に張り付かせたほうがよいので、描ける最大値へ丸める。
    int limit = 1;
    for (int i = 0; i < handleCount && i < kMaxNumberDigits; ++i) limit *= 10;
    if (value >= limit) value = limit - 1;

    int digits[kMaxNumberDigits] = {};
    int count = 0;
    do {
      digits[count++] = value % 10;
      value /= 10;
    } while (value > 0 && count < handleCount && count < kMaxNumberDigits);

    for (int i = 0; i < count; ++i) {
      const int handle = handles[i];
      if (handle < 0) continue;

      const int d = digits[count - 1 - i]; // 上位桁から描く
      const float srcX = static_cast<float>(d % 5) * kDigitSrcSize;
      const float srcY = static_cast<float>(d / 5) * kDigitSrcSize;

      Transform t{};
      t.scale = {1.0f, 1.0f, 1.0f};
      t.rotation = {0.0f, 0.0f, 0.0f};
      t.translation = {x + w * static_cast<float>(i), y, 0.0f};
      RC::SetSpriteTransform(handle, t);
      RC::SetSpriteScreenSize(handle, w, h);
      RC::SetSpriteColor(handle, {1.0f, 1.0f, 1.0f, 1.0f});
      RC::DrawSpriteRect(handle, srcX, srcY, kDigitSrcSize, kDigitSrcSize,
                         kDigitSheetW, kDigitSheetH, 0.5f);
    }
    return x + w * static_cast<float>(count);
  }

  static constexpr float kDigitSrcSize = 32.0f;  ///< number.png の 1 文字あたりのピクセル数
  static constexpr float kDigitSheetW = 160.0f;  ///< number.png の幅
  static constexpr float kDigitSheetH = 64.0f;   ///< number.png の高さ
  static constexpr int kWaveDigits = 2;          ///< ウェーブ番号に使うハンドル数（最大 99）
  static constexpr int kRemainDigits = 2;        ///< 残り数に使うハンドル数
  static constexpr int kMaxNumberDigits = 4;     ///< DrawNumber の内部バッファ長
  static constexpr int kMaxPips = 8;             ///< 丸で残り数を出す上限

  State state_ = State::Idle;
  int activeWaveId_ = 0;
  int wavesCleared_ = 0;
  int remainingEnemies_ = 0;
  int peakEnemies_ = 0;
  float stateTimer_ = 0.0f;

  int waveDigitHandles_[kWaveDigits] = {-1, -1};
  int remainDigitHandles_[kRemainDigits] = {-1, -1};
  int clearBannerHandle_ = -1;

#if RC_ENABLE_IMGUI
  int debugWaveId_ = 1;
#endif
};

REGISTER_SCRIPT(WaveManagerScript)
