#include "VerifyPanel.h"

#include "Common/EngineConfig.h"

#if RC_ENABLE_IMGUI
#include "imgui/imgui.h"
#endif

#include "../Game/Framework/GameModeBase.h"
#include "../Game/Framework/GameSession.h"
#include "../Game/Framework/GameStateBase.h"
#include "../Game/Scene/Scene.h"
#include "Common/Log/Log.h"
#include "DebugBridge.h"
#include "Dx12/Dx12Core.h"
#include "ECS/Entity.h"
#include "ECS/NativeScriptComponent.h"
#include "ECS/ScriptableEntity.h"
#include "Render/RenderCommon.h"
#include "Render/RenderContext.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

// ============================================================================
// チェックリストの中身
// ----------------------------------------------------------------------------
// 直近3週間（8/09〜8/30）で実装したものを、確認する順に並べる。
// 「how」には ここを見れば分かる という判定材料を書いておく。
// 録画を撮り直さないと分からない、という状態を無くすのが目的。
// ============================================================================

namespace {

/// @brief チェックリスト1項目の状態
enum class CheckState : int {
  Unchecked = 0, ///< まだ見ていない
  Ok = 1,        ///< 狙いどおり
  Ng = 2,        ///< 直しが要る
};

/// @brief チェックリストの1項目（定義側。実行中に変わらない）
struct CheckItem {
  const char *id;    ///< タスクID
  const char *week;  ///< いつ実装したか
  const char *title; ///< 何を実装したか
  const char *how;   ///< 何を見れば確認できるか
  const char *tab;   ///< 確認に使うタブ
};

const CheckItem kItems[] = {
    // ---- 第2週（commit 6469548「波と影の改良」）----
    {"C-04", "第2週", "シャドウマップの PCF 対応",
     "「描画」タブで PCF 半径を 0 と 1.0 で切り替える。0 のとき影の輪郭が"
     "ジャギーになり、1.0 で滑らかになれば 3x3 タップが効いている。"
     "シャドウマップのプレビューに深度が出ていることも確認する。",
     "描画"},
    {"C-03", "第2週", "障害物への波の反射（鏡像法）",
     "「描画」タブで反射の強さを 0.001（ほぼ無効）と 1.0 で切り替え、"
     "岩の風上側だけに波が立つか、その波が岩から離れる向きへ進むかを見る。"
     "岩の裏側に出ているなら facing の符号が逆。到達範囲も動かして比べる。",
     "描画"},
    {"A-02", "第2週", "コアゲームループの結合（GameSession）",
     "「ゲームループ」タブでスコアと HP を書き込んでから Result へ飛び、"
     "数字とランクのバーが一致するか見る。HP 満タンで S、6割で A、3割で B。"
     "Title へ戻って再度 Game に入ると Play Count が増え、スコアが 0 に戻る。",
     "ゲームループ"},
    {"D-04", "第2週", "死亡エンティティのハンドル解放漏れ",
     "「リソース」タブで基準を記録してから、敵を倒す／シーンを一周する。"
     "使用中スプライトとライトの数が基準へ戻れば回収できている。"
     "戻らずに増え続けるなら、まだ解放漏れが残っている。",
     "リソース"},
    {"D-05", "第2週", "スプライトの切り出し矩形が効かない不具合",
     "Result 画面でスコアの各桁が別々の数字として出ていれば直っている。"
     "全桁が同じ数字なら、1ハンドル1フレーム1回の制約に触れている。",
     "リソース"},

    // ---- 第3週（未コミット）----
    {"C-01/02", "第3週", "浮力シミュレーション",
     "「浮力・船」タブで対象の密度比・抵抗と、そこから出る周期／減衰比を見る。"
     "高い位置から落として、余分に沈み込まず 1.5 秒ほどで水面へ戻るか。"
     "2〜3回跳ねて収まる（減衰比 0.34 前後）のが狙い。",
     "浮力・船"},
    {"T-20", "第3週", "ステージのルート分岐",
     "「レール・ウェーブ」タブで現在のウェイポイント番号が進むのを見る。"
     "右キーを押しながら分岐点を通ると水上ルートへ逸れ、共通のゴールで"
     "合流するか。終点で rail_finished が立ち、周回に入らないこと。",
     "レール・ウェーブ"},
    {"A-05", "第3週", "レベルローダーの LIGHT / CAMERA 対応",
     "ステージを読み込んで、Hierarchy にライトとカメラが出ているか。"
     "向きが数十倍ずれているなら、レベル JSON の rotation_in_degrees が"
     "実データの単位（度数法／ラジアン）と食い違っている。",
     "リソース"},
    {"D-07〜10", "第3週", "既存不具合の修正4件",
     "シーンを保存 → 開き直して、エンティティが複製されていないこと。"
     "スクリプトを外した物体が無重力のまま残っていないこと。",
     "リソース"},

    // ---- 第4週（未コミット・作業中）----
    {"D-01", "第4週", "水面シェーダの障害物座標のデータ化",
     "「描画」タブの障害物リストに、岩の実際の位置と半径が出ているか。"
     "岩を動かすとリストの座標も追従すること。半径がスケールと"
     "食い違っている（旧ハードコードの 7/8/9 のまま）なら要調整。",
     "描画"},
    {"A-03", "第4週", "ウェーブ戦闘の構築",
     "「レール・ウェーブ」タブでウェーブを開始し、残敵数が減っていくか。"
     "全滅で累計クリア数が増え、レールが再進行するか。"
     "強制クリアでも同じ経路を通ることを確認する。",
     "レール・ウェーブ"},
    {"T-15", "第4週", "新規敵タイプ「船」の AI と挙動",
     "「浮力・船」タブで船の HP と状態を見る。船体が波に乗って傾くか。"
     "撃沈するとダメージではなく浸水（flooding）で沈むか。",
     "浮力・船"},
    {"—", "全体", "Title → Game → Result / GameOver の一周",
     "遷移のたびに弾や敵が残っていないこと。HUD が暗い 3D 背景の上でも"
     "読めること。ここが通ればコミットしてよい。",
     "ゲームループ"},
};

constexpr int kItemCount = static_cast<int>(sizeof(kItems) / sizeof(kItems[0]));

/// @brief チェックリスト1項目の記録側（実行中に変わる）
struct CheckRecord {
  CheckState state = CheckState::Unchecked;
  char note[192] = {};
};

CheckRecord g_records[kItemCount];

constexpr const char *kChecklistPath = "../project/VerifyChecklist.json";

/// @brief 保存済みのチェック状態を読み戻したか
/// @details 保存は終了処理から無条件に走るのに対し、読み込みはパネルを
///          開いたときにしか走らない。この差を放置すると
///          「パネルを一度も開かずに終了 → 前回の記録が空で上書きされる」
///          という形で記録が消える。保存前にも必ず読み込みを通すためのフラグ。
bool g_checklistLoaded = false;

#if RC_ENABLE_IMGUI

/// @brief リソース数の基準値（「基準を記録」で取る）
struct ResourceBaseline {
  bool taken = false;
  size_t spriteAllocated = 0;
  size_t spriteInUse = 0;
  int pointLights = 0;
  int spotLights = 0;
  int areaLights = 0;
  size_t entities = 0;
};
ResourceBaseline g_baseline;

// ---------------------------------------------------------------------------
// スクリプトへ到達するための小道具
// ---------------------------------------------------------------------------
// ゲームプレイのスクリプトは .cpp 内ローカル型でヘッダが無いため、
// ここからは型が見えない。scriptTypeName（文字列）で絞り込み、
// 基底の仮想関数（OnImGui / Serialize）とエンティティのタグだけを使う。

/// @brief 指定した型名のスクリプトを持つエンティティを集める
/// @param typeName 部分一致で判定する（typeid 名は "class Foo" になり得るため）
std::vector<Entity *> FindEntitiesWithScript(Scene *scene, const char *typeName) {
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
ScriptableEntity *GetScriptInstance(Entity *e, const char *typeName) {
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

/// @brief 指定したタグを持つ最初のエンティティを探す
Entity *FindEntityWithTag(Scene *scene, const char *tag) {
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

/// @brief チェック状態に応じた色つきラベルを出す
void DrawStateBadge(CheckState state) {
  switch (state) {
  case CheckState::Ok:
    ImGui::TextColored(ImVec4(0.40f, 0.85f, 0.45f, 1.0f), "[OK]");
    break;
  case CheckState::Ng:
    ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.40f, 1.0f), "[NG]");
    break;
  default:
    ImGui::TextColored(ImVec4(0.60f, 0.60f, 0.60f, 1.0f), "[ - ]");
    break;
  }
}

/// @brief 説明用のグレーの小さめテキスト
void Hint(const char *text) {
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.68f, 0.72f, 1.0f));
  ImGui::TextWrapped("%s", text);
  ImGui::PopStyleColor();
}

