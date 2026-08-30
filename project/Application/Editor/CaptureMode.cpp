#include "CaptureMode.h"

#include "Common/EngineConfig.h"

#if RC_ENABLE_IMGUI
#include "imgui/imgui.h"
#endif

#include "../Game/Framework/GameSession.h"
#include "../Game/Scene/Scene.h"
#include "Common/Log/Log.h"
#include "Common/Water/WaterSurface.h"
#include "DebugBridge.h"
#include "Dx12/Dx12Core.h"
#include "ECS/CameraComponent.h"
#include "ECS/Entity.h"
#include "ECS/LightComponent.h"
#include "ECS/NativeScriptComponent.h"
#include "ECS/RigidbodyComponent.h"
#include "ECS/ScriptableEntity.h"
#include "ECS/TransformComponent.h"
#include "ECS/WaterComponent.h"
#include "Input/Input.h"
#include "Render/RenderCommon.h"
#include "Render/RenderContext.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace {

// 撮影モードに入っているか / シーンを再生状態にしてほしいか。
// この2つだけは Release ビルドでも参照するのでガードの外に置く。
bool g_active = false;
bool g_wantsPlaying = false;
/// @brief マウスがゲーム画面の上にあるか（字幕パネルの上ではないか）
bool g_gameHovered = true;

#if RC_ENABLE_IMGUI

float g_stepTime = 0.0f; ///< そのステップに入ってからの経過秒
float g_clock = 0.0f;    ///< 撮影モードに入ってからの累積秒（キャッシュの期限に使う）

// ===========================================================================
// ステップの定義
// ---------------------------------------------------------------------------
// 撮影順に並べる。週ごとにまとめてあるので、
// 「今週ぶんだけ撮る」ときは該当の週から始めればよい。
// ===========================================================================

/// @brief そのステップで何を出すか
enum class Kind {
  Info,        ///< 説明と汎用の実測値だけ
  Week1Scene,  ///< 第1週の題材（別シーン）へ飛ばす
  Pcf,         ///< C-04: PCF を自動で ON/OFF して見比べる
  Reflect,     ///< C-03: 波の反射を自動で ON/OFF して見比べる
  SceneFlow,   ///< A-02: Title → Game → Result → Title を順に飛ばす
  SpriteDigits,///< D-05: スコアの桁が別々に出ることを見せる
  Resources,   ///< D-04: ハンドルが回収されることを数で見せる
  Rail,        ///< T-20: ルート分岐
  Lights,      ///< A-05: レベル由来のライトとカメラ
  Rotation,    ///< 回転単位（度数法／ラジアン）
  Entities,    ///< D-07〜D-10: 保存で複製が増えないこと
  Buoyancy,    ///< C-01/C-02: 落として実測する
  Wave,        ///< A-03: ウェーブ戦闘
  Ship,        ///< T-15: 船の AI と浮力の流用
  Obstacles,   ///< D-01: 障害物のデータ化
};

/// @brief 撮影ステップ1件
struct Step {
  const char *week;   ///< いつの実装か
  const char *id;     ///< タスクID
  const char *title;  ///< 何を証明するのか
  const char *watch;  ///< 何を見れば分かるのか（字幕の本文）
  const char *action; ///< F7 で何が起きるか。nullptr なら F7 は無効
  Kind kind;
};

const Step kSteps[] = {
    // ---------------- 第1週 ----------------
    {"第1週", "PostFX",
     "ポストエフェクト基盤と自作4種",
     "プレイヤーの Y が 0 を跨いだ瞬間に、適用スタックが "
     "LightShaft → Caustics → Underwater → Vignette → RadialBlur → ScreenDroplets "
     "へ 0.5 秒で組み替わる。水上へ戻すと該当パスが外れて描画負荷も消える。"
     "潜る・上がるを2往復して見せること。",
     nullptr, Kind::Info},
    {"第1週", "Anim",
     "スケルタルアニメ改良とボーンソケット・炎パーティクル",
     "CG4 シーンへ移動する。農夫モデルに Idle1 / Idle2 / Walk / Run / Attack の "
     "5状態を組んであり、放置5秒で待機モーションが切り替わる。"
     "0.2秒のクロスフェードで繋がること、右手の炎エミッタがボーンに追従することを見せる。",
     "CG4 シーンへ移動する", Kind::Week1Scene},

    // ---------------- 第2週 ----------------
    {"第2週", "C-04",
     "シャドウマップの PCF 対応",
     "2.5秒ごとに PCF を自動で切り替える。OFF のとき影の輪郭が階段状にギザつき、"
     "ON にすると滑らかになる。影の境界がはっきり出ている物（岩や船）を"
     "画面に大きく入れてから撮ること。",
     "自動切り替えを止める / 再開する", Kind::Pcf},
    {"第2週", "C-03",
     "障害物への波の反射（鏡像法）",
     "3秒ごとに反射を自動で切り替える。ON のとき岩の風上側にだけ波が立ち、"
     "その波が岩から離れる向きへ伝わっていく。OFF では岩の周りが平らなまま。"
     "岩を画面の中央に入れ、風上側が見える角度から撮ること。",
     "自動切り替えを止める / 再開する", Kind::Reflect},
    {"第2週", "A-02",
     "コアゲームループの結合",
     "Title → Game → Result → Title を一周させる。"
     "シーンをまたいでスコアと HP が引き継がれ、Result のランクバーが"
     "下の値と一致することを見せる。ここが今までの動画に写っていなかった。",
     "次のシーンへ進む", Kind::SceneFlow},
    {"第2週", "D-05",
     "スプライトの切り出し矩形が効かない不具合",
     "スコアを 1234567 にして Result へ飛ぶ。各桁が別々の数字として出ていれば直っている。"
     "全桁が同じ数字なら、1ハンドル1フレーム1回の制約に触れている。",
     "スコアを 1234567 にして Result へ飛ぶ", Kind::SpriteDigits},
    {"第2週", "D-04",
     "死亡エンティティのハンドル解放漏れ",
     "基準を記録してから敵を倒す、またはシーンを一周する。"
     "使用中スプライトとライトの数が基準へ戻れば回収できている。"
     "増えたまま戻らないなら解放漏れが残っている。",
     "いまの数を基準として記録する", Kind::Resources},

    // ---------------- 第3週 ----------------
    {"第3週", "T-20",
     "ステージのルート分岐",
     "右キーを押しながら分岐点を通過すると水上ルートへ逸れ、共通のゴールで合流する。"
     "下のウェイポイント番号が飛ぶ瞬間と、直近に成立した分岐条件が証拠になる。"
     "終点では rail_finished が立ち、周回に入らないこと。",
     nullptr, Kind::Rail},
    {"第3週", "A-05",
     "レベルローダーの LIGHT / CAMERA 対応",
     "レベル JSON から読み込んだライトとカメラの内訳を出す。"
     "平行光源・点光源・スポット・エリアの4種と、カメラの画角・クリップ距離が"
     "データ側の値で入っていることが確認できる。",
     nullptr, Kind::Lights},
    {"第3週", "回転単位",
     "回転の単位の食い違いを解消",
     "エンジン内部はラジアン、Blender の出力もラジアン。"
     "度数法で書かれたデータはファイル単位のフラグで吸収する。"
     "下の一覧でラジアン値と度数換算が対応していれば、取り違えは起きていない。",
     nullptr, Kind::Rotation},
    {"第3週", "D-07〜10",
     "既存不具合の修正4件",
     "シーンを保存して開き直してもエンティティ数が増えないこと（保存のたびに"
     "レベル由来の複製が増える問題）。スクリプトを外した物体が"
     "無重力のまま残らないこと。下の数が一定なら直っている。",
     nullptr, Kind::Entities},
    {"第3週", "C-01/02",
     "浮力シミュレーションのパラメータ検証",
     "浮遊物を持ち上げて落とし、着水からの挙動をその場で実測する。"
     "2乗抵抗を入れる前は 12m 落下で余分に 3.94m 沈み、復帰に 6.22 秒かかっていた。"
     "いまは余分な沈み込みがほぼ 0、復帰は 1.4 秒前後で、落下高さを変えても変わらない。",
     "持ち上げて落とす（計測開始）", Kind::Buoyancy},

    // ---------------- 第4週（今週） ----------------
    {"第4週", "A-03",
     "ウェーブ戦闘の構築",
     "ウェーブを開始すると敵が湧き、レールが停止する。"
     "残敵数が減っていき、全滅した瞬間に累計クリア数が増えてレールが再進行する。"
     "この一連が今週いちばん見せたいところ。",
     "ウェーブを開始する", Kind::Wave},
    {"第4週", "T-15",
     "新規敵タイプ「船」の AI と挙動",
     "船は Y と傾きを自分で動かさず浮力に任せているので、波に乗って上下し傾く。"
     "撃沈しても即消滅せず、浸水（flooding）で密度比が上がって沈んでいく。"
     "沈みきるまでの数秒が写るように撮ること。",
     "いちばん近い船を撃沈させる", Kind::Ship},
    {"第4週", "D-01",
     "水面シェーダの障害物座標のデータ化",
     "以前はレンダラ内に座標がハードコードされていた。"
     "いまは岩の Transform から拾っているので、岩を動かすと下の座標も追従する。"
     "描画の反射と当たり判定の水面が同じリストを見ている。",
     nullptr, Kind::Obstacles},
};

constexpr int kStepCount = static_cast<int>(sizeof(kSteps) / sizeof(kSteps[0]));

// ===========================================================================
// 状態
// ===========================================================================

int g_step = 0;
bool g_autoToggle = true; ///< PCF / 反射の自動切り替え
bool g_toggleOn = false;  ///< 自動切り替えの現在の側
bool g_showHelp = true;   ///< 下部の操作ヒント（撮影時は消せる）

/// @brief A-02 のシーン送りで次に飛ぶ先
int g_sceneFlowIndex = 0;
const char *const kSceneFlow[] = {"Title", "Game", "Result", "Title"};
constexpr int kSceneFlowCount =
    static_cast<int>(sizeof(kSceneFlow) / sizeof(kSceneFlow[0]));

/// @brief D-04 の基準値
struct Baseline {
  bool taken = false;
  size_t spriteInUse = 0;
  int pointLights = 0;
  int spotLights = 0;
  int areaLights = 0;
  size_t entities = 0;
};
Baseline g_baseline;

/// @brief 浮力の落下計測
/// @details 「12m 落下 → 余分な沈み込み 0.00m → 1.38 秒で復帰」という
///          報告書の数値を、実機でその場で出し直すための計測。
struct DropMeasure {
  enum class Phase { Idle, Falling, Submerged, Done };
  Phase phase = Phase::Idle;
  uint32_t targetId = 0;   ///< 計測対象のエンティティID
  float dropHeight = 12.0f;///< 静水面から何 m 上へ持ち上げるか
  float halfHeight = 1.0f; ///< 対象の半分の高さ（完全水没の判定に使う）
  float startY = 0.0f;
  float waterY = 0.0f;
  float deepest = 0.0f;    ///< 完全水没からさらに沈んだ最大量(m)
  float submergedTime = 0.0f;
  float recoverTime = 0.0f;///< 完全水没してから水面へ戻るまで(s)
  bool everFullySubmerged = false;
};
DropMeasure g_drop;

// ===========================================================================
// 小道具
// ===========================================================================

/// @brief 指定した型名のスクリプトを持つエンティティを集める
/// @details ゲームプレイのスクリプトは .cpp 内ローカル型でヘッダが無いため、
///          型では引けない。登録名（文字列）で絞り込む。
std::vector<Entity *> FindWithScript(Scene *scene, const char *typeName) {
  std::vector<Entity *> found;
  if (!scene || !typeName) return found;
  for (const auto &e : scene->GetEntities()) {
    if (!e) continue;
    auto *nsc = e->GetComponent<NativeScriptComponent>();
    if (!nsc) continue;
    for (const auto &entry : nsc->scripts) {
      if (entry.scriptTypeName.find(typeName) != std::string::npos) {
        found.push_back(e.get());
        break;
      }
    }
  }
  return found;
}

/// @brief エンティティから指定した型名のスクリプト実体を引く
ScriptableEntity *GetScript(Entity *e, const char *typeName) {
  if (!e || !typeName) return nullptr;
  auto *nsc = e->GetComponent<NativeScriptComponent>();
  if (!nsc) return nullptr;
  for (const auto &entry : nsc->scripts) {
    if (entry.scriptTypeName.find(typeName) != std::string::npos) {
      return entry.instance;
    }
  }
  return nullptr;
}

/// @brief 指定タグを持つ最初のエンティティ
Entity *FindWithTag(Scene *scene, const char *tag) {
  if (!scene || !tag) return nullptr;
  for (const auto &e : scene->GetEntities()) {
    if (e && e->HasTag(tag)) return e.get();
  }
  return nullptr;
}

/// @brief スクリプトの設定値を JSON で取り出す（型が見えないための迂回）
nlohmann::json SafeSerialize(ScriptableEntity *s) {
  if (!s) return nlohmann::json::object();
  try {
    return s->Serialize();
  } catch (...) {
    return nlohmann::json::object();
  }
}

/// @brief SafeSerialize の結果を 0.25 秒だけ使い回す
/// @param e 対象のエンティティ
/// @param typeName スクリプトの登録名
/// @details 字幕は毎フレーム描くが、レールのウェイポイント一覧のように
///          そこそこ大きい JSON を 60fps で組み直すと、
///          録画にフレーム落ちとして写る可能性がある。
/// @note キーはエンティティIDと型名の両方で見る。IDだけにすると、
///       1つのエンティティに複数のスクリプトが載っている場合（船＝
///       ShipEnemyScript ＋ BuoyancyScript）に別のスクリプトの
///       JSON を返してしまう。
/// @warning typeName はポインタで比較している。呼び出しは文字列リテラルに
///          限ること。std::string::c_str() を渡すと、解放後に同じアドレスが
///          再利用されたときに別の型でキャッシュヒットする。
const nlohmann::json &CachedSerialize(Entity *e, const char *typeName) {
  static nlohmann::json cache = nlohmann::json::object();
  static uint32_t cachedId = 0;
  static const char *cachedType = nullptr;
  static float nextRefresh = -1.0f;

  const uint32_t id = e ? e->GetId() : 0u;
  if (cachedId != id || cachedType != typeName || g_clock >= nextRefresh) {
    cache = SafeSerialize(GetScript(e, typeName));
    cachedId = id;
    cachedType = typeName;
    nextRefresh = g_clock + 0.25f;
  }
  return cache;
}

/// @brief シーンから水面の波パラメータを組み立てる
/// @return 水面エンティティが見つかれば true
bool BuildWaterParams(Scene *scene, RC::WaterWaveParams &out) {
  if (!scene) return false;
  for (const auto &e : scene->GetEntities()) {
    if (!e) continue;
    auto *w = e->GetComponent<WaterComponent>();
    if (!w) continue;
    out.waveHeight = w->waveHeight;
    out.waveSpeed = w->waveSpeed;
    out.waveFreq = w->waveFreq;
    out.waveHeight2 = w->waveHeight2;
    out.waveSpeed2 = w->waveSpeed2;
    out.waveFreq2 = w->waveFreq2;
    out.waveSteepness = w->waveSteepness;
    if (auto *tr = e->GetComponent<TransformComponent>()) {
      out.baseHeight = tr->position.y;
    }
    return true;
  }
  return false;
}