// ===========================================================================
// タブ: チェックリスト
// ===========================================================================
void DrawChecklistTab() {
  int ok = 0, ng = 0;
  for (int i = 0; i < kItemCount; ++i) {
    if (g_records[i].state == CheckState::Ok) ++ok;
    if (g_records[i].state == CheckState::Ng) ++ng;
  }

  ImGui::Text("確認済み %d / %d", ok, kItemCount);
  ImGui::SameLine();
  if (ng > 0) {
    ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.40f, 1.0f), "（要修正 %d 件）", ng);
  } else {
    ImGui::TextDisabled("（要修正なし）");
  }
  ImGui::ProgressBar(kItemCount > 0 ? static_cast<float>(ok) / kItemCount : 0.0f,
                     ImVec2(-1.0f, 0.0f));

  if (ImGui::Button("保存")) {
    VerifyPanel::SaveChecklist();
  }
  ImGui::SameLine();
  if (ImGui::Button("読み込み")) {
    VerifyPanel::LoadChecklist();
  }
  ImGui::SameLine();
  if (ImGui::Button("すべて未確認へ戻す")) {
    for (int i = 0; i < kItemCount; ++i) {
      g_records[i].state = CheckState::Unchecked;
      g_records[i].note[0] = '\0';
    }
  }
  ImGui::SameLine();
  ImGui::TextDisabled("(%s)", kChecklistPath);

  ImGui::Separator();
  Hint("上から順に見ていく。各項目の「確認方法」に書いた画がそのとおりに"
       "出ていれば OK、違っていれば NG にして気づいたことをメモに残す。");
  ImGui::Spacing();

  const char *lastWeek = nullptr;
  for (int i = 0; i < kItemCount; ++i) {
    const CheckItem &item = kItems[i];
    CheckRecord &rec = g_records[i];

    if (!lastWeek || std::strcmp(lastWeek, item.week) != 0) {
      lastWeek = item.week;
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.95f, 1.0f), "── %s ──", item.week);
    }

    ImGui::PushID(i);
    DrawStateBadge(rec.state);
    ImGui::SameLine();

    const bool open = ImGui::TreeNodeEx(
        "##node", ImGuiTreeNodeFlags_SpanAvailWidth,
        "%s  %s", item.id, item.title);

    if (open) {
      Hint(item.how);
      ImGui::Spacing();

      int state = static_cast<int>(rec.state);
      ImGui::RadioButton("未確認", &state, 0);
      ImGui::SameLine();
      ImGui::RadioButton("OK", &state, 1);
      ImGui::SameLine();
      ImGui::RadioButton("NG", &state, 2);
      rec.state = static_cast<CheckState>(state);

      ImGui::SameLine();
      ImGui::TextDisabled("| 確認タブ: %s", item.tab);

      ImGui::SetNextItemWidth(-1.0f);
      ImGui::InputTextWithHint("##note", "気づいたこと・直す内容", rec.note,
                               sizeof(rec.note));
      ImGui::Spacing();
      ImGui::TreePop();
    }
    ImGui::PopID();
  }
}