/// @brief 指定 XZ の水面の高さを、描画と同じ式で求める
float SampleWaterHeight(Scene *scene, float x, float z) {
  RC::WaterWaveParams params;
  if (!BuildWaterParams(scene, params)) return 0.0f;

  RC::Vector4 raw[RC::WaterSurface::kMaxObstacles] = {};
  const int count =
      RC::GetWaterObstacles(raw, RC::WaterSurface::kMaxObstacles);
  RC::WaterObstacle obstacles[RC::WaterSurface::kMaxObstacles] = {};
  for (int i = 0; i < count; ++i) {
    obstacles[i].pos = {raw[i].x, raw[i].y, raw[i].z};
    obstacles[i].radius = raw[i].w;
  }

  return RC::WaterSurface::SampleHeight(
      params, RC::GetWaterTime(), x, z, obstacles, count,
      RC::GetWaterReflectStrength(), RC::GetWaterReflectRange());
}

/// @brief 浮力スクリプトが喫水の基準にしているのと同じ水面高さを求める
/// @details BuoyancyScript は useFourSamples が立っていると前後左右4点の平均を
///          基準にしている。ここで中心1点だけを見ると波高（0.3m 級）ぶん
///          ずれるため、「余分な沈み込みがほぼ 0」という 0.1m オーダーの主張を
///          測るには誤差が大きすぎる。同じ取り方に揃える。
/// @param yaw 対象の Y 軸回転（ラジアン）。サンプル点はこれに合わせて回す
float SampleWaterHeightLikeBuoyancy(Scene *scene, float x, float z, float yaw,
                                    const nlohmann::json &params) {
  const float fwd = params.value("sampleRadius", 1.0f);
  // 条件も BuoyancyScript と同じにする（useFourSamples && sampleRadius > 0）
  if (!params.value("useFourSamples", true) || fwd <= 0.0f) {
    return SampleWaterHeight(scene, x, z);
  }
  float side = params.value("sampleRadiusSide", 0.0f);
  if (side <= 0.0f) side = fwd;

  // サンプル点は世界軸ではなくエンティティの向きに合わせて回す。
  // 世界軸のまま取ると、船のように向きが変わる物体で前後と左右が入れ替わり、
  // sampleRadius と sampleRadiusSide が違う値のときに別の場所を測ることになる。
  const float cy = std::cos(yaw);
  const float sy = std::sin(yaw);
  const float fx = sy * fwd,  fz = cy * fwd;   // 船首方向
  const float rx = cy * side, rz = -sy * side; // 右舷方向

  return (SampleWaterHeight(scene, x + fx, z + fz) +
          SampleWaterHeight(scene, x - fx, z - fz) +
          SampleWaterHeight(scene, x + rx, z + rz) +
          SampleWaterHeight(scene, x - rx, z - rz)) *
         0.25f;
}

// ---------------------------------------------------------------------------
// 字幕の描画部品
// ---------------------------------------------------------------------------

const ImVec4 kAccent = ImVec4(0.45f, 0.78f, 1.00f, 1.0f);
const ImVec4 kOk = ImVec4(0.40f, 0.88f, 0.48f, 1.0f);
const ImVec4 kWarn = ImVec4(0.98f, 0.72f, 0.32f, 1.0f);
const ImVec4 kDim = ImVec4(0.72f, 0.76f, 0.82f, 1.0f);

/// @brief 指定倍率でフォントを大きくする（1.92 で SetWindowFontScale は非推奨）
void PushScaledFont(float factor) {
  ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * factor);
}

/// @brief 値のラベル行（左に項目名、右に値）
/// @details 桁位置はフォントサイズに追随させる。固定ピクセルにすると、
///          字幕用に文字を大きくしたときに長いラベルが桁位置を追い越し、
///          SameLine がカーソルを戻して文字が重なる。
void Row(const char *label, const char *fmt, ...) IM_FMTARGS(2);
void Row(const char *label, const char *fmt, ...) {
  ImGui::TextColored(kDim, "%s", label);
  // SameLine のオフセットはウィンドウ外枠からの距離なので、
  // ラベル幅に左パディングを足さないと 1 文字ぶんの余白が取れない。
  const float column = ImGui::GetFontSize() * 13.0f;
  const float used = ImGui::GetStyle().WindowPadding.x +
                     ImGui::GetItemRectSize().x + ImGui::GetFontSize();
  ImGui::SameLine((std::max)(column, used));
  va_list args;
  va_start(args, fmt);
  ImGui::TextV(fmt, args);
  va_end(args);
}

/// @brief A/B 比較の現在の側を大きく出す
void BigState(bool on, const char *onText, const char *offText) {
  PushScaledFont(1.9f);
  ImGui::TextColored(on ? kOk : kWarn, "%s", on ? onText : offText);
  ImGui::PopFont();
}

// ---------------------------------------------------------------------------
// ステップごとの実測値表示
// ---------------------------------------------------------------------------

void LiveInfo(Scene *scene) {
  Entity *player = FindWithTag(scene, "is_underwater");
  if (!player) {
    const auto found = FindWithScript(scene, "RailShooterController");
    if (!found.empty()) player = found.front();
  }
  if (!player) {
    ImGui::TextColored(kDim, "（プレイヤーが見つかりません）");
    return;
  }
  const bool underwater = player->GetTagInt("is_underwater", 0) != 0;
  BigState(underwater, "水中（6種のスタックが有効）", "水上（該当パスは外してある）");
  if (auto *tr = player->GetComponent<TransformComponent>()) {
    Row("プレイヤーの Y", "%.2f  （0 を跨いだ瞬間に組み替わる）", tr->position.y);
  }
}

void LiveWeek1Scene(Scene *scene) {
  ImGui::TextColored(kDim, "現在のシーン");
  ImGui::SameLine(190.0f);
  ImGui::Text("%s", scene ? scene->Name() : "(なし)");
  if (!DebugBridge::CanRequestScene()) {
    ImGui::TextColored(kWarn, "シーン切り替えの口が未登録です。上部メニューの Scene から移動してください。");
  }
}

void LivePcf(float deltaTime) {
  auto &dbg = RC::GetRenderContext().ShadowDebug();
  dbg.enabled = true; // このステップのあいだは上書きを有効にする

  if (g_autoToggle) {
    constexpr float kPeriod = 2.5f;
    g_toggleOn = std::fmod(g_stepTime, kPeriod * 2.0f) >= kPeriod;
  }
  dbg.pcfRadius = g_toggleOn ? 1.0f : 0.0f;

  BigState(g_toggleOn, "PCF ON  ─ 3x3 タップ（ハードウェア比較込みで実質 6x6）",
           "PCF OFF ─ 1 タップ（従来）");
  Row("pcfRadius", "%.2f", dbg.pcfRadius);
  Row("シャドウマップ", "%u x %u", RC::GetRenderContext().GetShadowMap().GetWidth(),
      RC::GetRenderContext().GetShadowMap().GetHeight());
  ImGui::TextColored(kDim, "%s",
                     g_autoToggle ? "2.5 秒ごとに自動で切り替えています（F7 で停止）"
                                  : "自動切り替えは停止中（F7 で再開）");
  (void)deltaTime;
}

void LiveReflect() {
  if (g_autoToggle) {
    constexpr float kPeriod = 3.0f;
    g_toggleOn = std::fmod(g_stepTime, kPeriod * 2.0f) >= kPeriod;
  }
  // 0 は「無効」ではなく既定値 1.0 へのフォールバックとして解釈されるため（D-14）、
  // OFF 側は 0 ではなく十分小さい正の値を使う。
  RC::SetWaterReflectOverride(true, g_toggleOn ? 1.0f : 0.001f,
                              RC::GetWaterReflectRange());

  BigState(g_toggleOn, "反射 ON  ─ 鏡像法（入射波の位相を引き継ぐ）",
           "反射 OFF ─ 岩の周りは平らなまま");

  float strength = 0.0f, range = 0.0f;
  RC::GetWaterReflectOverride(&strength, &range);
  Row("反射の強さ", "%.3f", strength);
  Row("到達範囲", "%.2f （障害物半径に対する倍率）", range);

  RC::Vector4 obstacles[RC::WaterSurface::kMaxObstacles] = {};
  const int count = RC::GetWaterObstacles(obstacles, RC::WaterSurface::kMaxObstacles);
  Row("反射させる障害物", "%d 個", count);
  if (count == 0) {
    ImGui::TextColored(kWarn, "障害物が 0 件です。岩が水面をまたいでいるか確認してください。");
  }
  ImGui::TextColored(kDim, "%s",
                     g_autoToggle ? "3 秒ごとに自動で切り替えています（F7 で停止）"
                                  : "自動切り替えは停止中（F7 で再開）");
}

void LiveSceneFlow(Scene *scene) {
  auto &s = GameSession::Get();
  const char *outcome = "プレイ中";
  switch (s.GetOutcome()) {
  case GameSession::Outcome::Cleared: outcome = "クリア"; break;
  case GameSession::Outcome::GameOver: outcome = "ゲームオーバー"; break;
  default: break;
  }

  PushScaledFont(1.6f);
  ImGui::TextColored(kAccent, "%s", scene ? scene->Name() : "(なし)");
  ImGui::PopFont();

  Row("スコア", "%d", s.Score());
  Row("HP", "%d / %d", s.PlayerHp(), s.PlayerMaxHp());
  Row("経過時間", "%.2f 秒", s.ElapsedTime());
  Row("決着", "%s", outcome);
  Row("評価ランク", "%c  （バーの本数 S=4 / A=3 / B=2 / C=1）", s.Rank());
  Row("プレイ回数", "%d", s.PlayCount());

  ImGui::TextColored(kDim, "F7 で次のシーンへ： %s",
                     kSceneFlow[g_sceneFlowIndex % kSceneFlowCount]);
}

void LiveSpriteDigits() {
  auto &s = GameSession::Get();
  Row("スコア", "%d", s.Score());
  Row("期待される見え方", "%s", "1 2 3 4 5 6 7 が桁ごとに違う数字で出る");
  ImGui::TextColored(kDim,
                     "1 ハンドルにつき 1 フレーム 1 回しか描けないため、"
                     "桁ごとに別ハンドルを確保している。全桁が同じ数字なら"
                     "そこを踏んでいる。");
}

void LiveResources(Scene *scene) {
  auto &ctx = RC::GetRenderContext();
  const size_t inUse = ctx.Sprites().InUseCount();
  const size_t allocated = ctx.Sprites().AllocatedCount();
  const int pt = RC::GetActivePointLightCount();
  const int sp = RC::GetActiveSpotLightCount();
  const int ar = RC::GetActiveAreaLightCount();
  const size_t ents = scene ? scene->GetEntities().size() : 0u;

  auto diff = [](long long now, long long base, bool has) {
    if (!has) { ImGui::SameLine(); ImGui::TextColored(kDim, "（基準なし）"); return; }
    const long long d = now - base;
    ImGui::SameLine();
    if (d > 0) ImGui::TextColored(kWarn, "  基準比 +%lld", d);
    else if (d < 0) ImGui::TextColored(kAccent, "  基準比 %lld", d);
    else ImGui::TextColored(kOk, "  基準どおり");
  };

  Row("使用中スプライト", "%lld", static_cast<long long>(inUse));
  diff(static_cast<long long>(inUse), static_cast<long long>(g_baseline.spriteInUse), g_baseline.taken);
  Row("点光源 / スポット / エリア", "%d / %d / %d", pt, sp, ar);
  diff(pt + sp + ar, g_baseline.pointLights + g_baseline.spotLights + g_baseline.areaLights,
       g_baseline.taken);
  Row("エンティティ", "%lld", static_cast<long long>(ents));
  diff(static_cast<long long>(ents), static_cast<long long>(g_baseline.entities), g_baseline.taken);
  Row("累計確保スプライト", "%lld （スロット再利用なしなので増える一方）",
      static_cast<long long>(allocated));

  ImGui::TextColored(kDim, "F7 でいまの数を基準として記録します。");
}

void LiveRail(Scene *scene) {
  Entity *rail = FindWithTag(scene, "has_rail");
  if (!rail) {
    ImGui::TextColored(kWarn, "このシーンにレールがありません（has_rail タグなし）。");
    return;
  }

  const nlohmann::json &j = CachedSerialize(rail, "RailMovementScript");
  const bool finished = rail->GetTagInt("rail_finished", 0) != 0;
  const int wp = rail->GetTagInt("rail_wp", 0);
  const int branchTo = rail->GetTagInt("rail_branch_to", -1);
  const int branchCountTaken = rail->GetTagInt("rail_branch_count", 0);

  // いま向かっているウェイポイント番号を大きく出す。
  // 分岐した瞬間はここが「+1」ではなく飛ぶので、それが T-20 の証拠になる。
  PushScaledFont(2.2f);
  ImGui::TextColored(finished ? kOk : kAccent, "WP %d %s", wp,
                     finished ? " ─ 終点に到達（rail_finished）" : "");
  ImGui::PopFont();

  int total = 0, endCount = 0, branchCount = 0;
  if (j.contains("waypoints") && j["waypoints"].is_array()) {
    total = static_cast<int>(j["waypoints"].size());
    for (const auto &w : j["waypoints"]) {
      if (w.contains("isEnd") && w["isEnd"].get<bool>()) ++endCount;
      if (w.contains("branches") && w["branches"].is_array()) {
        branchCount += static_cast<int>(w["branches"].size());
      }
    }
  }
  Row("ウェイポイント総数", "%d", total);
  Row("置いてある分岐", "%d 本", branchCount);
  Row("終点フラグ", "%d 個  %s", endCount,
      endCount == 0 ? "← 0 だと合流後に周回します" : "");
  Row("分岐が成立した回数", "%d", branchCountTaken);
  Row("直近の分岐先", "%s",
      branchTo < 0 ? "直進（分岐していない）" : std::to_string(branchTo).c_str());

  ImGui::Spacing();
  ImGui::TextColored(kDim,
                     "分岐点の手前で右キーを押しながら通過すると水上ルートへ逸れます。"
                     "WP 番号が連番ではなく飛んだ瞬間と、そのあと共通のゴールへ"
                     "戻ってくるところを続けて写してください。");
}