// ===========================================================================
// タブ: 描画（C-03 波の反射 / C-04 PCF / D-01 障害物）
// ===========================================================================
void DrawRenderTab(Dx12Core *core) {
  auto &ctx = RC::GetRenderContext();

  // ---- C-04 シャドウマップ PCF ----
  if (ImGui::CollapsingHeader("C-04  シャドウマップの PCF",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    auto &dbg = ctx.ShadowDebug();

    Hint("シーン側は毎フレーム pcfRadius = 1.0 を作り直して渡しているため、"
         "外から値を効かせるには上書きを有効にする必要がある。"
         "チェックを外せば元の挙動へ戻る。");

    ImGui::Checkbox("上書きを有効にする##shadow", &dbg.enabled);

    ImGui::BeginDisabled(!dbg.enabled);
    ImGui::SliderFloat("PCF 半径", &dbg.pcfRadius, 0.0f, 4.0f, "%.2f");
    Hint("0 以下で 1 タップになり PCF が無効。1.0 が現在の実装値。"
         "0 と 1.0 を往復させて輪郭のジャギーが消えるかを見る。");

    ImGui::Spacing();
    if (ImGui::Button("PCF OFF (0)")) dbg.pcfRadius = 0.0f;
    ImGui::SameLine();
    if (ImGui::Button("標準 (1.0)")) dbg.pcfRadius = 1.0f;
    ImGui::SameLine();
    if (ImGui::Button("強め (2.5)")) dbg.pcfRadius = 2.5f;
    ImGui::Spacing();

    ImGui::SliderFloat("バイアス", &dbg.bias, 0.0f, 0.05f, "%.4f");
    Hint("PCF 半径を広げると接地部分の影が薄れるので、バイアスと"
         "セットで見ること。シーン側のハードコード値は 0.01。");
    ImGui::SliderFloat("影の濃さ", &dbg.darkness, 0.0f, 1.0f, "%.2f");
    ImGui::Checkbox("影そのものを切る", &dbg.forceDisable);
    ImGui::EndDisabled();

    ImGui::Spacing();
    const auto &sm = ctx.GetShadowMap();
    ImGui::Text("シャドウマップ: %u x %u", sm.GetWidth(), sm.GetHeight());
    if (core && sm.GetResource()) {
      const auto srv = core->SRV().GPUAt(sm.GetSrvIndex());
      if (srv.ptr != 0) {
        ImGui::Image((ImTextureID)srv.ptr, ImVec2(192.0f, 192.0f));
        Hint("SRV は R32_FLOAT なので赤の濃淡で出る（明るいほど遠い）。"
             "濃淡がまったく無い一様な色なら、影を落とす物が"
             "ライトの正射影範囲（range 40）から外れている。");
      }
    }
  }

  ImGui::Spacing();

  // ---- C-03 波の反射 ----
  if (ImGui::CollapsingHeader("C-03  障害物への波の反射",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    float ovStrength = 1.0f, ovRange = 3.0f;
    bool ovEnabled = RC::GetWaterReflectOverride(&ovStrength, &ovRange);

    ImGui::Text("スクリプトが入れている値: 強さ %.3f / 範囲 %.2f",
                RC::GetWaterReflectStrength(), RC::GetWaterReflectRange());
    Hint("水面に載せた WaterObstacleScript が毎フレーム入れ直すため、"
         "外から一度書いても次のフレームで消える。"
         "見比べたいあいだだけ上書きを有効にする。");

    bool changed = ImGui::Checkbox("上書きを有効にする##water", &ovEnabled);

    ImGui::BeginDisabled(!ovEnabled);
    changed |= ImGui::SliderFloat("反射の強さ", &ovStrength, 0.001f, 3.0f, "%.3f");
    changed |= ImGui::SliderFloat("到達範囲（半径倍率）", &ovRange, 0.5f, 8.0f, "%.2f");

    ImGui::Spacing();
    if (ImGui::Button("ほぼ無効 (0.001)")) { ovStrength = 0.001f; changed = true; }
    ImGui::SameLine();
    if (ImGui::Button("標準 (1.0)")) { ovStrength = 1.0f; changed = true; }
    ImGui::SameLine();
    if (ImGui::Button("強調 (2.5)")) { ovStrength = 2.5f; changed = true; }
    ImGui::EndDisabled();

    if (changed) {
      RC::SetWaterReflectOverride(ovEnabled, ovStrength, ovRange);
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.75f, 0.35f, 1.0f));
    ImGui::TextWrapped(
        "0 を渡すと「無効」ではなく既定値 1.0 / 3.0 に化ける（D-14）。"
        "反射を消したいときは 0.001 を使うこと。スライダの下限もそこで止めてある。");
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Text("水面の時刻: %.2f s", RC::GetWaterTime());
    Hint("CPU 側の水面高さ計算はこの時刻を読む。見た目と当たり判定の"
         "水面がずれているときは、まずここが同じ値かを疑う。");
  }

  ImGui::Spacing();

  // ---- D-01 障害物リスト ----
  if (ImGui::CollapsingHeader("D-01  水面が見ている障害物リスト",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    const int maxObs = RC::GetMaxWaterObstacles();
    std::vector<RC::Vector4> obstacles(static_cast<size_t>((std::max)(maxObs, 1)));
    const int count = RC::GetWaterObstacles(obstacles.data(), maxObs);

    ImGui::Text("障害物 %d / %d 件", count, maxObs);
    Hint("以前はレンダラ内にハードコードしていた値（D-01）。"
         "岩を動かしたときにここの座標が追従すれば、データ化できている。");

    if (count <= 0) {
      ImGui::TextDisabled("（登録なし。水面に WaterObstacleScript が載っていないか、"
                          "岩が水面をまたいでいない）");
    } else if (ImGui::BeginTable("obstacles", 5,
                                 ImGuiTableFlags_Borders |
                                     ImGuiTableFlags_RowBg |
                                     ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn("#");
      ImGui::TableSetupColumn("X");
      ImGui::TableSetupColumn("Y");
      ImGui::TableSetupColumn("Z");
      ImGui::TableSetupColumn("半径");
      ImGui::TableHeadersRow();
      for (int i = 0; i < count; ++i) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("%d", i);
        ImGui::TableNextColumn(); ImGui::Text("%.2f", obstacles[i].x);
        ImGui::TableNextColumn(); ImGui::Text("%.2f", obstacles[i].y);
        ImGui::TableNextColumn(); ImGui::Text("%.2f", obstacles[i].z);
        ImGui::TableNextColumn(); ImGui::Text("%.2f", obstacles[i].w);
      }
      ImGui::EndTable();
    }
  }
}

// ===========================================================================
// タブ: ゲームループ（A-02）
// ===========================================================================
void DrawGameLoopTab(Scene *currentScene) {
  auto &session = GameSession::Get();

  ImGui::Text("現在のシーン: %s",
              currentScene ? currentScene->Name() : "(なし)");

  ImGui::Spacing();
  if (ImGui::CollapsingHeader("GameSession の中身",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    Hint("シーンをまたいで残る唯一の値。Result / GameOver はここを読んで"
         "表示している。Game シーンの表示と一致していれば受け渡しは通っている。");

    const char *outcomeName = "InProgress";
    switch (session.GetOutcome()) {
    case GameSession::Outcome::Cleared: outcomeName = "Cleared"; break;
    case GameSession::Outcome::GameOver: outcomeName = "GameOver"; break;
    default: break;
    }

    ImGui::Text("スコア     : %d", session.Score());
    ImGui::Text("HP         : %d / %d", session.PlayerHp(), session.PlayerMaxHp());
    ImGui::Text("経過時間   : %.2f s", session.ElapsedTime());
    ImGui::Text("決着       : %s", outcomeName);
    ImGui::Text("評価ランク : %c", session.Rank());
    ImGui::Text("プレイ回数 : %d", session.PlayCount());

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.75f, 0.35f, 1.0f));
    ImGui::TextWrapped(
        "ランクは HP 率だけで決まっている（S: 99%%以上 / A: 60%% / B: 30%% / C: それ未満）。"
        "スコアは効いていないので、そのつもりで見ること。");
    ImGui::PopStyleColor();
  }

  ImGui::Spacing();
  if (ImGui::CollapsingHeader("値を作って Result の表示を確かめる",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    Hint("Game シーンでは RailShooterController が毎フレーム上書きするため、"
         "ここで書いた値は残らない。Result / GameOver へ移ってから使うこと。");

    static int score = 12345;
    static int hp = 5;
    static int maxHp = 5;

    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputInt("スコア", &score);
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputInt("HP", &hp);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputInt("最大 HP", &maxHp);

    if (ImGui::Button("書き込む")) {
      session.SetScore(score);
      session.SetPlayerHp(hp, (std::max)(maxHp, 1));
      Log::Print("[Verify] GameSession へ値を書き込みました");
    }
    ImGui::SameLine();
    if (ImGui::Button("クリア扱いにする")) {
      session.Finish(GameSession::Outcome::Cleared);
    }
    ImGui::SameLine();
    if (ImGui::Button("ゲームオーバー扱いにする")) {
      session.Finish(GameSession::Outcome::GameOver);
    }

    ImGui::Spacing();
    ImGui::Text("ランクを狙って作る:");
    if (ImGui::Button("S (満タン)")) {
      session.SetPlayerHp(maxHp, (std::max)(maxHp, 1));
      session.Finish(GameSession::Outcome::Cleared);
    }
    ImGui::SameLine();
    if (ImGui::Button("A (6割)")) {
      session.SetPlayerHp(static_cast<int>(maxHp * 0.7f), (std::max)(maxHp, 1));
      session.Finish(GameSession::Outcome::Cleared);
    }
    ImGui::SameLine();
    if (ImGui::Button("B (3割)")) {
      session.SetPlayerHp((std::max)(static_cast<int>(maxHp * 0.4f), 1),
                          (std::max)(maxHp, 1));
      session.Finish(GameSession::Outcome::Cleared);
    }
    ImGui::SameLine();
    if (ImGui::Button("C (瀕死)")) {
      session.SetPlayerHp(0, (std::max)(maxHp, 1));
      session.Finish(GameSession::Outcome::Cleared);
    }
    Hint("押したあと Result へ飛んで、色付きバーの本数が S=4 / A=3 / B=2 / C=1 に"
         "なっているかを見る。");
  }

  ImGui::Spacing();
  if (ImGui::CollapsingHeader("シーンを飛ばす", ImGuiTreeNodeFlags_DefaultOpen)) {
    const bool ready = DebugBridge::CanRequestScene();
    if (!ready) {
      ImGui::TextDisabled("（App 側から切り替え先が登録されていない。"
                          "上部メニューの Scene からも切り替えられる）");
    }
    ImGui::BeginDisabled(!ready);
    const char *scenes[] = {"Title", "Select", "Game", "Result", "GameOver"};
    for (int i = 0; i < 5; ++i) {
      if (i > 0) ImGui::SameLine();
      if (ImGui::Button(scenes[i])) {
        DebugBridge::RequestScene(scenes[i]);
        Log::Print(std::string("[Verify] シーン切り替えを要求: ") + scenes[i]);
      }
    }
    ImGui::EndDisabled();
    Hint("Title → Game → Result → Title と一周させ、遷移のたびに弾や敵が"
         "残っていないかを見る。残数は「リソース」タブで数えられる。");
  }

  ImGui::Spacing();
  if (currentScene) {
    if (auto *mode = currentScene->GetGameMode()) {
      if (auto *state = mode->GetGameState()) {
        if (ImGui::CollapsingHeader("シーン側の GameState")) {
          ImGui::Text("スコア   : %d", state->GetScore());
          ImGui::Text("経過時間 : %.2f s", state->GetElapsedTime());
          Hint("こちらはシーンごとのインスタンス。Result からは読めないため"
               "GameSession を別に用意した、という関係になっている。");
        }
      }
    }
  }
}

// ===========================================================================
// タブ: レール・ウェーブ（T-20 / A-03）
// ===========================================================================
void DrawRailWaveTab(Scene *currentScene) {
  if (!currentScene) {
    ImGui::TextDisabled("シーンが読み込まれていません。");
    return;
  }

  // ---- T-20 ルート分岐 ----
  if (ImGui::CollapsingHeader("T-20  ステージのルート分岐",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    Entity *rail = FindEntityWithTag(currentScene, "has_rail");
    if (!rail) {
      ImGui::TextDisabled("（has_rail タグを持つエンティティが見つからない。"
                          "このシーンにレールは敷かれていない）");
    } else {
      ImGui::Text("レール: %s (id %u)", rail->GetName().c_str(), rail->GetId());
      ImGui::Text("終点到達 (rail_finished): %s",
                  rail->GetTagInt("rail_finished", 0) ? "はい" : "いいえ");

      Hint("分岐で index が飛ぶため「配列の末尾＝終点」が成り立たない。"
           "終点フラグを立てた地点で rail_finished が立つ。ここが立たずに"
           "index が回り続けるなら、迂回ルートの合流先で周回している。");

      ImGui::Spacing();
      if (auto *script = GetScriptInstance(rail, "RailMovementScript")) {
        Hint("以下はレール本体のインスペクタ表示。現在のウェイポイント番号、"
             "直近に選ばれた分岐、待機中のウェーブがここに出る。");
        ImGui::Separator();
        ImGui::Indent(8.0f);
        script->OnImGui();
        ImGui::Unindent(8.0f);
        ImGui::Separator();
      }

      if (ImGui::Button("このエンティティを選択##rail")) {
        currentScene->SetSelectedEntityId(rail->GetId());
      }
      ImGui::SameLine();
      ImGui::TextDisabled("Hierarchy / Inspector 側でも同じものを開けます。");
    }
  }

  ImGui::Spacing();

  // ---- A-03 ウェーブ戦闘 ----
  if (ImGui::CollapsingHeader("A-03  ウェーブ戦闘",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    Entity *mgr = currentScene->FindEntityByName("WaveManager");
    if (!mgr) {
      auto found = FindEntitiesWithScript(currentScene, "WaveManagerScript");
      if (!found.empty()) mgr = found.front();
    }

    if (!mgr) {
      ImGui::TextDisabled("（WaveManager が見つからない）");
    } else {
      const int active = mgr->GetTagInt("wave_active", 0);
      const int cleared = mgr->GetTagInt("waves_cleared", 0);
      const int lastCleared = mgr->GetTagInt("wave_cleared_id", 0);
      const int request = mgr->GetTagInt("wave_request", 0);

      ImGui::Text("進行中のウェーブ : %d %s", active,
                  active == 0 ? "（戦闘していない）" : "");
      ImGui::Text("累計クリア数     : %d", cleared);
      ImGui::Text("直近クリア       : %d", lastCleared);
      ImGui::Text("開始要求         : %d", request);

      // 残敵数はスクリプト内部にしか無いので、同じ条件で数え直す。
      // wave_active は Intro 中とクリア後は 0 に戻るため、そのあいだ
      // 「全ウェーブ合算」を残敵数として出さないよう、進行中のときだけ数える。
      if (active == 0) {
        ImGui::Text("残敵数           : （戦闘していない）");
      } else {
        int alive = 0, total = 0;
        for (const auto &e : currentScene->GetEntities()) {
          if (!e || e->GetTagInt("wave_id", 0) != active) continue;
          ++total;
          if (!e->HasTag("enemy_defeated")) ++alive;
        }
        ImGui::Text("残敵数           : %d / %d", alive, total);
      }
      Hint("残敵数はスクリプトの内部変数にしか無いため、"
           "同じ条件（wave_id が進行中のウェーブと一致し enemy_defeated が"
           "立っていない）で数え直している。"
           "これが 0 になった直後に累計クリア数が増えれば正しい。");

      ImGui::Spacing();
      static int reqWave = 1;
      ImGui::SetNextItemWidth(120.0f);
      ImGui::InputInt("ウェーブ番号", &reqWave);
      ImGui::SameLine();
      if (ImGui::Button("開始を要求")) {
        mgr->SetTag("wave_request", (std::max)(reqWave, 1));
        Log::Print("[Verify] ウェーブ開始を要求しました");
      }
      ImGui::SameLine();
      // 強制クリアは「どのウェーブを」まで指定しないと受理されない
      // （WaveManager 側が進行中のIDと一致するかを見ている）。
      // 固定値を書くとウェーブ2以降で黙って無反応になるため、進行中のIDを渡す。
      ImGui::BeginDisabled(active == 0);
      if (ImGui::Button("強制クリア")) {
        mgr->SetTag("wave_force_clear", active);
        Log::Print("[Verify] ウェーブを強制クリアしました");
      }
      ImGui::EndDisabled();
      Hint("強制クリアでもレールの再進行まで同じ経路を通るかを見ること。"
           "ここだけ通って実戦で止まるなら、全滅の検知側に問題がある。"
           "戦闘していないあいだは押せない（クリアするウェーブが無いため）。");

      ImGui::Spacing();
      if (ImGui::Button("このエンティティを選択##wave")) {
        currentScene->SetSelectedEntityId(mgr->GetId());
      }
    }

    // スポナーの状況
    auto spawners = FindEntitiesWithScript(currentScene, "WaveSpawnerScript");
    if (!spawners.empty()) {
      ImGui::Spacing();
      ImGui::Text("スポナー %d 基", static_cast<int>(spawners.size()));
      if (ImGui::BeginTable("spawners", 3,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("名前");
        ImGui::TableSetupColumn("担当ウェーブ");
        ImGui::TableSetupColumn("湧かせ終えた");
        ImGui::TableHeadersRow();
        for (auto *s : spawners) {
          // spawn_done は真偽値ではなく「湧かせ終えたウェーブID」。
          // 担当ウェーブと一致して初めて完了なので、そう読み替えて出す。
          const int waveId = s->GetTagInt("spawner_wave_id", 0);
          const int done = s->GetTagInt("spawn_done", 0);
          ImGui::TableNextRow();
          ImGui::TableNextColumn(); ImGui::TextUnformatted(s->GetName().c_str());
          ImGui::TableNextColumn(); ImGui::Text("%d", waveId);
          ImGui::TableNextColumn();
          ImGui::Text("%s", (waveId != 0 && done == waveId) ? "はい" : "いいえ");
        }
        ImGui::EndTable();
      }
    }
  }
}

// ===========================================================================
// タブ: 浮力・船（C-01 / C-02 / T-15）
// ===========================================================================
void DrawBuoyancyTab(Scene *currentScene) {
  if (!currentScene) {
    ImGui::TextDisabled("シーンが読み込まれていません。");
    return;
  }

  // ---- C-01 / C-02 浮力 ----
  auto floaters = FindEntitiesWithScript(currentScene, "BuoyancyScript");
  if (ImGui::CollapsingHeader("C-01 / C-02  浮力",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Text("浮遊物 %d 個", static_cast<int>(floaters.size()));
    Hint("密度比・線形抵抗・2乗抵抗の役割は「2乗抵抗＝着水でどれだけ潜るか、"
         "線形抵抗＝そのあと何回跳ねるか、密度比と物体サイズ＝上下動の周期」。"
         "高い位置から落として、余分な沈み込みが出ないかを見る。");

    if (floaters.empty()) {
      ImGui::TextDisabled("（BuoyancyScript を載せたエンティティが無い）");
    }

    for (auto *e : floaters) {
      ImGui::PushID(static_cast<int>(e->GetId()));
      if (ImGui::TreeNodeEx("##floater", ImGuiTreeNodeFlags_DefaultOpen, "%s",
                            e->GetName().c_str())) {
        auto *script = GetScriptInstance(e, "BuoyancyScript");
        const nlohmann::json j = SafeSerialize(script);

        const float halfHeight = j.value("halfHeight", 1.0f);
        const float density = j.value("densityRatio", 0.45f);
        const float gravity = j.value("gravity", 9.81f);
        const float linearDrag = j.value("linearDrag", 5.5f);
        const float quadDrag = j.value("quadraticDrag", 1.0f);

        // BuoyancyScript の OnImGui と同じ式。型が見えないので値だけ借りて再計算する。
        const float full = (std::max)(halfHeight * 2.0f, 1e-3f);
        const float dens = (std::max)(density, 1e-3f);
        const float omega = std::sqrt((std::max)(gravity, 0.0f) / (dens * full));
        const float period = (omega > 1e-4f) ? (6.28318531f / omega) : 0.0f;
        const float zeta =
            (omega > 1e-4f) ? ((std::max)(linearDrag, 0.0f) * dens) / (2.0f * omega)
                            : 0.0f;
        const char *feel = (zeta < 0.15f)   ? "ぷかぷか跳ね続ける"
                           : (zeta < 0.55f) ? "2〜3回跳ねて収まる（狙い）"
                           : (zeta < 1.0f)  ? "ほとんど跳ねない"
                                            : "跳ねずにじわっと戻る";

        ImGui::Text("密度比 %.2f / 線形抵抗 %.2f / 2乗抵抗 %.2f", density,
                    linearDrag, quadDrag);
        ImGui::Text("上下動の周期 : %.2f s", period);
        ImGui::Text("減衰比       : %.2f  %s", zeta, feel);
        ImGui::Text("水没時の浮力 : %+.2f m/s^2", gravity / dens - gravity);
        ImGui::Text("平衡時の露出 : 水面上に %.0f %%", (1.0f - dens) * 100.0f);

        ImGui::Spacing();
        ImGui::Text("状態タグ: 水中=%d 浸水=%d",
                    e->GetTagInt("in_water", 0), e->GetTagInt("flooding", 0));

        const bool flooding = e->GetTagInt("flooding", 0) != 0;
        if (ImGui::Button(flooding ? "浸水を止める" : "浸水させる")) {
          if (flooding) {
            e->ClearTag("flooding");
          } else {
            e->SetTag("flooding", 1);
          }
        }
        ImGui::SameLine();
        if (ImGui::Button("選択して詳細を開く")) {
          currentScene->SetSelectedEntityId(e->GetId());
        }
        Hint("スライダで追い込むときは Inspector 側の BuoyancyScript を使う。"
             "ここは複数の浮遊物を並べて見比べるための表示。");
        ImGui::TreePop();
      }
      ImGui::PopID();
    }
  }

  ImGui::Spacing();

  // ---- T-15 船 ----
  auto ships = FindEntitiesWithScript(currentScene, "ShipEnemyScript");
  if (ImGui::CollapsingHeader("T-15  新規敵タイプ「船」",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Text("船 %d 隻", static_cast<int>(ships.size()));
    Hint("船は Y と傾きを自分で動かさず、浮力へ任せる作りになっている。"
         "撃沈も即消滅ではなく浸水（flooding）で沈む。"
         "同じエンティティに BuoyancyScript が載っているかも合わせて確認する。");

    if (ships.empty()) {
      ImGui::TextDisabled("（ShipEnemyScript を載せたエンティティが無い）");
    }

    for (auto *e : ships) {
      ImGui::PushID(static_cast<int>(e->GetId()) + 100000);
      const bool hasBuoyancy = GetScriptInstance(e, "BuoyancyScript") != nullptr;
      if (ImGui::TreeNodeEx("##ship", ImGuiTreeNodeFlags_DefaultOpen, "%s%s",
                            e->GetName().c_str(),
                            hasBuoyancy ? "" : "  ← 浮力なし")) {
        ImGui::Text("HP: %d / %d", e->GetTagInt("current_hp", 0),
                    e->GetTagInt("max_hp", 0));
        ImGui::Text("撃沈=%d 浸水=%d ウェーブ=%d",
                    e->GetTagInt("enemy_defeated", 0),
                    e->GetTagInt("flooding", 0), e->GetTagInt("wave_id", 0));

        if (!hasBuoyancy) {
          ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.75f, 0.35f, 1.0f));
          ImGui::TextWrapped("BuoyancyScript が載っていないため、波に乗らず"
                             "撃沈しても沈まない。スポナーの scripts 配列を確認。");
          ImGui::PopStyleColor();
        }

        // 入力値は全船で共有（1つずつ変えたい場面が無いため）。
        static int damage = 5;
        ImGui::SetNextItemWidth(100.0f);
        ImGui::InputInt("ダメージ量（全船共通）", &damage);
        if (ImGui::Button("ダメージを与える")) {
          // pending_damage は 1 フレーム分をまとめて消費する加算式のタグ。
          // 代入にすると、同じフレームに弾が当たっていたぶんを消してしまう。
          e->SetTag("pending_damage",
                    e->GetTagInt("pending_damage", 0) + (std::max)(damage, 1));
        }
        ImGui::SameLine();
        if (ImGui::Button("選択して詳細を開く")) {
          currentScene->SetSelectedEntityId(e->GetId());
        }

        if (auto *script = GetScriptInstance(e, "ShipEnemyScript")) {
          ImGui::Separator();
          ImGui::Indent(8.0f);
          script->OnImGui();
          ImGui::Unindent(8.0f);
        }
        ImGui::TreePop();
      }
      ImGui::PopID();
    }
  }
}

// ===========================================================================
// タブ: リソース（D-04 / D-05 ほか）
// ===========================================================================
void DrawResourceTab(Scene *currentScene) {
  auto &ctx = RC::GetRenderContext();
  auto &sprites = ctx.Sprites();

  const size_t allocated = sprites.AllocatedCount();
  const size_t inUse = sprites.InUseCount();
  const int ptLights = RC::GetActivePointLightCount();
  const int spLights = RC::GetActiveSpotLightCount();
  const int arLights = RC::GetActiveAreaLightCount();
  const size_t entities =
      currentScene ? currentScene->GetEntities().size() : 0u;

  Hint("D-04（死亡エンティティのハンドル解放漏れ）は、数を数えるのが一番早い。"
       "基準を記録してから敵を倒す／シーンを一周し、使用中の数が基準へ戻るかを見る。"
       "戻らずに増え続けるなら、まだどこかで解放できていない。");

  ImGui::Spacing();
  if (ImGui::Button("基準を記録")) {
    g_baseline.taken = true;
    g_baseline.spriteAllocated = allocated;
    g_baseline.spriteInUse = inUse;
    g_baseline.pointLights = ptLights;
    g_baseline.spotLights = spLights;
    g_baseline.areaLights = arLights;
    g_baseline.entities = entities;
    Log::Print("[Verify] リソースの基準値を記録しました");
  }
  ImGui::SameLine();
  if (ImGui::Button("基準を消す")) {
    g_baseline = ResourceBaseline{};
  }
  ImGui::SameLine();
  ImGui::TextDisabled("%s", g_baseline.taken ? "（基準あり）" : "（基準なし）");

  ImGui::Spacing();
  if (ImGui::BeginTable("res", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("項目");
    ImGui::TableSetupColumn("現在");
    ImGui::TableSetupColumn("基準");
    ImGui::TableSetupColumn("差");
    ImGui::TableHeadersRow();

    auto row = [](const char *name, long long now, long long base, bool hasBase) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::TextUnformatted(name);
      ImGui::TableNextColumn(); ImGui::Text("%lld", now);
      ImGui::TableNextColumn();
      if (hasBase) ImGui::Text("%lld", base); else ImGui::TextDisabled("-");
      ImGui::TableNextColumn();
      if (!hasBase) {
        ImGui::TextDisabled("-");
      } else {
        const long long d = now - base;
        if (d > 0) {
          ImGui::TextColored(ImVec4(0.95f, 0.60f, 0.35f, 1.0f), "+%lld", d);
        } else if (d < 0) {
          ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.95f, 1.0f), "%lld", d);
        } else {
          ImGui::TextColored(ImVec4(0.40f, 0.85f, 0.45f, 1.0f), "0");
        }
      }
    };

    const bool b = g_baseline.taken;
    row("スプライト（使用中）", static_cast<long long>(inUse),
        static_cast<long long>(g_baseline.spriteInUse), b);
    row("スプライト（累計確保）", static_cast<long long>(allocated),
        static_cast<long long>(g_baseline.spriteAllocated), b);
    row("点光源", ptLights, g_baseline.pointLights, b);
    row("スポットライト", spLights, g_baseline.spotLights, b);
    row("エリアライト", arLights, g_baseline.areaLights, b);
    row("エンティティ", static_cast<long long>(entities),
        static_cast<long long>(g_baseline.entities), b);

    ImGui::EndTable();
  }

  ImGui::Spacing();
  ImGui::TextDisabled(
      "累計確保はスロットを再利用しない作りのため単調増加する。"
      "見るべきは「使用中」の方。");

  ImGui::Spacing();
  if (ImGui::CollapsingHeader("D-05  スプライトの切り出し矩形",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    Hint("1つのスプライトハンドルは1フレームに1回しか描けない。"
         "位置も色も UV もハンドルごとの定数バッファ1つで持っており、"
         "Draw はコマンドリストへの記録しかしないため、同じハンドルを"
         "位置を変えながら複数回描くと GPU 実行時には全部が最後の状態になる。");

    auto uiEntities = FindEntitiesWithScript(currentScene, "SceneFlowUI");
    ImGui::Text("SceneFlowUI %d 個", static_cast<int>(uiEntities.size()));
    if (!uiEntities.empty()) {
      ImGui::TextDisabled(
          "1つにつき数字8桁ぶん + score.png + バナーでスプライトを確保する。"
          "Result 画面でスコアの各桁が別々の数字に見えていれば直っている。");
    }
    for (auto *e : uiEntities) {
      ImGui::BulletText("%s", e->GetName().c_str());
    }
  }

  ImGui::Spacing();
  if (ImGui::CollapsingHeader("A-05  レベル側のライトとカメラ")) {
    Hint("レベル JSON から読んだライト／カメラがここに出る。"
         "向きが数十倍ずれているなら、rotation_in_degrees の指定と"
         "実データの単位（度数法／ラジアン）が食い違っている。");
    ImGui::Text("シーン内のライト合計: 点 %d / スポット %d / エリア %d",
                ptLights, spLights, arLights);
    ImGui::TextDisabled("平行光源とカメラは Hierarchy 側で確認する。");
  }
}

#endif // RC_ENABLE_IMGUI

} // namespace

// ===========================================================================
// 公開関数
// ===========================================================================

void VerifyPanel::SetSceneRequest(std::function<void(const std::string &)> fn) {
  // 実体は DebugBridge に置く（撮影モードからも同じ口を使うため）
  DebugBridge::SetSceneRequest(std::move(fn));
}

void VerifyPanel::SaveChecklist() {
  // まだ一度も読み込んでいない＝パネルを開かないまま終了した場合。
  // ここで空の g_records を書き出すと前回の記録が消えるので、何もしない。
  if (!g_checklistLoaded) return;

  nlohmann::json j;
  for (int i = 0; i < kItemCount; ++i) {
    nlohmann::json e;
    e["id"] = kItems[i].id;
    e["title"] = kItems[i].title;
    e["state"] = static_cast<int>(g_records[i].state);
    e["note"] = std::string(g_records[i].note);
    j.push_back(e);
  }
  std::ofstream ofs(kChecklistPath);
  if (ofs) {
    ofs << j.dump(2);
    Log::Print("[Verify] チェックリストを保存しました");
  } else {
    Log::Print("[Verify] チェックリストの保存に失敗しました");
  }
}

void VerifyPanel::LoadChecklist() {
  g_checklistLoaded = true;
  std::ifstream ifs(kChecklistPath);
  if (!ifs) return;
  try {
    nlohmann::json j;
    ifs >> j;
    if (!j.is_array()) return;
    // 項目の増減があっても壊れないよう、id で突き合わせる。
    for (const auto &e : j) {
      if (!e.contains("id")) continue;
      const std::string id = e["id"].get<std::string>();
      for (int i = 0; i < kItemCount; ++i) {
        if (id != kItems[i].id) continue;
        if (e.contains("state")) {
          const int s = e["state"].get<int>();
          g_records[i].state =
              static_cast<CheckState>((s >= 0 && s <= 2) ? s : 0);
        }
        if (e.contains("note")) {
          const std::string note = e["note"].get<std::string>();
          std::snprintf(g_records[i].note, sizeof(g_records[i].note), "%s",
                        note.c_str());
        }
        break;
      }
    }
  } catch (...) {
    Log::Print("[Verify] VerifyChecklist.json の読み込みに失敗しました");
  }
}

void VerifyPanel::Draw(bool *open, Scene *currentScene, Dx12Core *core) {
#if RC_ENABLE_IMGUI
  // 初回だけ保存済みのチェック状態を読み戻す。
  if (!g_checklistLoaded) {
    VerifyPanel::LoadChecklist();
  }

  ImGui::SetNextWindowSize(ImVec2(460.0f, 620.0f), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("実装確認 (Verify)", open)) {
    if (ImGui::BeginTabBar("verify_tabs")) {
      if (ImGui::BeginTabItem("チェックリスト")) {
        DrawChecklistTab();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("描画")) {
        DrawRenderTab(core);
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("ゲームループ")) {
        DrawGameLoopTab(currentScene);
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("レール・ウェーブ")) {
        DrawRailWaveTab(currentScene);
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("浮力・船")) {
        DrawBuoyancyTab(currentScene);
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("リソース")) {
        DrawResourceTab(currentScene);
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }
  }
  ImGui::End();
#else
  (void)open;
  (void)currentScene;
  (void)core;
#endif
}