void LiveLights(Scene *scene) {
  if (!scene) return;
  int dir = 0, pt = 0, sp = 0, ar = 0, cam = 0;
  const CameraComponent *mainCam = nullptr;
  for (const auto &e : scene->GetEntities()) {
    if (!e) continue;
    if (e->GetComponent<DirectionalLightComponent>()) ++dir;
    if (e->GetComponent<PointLightComponent>()) ++pt;
    if (e->GetComponent<SpotLightComponent>()) ++sp;
    if (e->GetComponent<AreaLightComponent>()) ++ar;
    if (auto *c = e->GetComponent<CameraComponent>()) {
      ++cam;
      if (c->isMain || !mainCam) mainCam = c;
    }
  }

  Row("平行光源", "%d", dir);
  Row("点光源", "%d", pt);
  Row("スポットライト", "%d", sp);
  Row("エリアライト", "%d", ar);
  Row("カメラ", "%d", cam);
  if (mainCam) {
    Row("画角 fovY", "%.3f rad （%.1f 度）", mainCam->fovY,
        mainCam->fovY * 57.2957795f);
    Row("クリップ", "near %.2f / far %.1f", mainCam->nearZ, mainCam->farZ);
  }
  ImGui::TextColored(kDim,
                     "Blender 側の energy・円錐の全開き角・エリアの辺の長さ・"
                     "clip_start / clip_end を吸収して読み込んでいます。");
}

void LiveRotation(Scene *scene) {
  if (!scene) return;
  ImGui::TextColored(kDim,
                     "エンジン内部の回転は一貫してラジアン。"
                     "度数法で書かれたレベルはファイル単位のフラグで吸収する。"
                     "下の2列が対応していれば取り違えは起きていない。");
  ImGui::Spacing();

  int shown = 0;
  for (const auto &e : scene->GetEntities()) {
    if (!e || shown >= 6) continue;
    auto *tr = e->GetComponent<TransformComponent>();
    if (!tr) continue;
    const float mag = std::fabs(tr->rotation.x) + std::fabs(tr->rotation.y) +
                      std::fabs(tr->rotation.z);
    if (mag < 0.001f) continue;
    ImGui::Text("%-18s rad(%6.3f, %6.3f, %6.3f)   deg(%6.1f, %6.1f, %6.1f)",
                e->GetName().c_str(), tr->rotation.x, tr->rotation.y,
                tr->rotation.z, tr->rotation.x * 57.2957795f,
                tr->rotation.y * 57.2957795f, tr->rotation.z * 57.2957795f);
    ++shown;
  }
  if (shown == 0) {
    ImGui::TextColored(kDim, "（回転が入っているエンティティがありません）");
  } else {
    ImGui::Spacing();
    ImGui::TextColored(kWarn,
                       "rad 側が 10 を超えていたら、度数法のデータをラジアンとして"
                       "読んでいる可能性があります（45 度 = 0.785 rad）。");
  }
}

void LiveEntities(Scene *scene) {
  const size_t ents = scene ? scene->GetEntities().size() : 0u;
  PushScaledFont(1.9f);
  ImGui::Text("エンティティ %lld 個", static_cast<long long>(ents));
  ImGui::PopFont();
  ImGui::TextColored(kDim,
                     "シーンを保存して開き直しても、この数が増えないことを見せる。"
                     "以前はレベルデータ由来のエンティティも書き出していたため、"
                     "保存のたびに複製が増えていた（D-08）。");
  ImGui::Spacing();
  ImGui::TextColored(kDim,
                     "また、スクリプトが他コンポーネントの設定値を書き換えないように"
                     "改めたので（D-09）、スクリプトを外した物体が無重力のまま"
                     "残ることも無くなっている。");
}

void LiveBuoyancy(Scene *scene, float deltaTime) {
  auto floaters = FindWithScript(scene, "BuoyancyScript");
  if (floaters.empty()) {
    ImGui::TextColored(kWarn, "浮遊物（BuoyancyScript）が見つかりません。");
    return;
  }

  // 計測対象を決める（未設定なら先頭）
  Entity *target = nullptr;
  for (auto *e : floaters) {
    if (e->GetId() == g_drop.targetId) { target = e; break; }
  }
  if (!target) {
    target = floaters.front();
    g_drop.targetId = target->GetId();
    g_drop.phase = DropMeasure::Phase::Idle;
  }

  auto *tr = target->GetComponent<TransformComponent>();
  if (!tr) return;

  const nlohmann::json &params = CachedSerialize(target, "BuoyancyScript");
  g_drop.halfHeight = params.value("halfHeight", 1.0f);
  const float density = params.value("densityRatio", 0.45f);
  const float linearDrag = params.value("linearDrag", 5.5f);
  const float quadDrag = params.value("quadraticDrag", 1.0f);

  const float waterY = SampleWaterHeightLikeBuoyancy(
      scene, tr->position.x, tr->position.z, tr->rotation.y, params);
  const float topY = tr->position.y + g_drop.halfHeight;

  // --- 計測の進行 --------------------------------------------------------
  switch (g_drop.phase) {
  case DropMeasure::Phase::Falling:
    g_drop.submergedTime += deltaTime;
    if (topY < waterY) {
      g_drop.phase = DropMeasure::Phase::Submerged;
      g_drop.everFullySubmerged = true;
      g_drop.submergedTime = 0.0f;
      g_drop.deepest = 0.0f;
    } else if (g_drop.submergedTime > 8.0f) {
      g_drop.phase = DropMeasure::Phase::Done; // 水面に届かなかった
    }
    break;
  case DropMeasure::Phase::Submerged: {
    g_drop.submergedTime += deltaTime;
    g_drop.deepest = (std::max)(g_drop.deepest, waterY - topY);
    if (topY >= waterY) {
      g_drop.recoverTime = g_drop.submergedTime;
      g_drop.phase = DropMeasure::Phase::Done;
    } else if (g_drop.submergedTime > 20.0f) {
      g_drop.recoverTime = g_drop.submergedTime;
      g_drop.phase = DropMeasure::Phase::Done; // 戻ってこない
    }
    break;
  }
  default:
    break;
  }

  // --- 表示 --------------------------------------------------------------
  ImGui::TextColored(kAccent, "計測対象: %s", target->GetName().c_str());
  Row("密度比", "%.2f  （水面上に %.0f%% 出て釣り合う）", density,
      (1.0f - density) * 100.0f);
  Row("線形抵抗 / 2乗抵抗", "%.2f / %.2f", linearDrag, quadDrag);

  // 周期と減衰比（BuoyancyScript と同じ式）
  const float full = (std::max)(g_drop.halfHeight * 2.0f, 1e-3f);
  const float dens = (std::max)(density, 1e-3f);
  const float gravity = params.value("gravity", 9.81f);
  const float omega = std::sqrt((std::max)(gravity, 0.0f) / (dens * full));
  const float period = (omega > 1e-4f) ? (6.28318531f / omega) : 0.0f;
  const float zeta = (omega > 1e-4f) ? (linearDrag * dens) / (2.0f * omega) : 0.0f;
  Row("上下動の周期", "%.2f 秒", period);
  Row("減衰比", "%.2f  %s", zeta,
      (zeta < 0.15f)   ? "ぷかぷか跳ね続ける"
      : (zeta < 0.55f) ? "2〜3回跳ねて収まる（狙い）"
      : (zeta < 1.0f)  ? "ほとんど跳ねない"
                       : "跳ねずにじわっと戻る");

  ImGui::Separator();
  Row("水面の高さ", "%.2f", waterY);
  Row("物体の上端", "%.2f", topY);

  ImGui::Spacing();
  ImGui::SetNextItemWidth(220.0f);
  ImGui::SliderFloat("落下高さ (m)", &g_drop.dropHeight, 2.0f, 40.0f, "%.0f");

  switch (g_drop.phase) {
  case DropMeasure::Phase::Idle:
    ImGui::TextColored(kDim, "F7 で %.0f m 上へ持ち上げて落とします。",
                       g_drop.dropHeight);
    break;
  case DropMeasure::Phase::Falling:
    PushScaledFont(1.6f);
    ImGui::TextColored(kWarn, "落下中… %.2f 秒", g_drop.submergedTime);
    ImGui::PopFont();
    break;
  case DropMeasure::Phase::Submerged:
    PushScaledFont(1.6f);
    ImGui::TextColored(kWarn, "水没中… %.2f 秒 / 深さ %.2f m",
                       g_drop.submergedTime, g_drop.deepest);
    ImGui::PopFont();
    break;
  case DropMeasure::Phase::Done: {
    PushScaledFont(1.9f);
    ImGui::TextColored(kOk, "計測結果");
    ImGui::PopFont();
    Row("落下高さ", "%.1f m （静水面からの持ち上げ量）",
        g_drop.startY - g_drop.waterY);
    if (!g_drop.everFullySubmerged) {
      ImGui::TextColored(kWarn,
                         "一度も完全水没しませんでした。落下高さを上げるか、"
                         "密度比が小さすぎないか確認してください。");
      break;
    }
    Row("余分な沈み込み", "%.2f m  （完全水没からさらに沈んだ量）", g_drop.deepest);
    Row("水面への復帰", "%.2f 秒", g_drop.recoverTime);
    ImGui::Spacing();
    ImGui::TextColored(kDim,
                       "2乗抵抗を入れる前は 12m 落下で 3.94m 余分に沈み、"
                       "復帰に 6.22 秒かかっていた（落下高さを上げるほど悪化した）。"
                       "落下高さを変えても結果がほぼ変わらないことも見せること。");
    break;
  }
  }
}

void LiveWave(Scene *scene) {
  if (!scene) return;
  Entity *mgr = scene->FindEntityByName("WaveManager");
  if (!mgr) {
    auto found = FindWithScript(scene, "WaveManagerScript");
    if (!found.empty()) mgr = found.front();
  }
  if (!mgr) {
    ImGui::TextColored(kWarn, "WaveManager が見つかりません。");
    return;
  }

  const int active = mgr->GetTagInt("wave_active", 0);
  const int cleared = mgr->GetTagInt("waves_cleared", 0);

  const bool busy = mgr->GetTagInt("wave_busy", 0) != 0;
  if (active == 0) {
    // Intro / クリア演出の途中は wave_active が 0 に戻っているので、
    // ここで「F7 でウェーブ開始」と促すと演出中に次を始めてしまう。
    BigState(false, "",
             busy ? "ウェーブの演出中（開始要求は受け付けません）"
                  : "戦闘していない（F7 でウェーブ開始）");
  } else {
    int alive = 0, total = 0;
    for (const auto &e : scene->GetEntities()) {
      if (!e || e->GetTagInt("wave_id", 0) != active) continue;
      ++total;
      if (!e->HasTag("enemy_defeated")) ++alive;
    }
    PushScaledFont(1.9f);
    ImGui::TextColored(alive == 0 ? kOk : kWarn, "WAVE %d ─ 残り %d / %d", active,
                       alive, total);
    ImGui::PopFont();
    ImGui::ProgressBar(total > 0 ? 1.0f - static_cast<float>(alive) / total : 0.0f,
                       ImVec2(-1.0f, 0.0f));
  }

  Row("累計クリア数", "%d", cleared);
  Row("直近クリア", "%d", mgr->GetTagInt("wave_cleared_id", 0));

  if (Entity *rail = FindWithTag(scene, "has_rail")) {
    Row("レールの現在地", "WP %d", rail->GetTagInt("rail_wp", 0));
    ImGui::TextColored(kDim,
                       "全滅した瞬間に累計クリア数が増え、停止していたレールが"
                       "再進行します（上の WP 番号が動き出す）。そこまで写すこと。");
  }
}

void LiveShip(Scene *scene) {
  auto ships = FindWithScript(scene, "ShipEnemyScript");
  PushScaledFont(1.6f);
  ImGui::TextColored(ships.empty() ? kWarn : kAccent, "船 %d 隻",
                     static_cast<int>(ships.size()));
  ImGui::PopFont();

  if (ships.empty()) {
    ImGui::TextColored(kDim,
                       "船はウェーブスポナーから湧きます。先に A-03 のステップで"
                       "該当ウェーブを開始してください。");
    return;
  }

  for (auto *e : ships) {
    const bool buoyant = GetScript(e, "BuoyancyScript") != nullptr;
    ImGui::Text("%-16s HP %d / %d   撃沈=%d 浸水=%d %s", e->GetName().c_str(),
                e->GetTagInt("current_hp", 0), e->GetTagInt("max_hp", 0),
                e->GetTagInt("enemy_defeated", 0), e->GetTagInt("flooding", 0),
                buoyant ? "" : " ← 浮力なし");
  }
  ImGui::Spacing();
  ImGui::TextColored(kDim,
                     "撃沈すると flooding が 1 になり、密度比が floodedDensityRatio へ"
                     "寄っていって沈みます。沈みきるまで写すこと。F7 で撃沈させられます。");
}

void LiveObstacles(Scene *scene) {
  RC::Vector4 obstacles[RC::WaterSurface::kMaxObstacles] = {};
  const int count = RC::GetWaterObstacles(obstacles, RC::WaterSurface::kMaxObstacles);

  PushScaledFont(1.6f);
  ImGui::TextColored(count > 0 ? kAccent : kWarn, "障害物 %d / %d 件", count,
                     RC::GetMaxWaterObstacles());
  ImGui::PopFont();

  for (int i = 0; i < count; ++i) {
    ImGui::Text("  #%d   位置 (%7.2f, %7.2f, %7.2f)   半径 %.2f", i,
                obstacles[i].x, obstacles[i].y, obstacles[i].z, obstacles[i].w);
  }

  // 岩の実際の位置と突き合わせる
  auto rocks = FindWithScript(scene, "WaterObstacleScript");
  if (!rocks.empty()) {
    ImGui::Spacing();
    ImGui::TextColored(kDim, "拾い出しているスクリプト: %s",
                       rocks.front()->GetName().c_str());
  }
  ImGui::Spacing();
  ImGui::TextColored(kDim,
                     "岩を動かすとこの座標も追従します。描画の反射と CPU 側の"
                     "水面高さが同じリストを見ているので、両方が同時に正しくなります。");
}

// ---------------------------------------------------------------------------
// F7 の実行
// ---------------------------------------------------------------------------

void RunStepAction(Scene *scene) {
  const Step &step = kSteps[g_step];
  switch (step.kind) {
  case Kind::Week1Scene:
    DebugBridge::RequestScene("CG4");
    break;

  case Kind::Pcf:
  case Kind::Reflect:
    g_autoToggle = !g_autoToggle;
    break;

  case Kind::SceneFlow: {
    const char *next = kSceneFlow[g_sceneFlowIndex % kSceneFlowCount];
    DebugBridge::RequestScene(next);
    g_sceneFlowIndex = (g_sceneFlowIndex + 1) % kSceneFlowCount;
    break;
  }

  case Kind::SpriteDigits: {
    auto &s = GameSession::Get();
    s.SetScore(1234567);
    s.SetPlayerHp(5, 5);
    s.Finish(GameSession::Outcome::Cleared);
    DebugBridge::RequestScene("Result");
    break;
  }

  case Kind::Resources: {
    auto &ctx = RC::GetRenderContext();
    g_baseline.taken = true;
    g_baseline.spriteInUse = ctx.Sprites().InUseCount();
    g_baseline.pointLights = RC::GetActivePointLightCount();
    g_baseline.spotLights = RC::GetActiveSpotLightCount();
    g_baseline.areaLights = RC::GetActiveAreaLightCount();
    g_baseline.entities = scene ? scene->GetEntities().size() : 0u;
    Log::Print("[Capture] リソースの基準値を記録しました");
    break;
  }

  case Kind::Buoyancy: {
    auto floaters = FindWithScript(scene, "BuoyancyScript");
    Entity *target = nullptr;
    for (auto *e : floaters) {
      if (e->GetId() == g_drop.targetId) { target = e; break; }
    }
    if (!target && !floaters.empty()) target = floaters.front();
    if (!target) break;
    auto *tr = target->GetComponent<TransformComponent>();
    if (!tr) break;

    const nlohmann::json params =
        SafeSerialize(GetScript(target, "BuoyancyScript"));

    g_drop.targetId = target->GetId();
    g_drop.waterY = SampleWaterHeightLikeBuoyancy(
        scene, tr->position.x, tr->position.z, tr->rotation.y, params);
    // 前回の上下動の速度が残っていると同じ高さから落としても結果がばらつく。
    // BuoyancyScript の内部速度は型が見えないのでタグ経由で消してもらい、
    // Rigidbody 側はここで直接消す（タグ経由だと 1 フレーム遅れるため）。
    target->SetTag("reset_motion", 1);
    if (auto *rb = target->GetComponent<RigidbodyComponent>()) {
      rb->velocity = {0.0f, 0.0f, 0.0f};
    }
    tr->position.y = g_drop.waterY + g_drop.dropHeight;
    g_drop.startY = tr->position.y;
    g_drop.phase = DropMeasure::Phase::Falling;
    g_drop.submergedTime = 0.0f;
    g_drop.deepest = 0.0f;
    g_drop.recoverTime = 0.0f;
    g_drop.everFullySubmerged = false;
    Log::Print("[Capture] 浮力の落下計測を開始しました");
    break;
  }

  case Kind::Wave: {
    if (!scene) break;
    Entity *mgr = scene->FindEntityByName("WaveManager");
    if (!mgr) {
      auto found = FindWithScript(scene, "WaveManagerScript");
      if (!found.empty()) mgr = found.front();
    }
    if (!mgr) break;

    const int active = mgr->GetTagInt("wave_active", 0);
    const int pending = mgr->GetTagInt("wave_request", 0);

    // 進行中なら強制クリア。強制クリアは進行中のIDと一致していないと受理されない。
    if (active != 0) {
      mgr->SetTag("wave_force_clear", active);
      Log::Print("[Capture] ウェーブを強制クリアしました");
      break;
    }
    // Intro 中と Cleared 中は wave_active も wave_request も 0 に戻っていて、
    // 外からは Idle と見分けが付かない。ここで要求を通すと、クリア演出の途中で
    // 次のウェーブが始まってしまう（動画で一番見せたい場面が壊れる）。
    // WaveManager が公開している wave_busy で弾く。
    if (pending != 0 || mgr->GetTagInt("wave_busy", 0) != 0) break;

    // 要求するIDは「シーンに置いてあるスポナーの担当ウェーブのうち、まだクリアしていない最小値」。
    // waves_cleared + 1 にすると、スポナーが無いIDを要求したときに
    // WaveManager が即クリア扱いにするため、敵が湧かないまま累計だけ増える。
    const int lastCleared = mgr->GetTagInt("wave_cleared_id", 0);
    int next = 0;
    for (auto *s : FindWithScript(scene, "WaveSpawnerScript")) {
      const int id = s->GetTagInt("spawner_wave_id", 0);
      if (id <= 0 || id <= lastCleared) continue;
      if (next == 0 || id < next) next = id;
    }
    if (next == 0) {
      // 全部クリア済みなら最小のIDへ戻して撮り直せるようにする
      for (auto *s : FindWithScript(scene, "WaveSpawnerScript")) {
        const int id = s->GetTagInt("spawner_wave_id", 0);
        if (id <= 0) continue;
        if (next == 0 || id < next) next = id;
      }
    }
    if (next == 0) {
      Log::Print("[Capture] ウェーブスポナーが見つからないため開始できません");
      break;
    }
    mgr->SetTag("wave_request", next);
    Log::Print("[Capture] ウェーブ " + std::to_string(next) + " の開始を要求しました");
    break;
  }

  case Kind::Ship: {
    auto ships = FindWithScript(scene, "ShipEnemyScript");
    for (auto *e : ships) {
      if (e->GetTagInt("enemy_defeated", 0) != 0) continue;
      // pending_damage は 1 フレーム分をまとめて消費する加算式のタグ。
      const int hp = (std::max)(e->GetTagInt("current_hp", 1), 1);
      e->SetTag("pending_damage", e->GetTagInt("pending_damage", 0) + hp);
      Log::Print("[Capture] 船を撃沈させました");
      break;
    }
    break;
  }

  default:
    break;
  }
}

// ---------------------------------------------------------------------------
// ステップ移動
// ---------------------------------------------------------------------------

/// @brief ステップを離れるときに、そのステップが入れた上書きを畳む
void LeaveStep() {
  const Step &step = kSteps[g_step];
  if (step.kind == Kind::Pcf) {
    RC::GetRenderContext().ShadowDebug().enabled = false;
  } else if (step.kind == Kind::Reflect) {
    RC::SetWaterReflectOverride(false, 1.0f, 3.0f);
  }
}

void GoToStep(int index) {
  LeaveStep();
  g_step = (index % kStepCount + kStepCount) % kStepCount;
  g_stepTime = 0.0f;
  g_autoToggle = true;
  g_toggleOn = false;
}

#endif // RC_ENABLE_IMGUI

} // namespace

// ===========================================================================
// 公開関数
// ===========================================================================

bool CaptureMode::IsActive() { return g_active; }

bool CaptureMode::WantsPlaying() { return g_wantsPlaying; }

bool CaptureMode::IsGameHovered() { return g_gameHovered; }

void CaptureMode::SetActive(bool active) {
  if (g_active == active) return;
#if RC_ENABLE_IMGUI
  if (!active) {
    LeaveStep(); // 抜けるときは上書きを必ず畳む
  }
#endif
  g_active = active;
  g_wantsPlaying = active;
  // 入った直後の 1 フレームはまだ Draw を通っていないので、
  // ゲーム画面の上にいる前提にしておく（そうしないと初回だけ撃てない）。
  g_gameHovered = true;
  if (active) {
#if RC_ENABLE_IMGUI
    g_stepTime = 0.0f;
#endif
    Log::Print("[Capture] 撮影モードを開始しました（F10 次へ / F8 戻る / F7 実行 / F9 終了）");
  } else {
    Log::Print("[Capture] 撮影モードを終了しました");
  }
}

bool CaptureMode::HandleHotkeys() {
#if RC_ENABLE_IMGUI
  // ゲーム側が使っているキー（方向キー・スペース・F1/F3/F4）とぶつからないよう、
  // 撮影モードの操作は F7 〜 F10 に寄せてある。
  if (ImGui::IsKeyPressed(ImGuiKey_F9, false)) {
    SetActive(!g_active);
    return true;
  }
#endif
  return false;
}

void CaptureMode::Draw(D3D12_GPU_DESCRIPTOR_HANDLE viewportSrv, Dx12Core *core,
                       Scene *currentScene, float deltaTime) {
#if RC_ENABLE_IMGUI
  if (!g_active) return;

  g_stepTime += deltaTime;
  g_clock += deltaTime;

  // --- キー操作 ---------------------------------------------------------
  if (ImGui::IsKeyPressed(ImGuiKey_F10, false)) GoToStep(g_step + 1);
  if (ImGui::IsKeyPressed(ImGuiKey_F8, false)) GoToStep(g_step - 1);
  if (ImGui::IsKeyPressed(ImGuiKey_F7, false)) RunStepAction(currentScene);
  if (ImGui::IsKeyPressed(ImGuiKey_F6, false)) g_showHelp = !g_showHelp;

  const ImGuiViewport *vp = ImGui::GetMainViewport();

  // --- ゲーム画面を全画面で敷く ----------------------------------------
  // アスペクト比を保って中央に置く（引き伸ばすと録画で見栄えが悪くなる）。
  {
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    // アスペクト比を保つと上下（または左右）に帯が出る。
    // エディタの背景色（濃いグレー）のままだと録画で目立つので黒にする。
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    // NoInputs にしてあるのは見た目のためではなく描画順のため。
    // ImGui は NoBringToFrontOnFocus 付きのウィンドウをリストの先頭へ入れる＝
    // 最初に描く＝最背面になるので、それを付けると DockSpace の不透明な背景に
    // ゲーム画面が隠れて真っ暗な動画しか撮れない。
    // かといって外すと、ゲーム画面をクリックした瞬間に前面へ来て字幕を隠す。
    // NoInputs ならホバー判定の対象外になり、順序が固定されたまま
    // マウス入力もゲーム側へ素通りする。
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    if (ImGui::Begin("##CaptureView", nullptr, flags)) {
      if (viewportSrv.ptr != 0 && vp->Size.x > 0.0f && vp->Size.y > 0.0f) {
        float aspect = 16.0f / 9.0f;
        if (core) {
          const auto &gameVp = core->Viewport();
          if (gameVp.Width > 0.0f && gameVp.Height > 0.0f) {
            aspect = gameVp.Width / gameVp.Height;
          }
        }
        float w = vp->Size.x;
        float h = w / aspect;
        if (h > vp->Size.y) {
          h = vp->Size.y;
          w = h * aspect;
        }
        ImGui::SetCursorPos(ImVec2((vp->Size.x - w) * 0.5f, (vp->Size.y - h) * 0.5f));
        const ImVec2 imageMin = ImGui::GetCursorScreenPos();
        ImGui::Image((ImTextureID)viewportSrv.ptr, ImVec2(w, h));

        // ゲームへマウス座標を渡す。
        // 通常は Viewport パネルがこれをやっているが、撮影モードでは
        // そのパネルを描かないので、ここで同じことをしないと
        // 照準がマウスに追従しなくなる（画面に敷いた矩形が
        // レターボックスで中央寄せされているぶんも差し引く）。
        if (auto *input = Input::GetInstance()) {
          float gameW = w, gameH = h;
          if (core) {
            const auto &gameVp = core->Viewport();
            if (gameVp.Width > 0.0f && gameVp.Height > 0.0f) {
              gameW = gameVp.Width;
              gameH = gameVp.Height;
            }
          }
          const ImVec2 mouse = ImGui::GetMousePos();
          input->SetGameMousePosition(((mouse.x - imageMin.x) / w) * gameW,
                                      ((mouse.y - imageMin.y) / h) * gameH);
        }
      }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
  }

  const Step &step = kSteps[g_step];

  // --- 上部の字幕 -------------------------------------------------------
  {
    const float width = (std::min)(vp->Size.x - 40.0f, 900.0f);
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + 20.0f, vp->Pos.y + 20.0f));
    ImGui::SetNextWindowSize(ImVec2(width, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.82f);
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs;

    if (ImGui::Begin("##CaptureCaption", nullptr, flags)) {
      ImGui::TextColored(kDim, "%s  ・  %d / %d", step.week, g_step + 1, kStepCount);

      PushScaledFont(2.1f);
      ImGui::TextColored(kAccent, "%s", step.id);
      ImGui::SameLine();
      ImGui::TextUnformatted(step.title);
      ImGui::PopFont();

      ImGui::Spacing();
      PushScaledFont(1.15f);
      ImGui::PushTextWrapPos(width - 20.0f);
      ImGui::TextUnformatted(step.watch);
      ImGui::PopTextWrapPos();
      ImGui::PopFont();
    }
    ImGui::End();
  }

  // --- 左下の実測値 -----------------------------------------------------
  {
    const float width = (std::min)(vp->Size.x - 40.0f, 620.0f);
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + 20.0f, vp->Pos.y + vp->Size.y - 20.0f),
                            ImGuiCond_Always, ImVec2(0.0f, 1.0f));
    ImGui::SetNextWindowSize(ImVec2(width, 0.0f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f),
                                        ImVec2(width, vp->Size.y * 0.62f));
    ImGui::SetNextWindowBgAlpha(0.82f);
    // NoDecoration には NoScrollbar が含まれる。高さの上限に当たったときに
    // 下が黙って切れてしまうので、ここは個別指定にしてスクロールを残す。
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_AlwaysAutoResize;

    bool overlayHovered = false;
    if (ImGui::Begin("##CaptureLive", nullptr, flags)) {
      // このパネルだけは操作できる（落下高さのスライダがある）ので、
      // 上にいるあいだはゲームへ入力を通さない。
      // 字幕とヒントは NoInputs なのでホバー対象にならず、ここだけ見ればよい。
      overlayHovered = ImGui::IsWindowHovered(
          ImGuiHoveredFlags_RootAndChildWindows |
          ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
      PushScaledFont(1.1f);
      switch (step.kind) {
      case Kind::Info:         LiveInfo(currentScene); break;
      case Kind::Week1Scene:   LiveWeek1Scene(currentScene); break;
      case Kind::Pcf:          LivePcf(deltaTime); break;
      case Kind::Reflect:      LiveReflect(); break;
      case Kind::SceneFlow:    LiveSceneFlow(currentScene); break;
      case Kind::SpriteDigits: LiveSpriteDigits(); break;
      case Kind::Resources:    LiveResources(currentScene); break;
      case Kind::Rail:         LiveRail(currentScene); break;
      case Kind::Lights:       LiveLights(currentScene); break;
      case Kind::Rotation:     LiveRotation(currentScene); break;
      case Kind::Entities:     LiveEntities(currentScene); break;
      case Kind::Buoyancy:     LiveBuoyancy(currentScene, deltaTime); break;
      case Kind::Wave:         LiveWave(currentScene); break;
      case Kind::Ship:         LiveShip(currentScene); break;
      case Kind::Obstacles:    LiveObstacles(currentScene); break;
      }
      ImGui::PopFont();
    }
    ImGui::End();

    // スライダをドラッグしたままウィンドウの外へマウスが出ることがある。
    // そのあいだも「操作中」として扱わないと、ドラッグ中に弾が出てしまう。
    g_gameHovered = !(overlayHovered || ImGui::IsAnyItemActive());
  }

  // --- 下部の操作ヒント（F6 で消せる） ----------------------------------
  if (g_showHelp) {
    ImGui::SetNextWindowPos(
        ImVec2(vp->Pos.x + vp->Size.x - 20.0f, vp->Pos.y + vp->Size.y - 20.0f),
        ImGuiCond_Always, ImVec2(1.0f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.7f);
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs;

    if (ImGui::Begin("##CaptureHelp", nullptr, flags)) {
      ImGui::TextColored(kDim, "F10 次へ   F8 戻る   F7 %s   F6 このヒントを消す   F9 終了",
                         step.action ? step.action : "（このステップでは無し）");
    }
    ImGui::End();
  }
#else
  (void)viewportSrv;
  (void)core;
  (void)currentScene;
  (void)deltaTime;
#endif
}
