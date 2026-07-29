#include "ECS/ScriptableEntity.h"
#include "ECS/ScriptRegistry.h"
#include "ECS/TransformComponent.h"
#include "ECS/ModelRendererComponent.h"
#include "ECS/ColliderComponent.h"
#include "Common/EngineConfig.h"
#include "Math/Math.h"
#include "RenderCommon.h"
#include "Scene.h"
#include "CharacterTags.h"
#include <cmath>
#include <iostream>
#include <string>

#if RC_ENABLE_IMGUI
#include "imgui/imgui.h"
#endif

/// @brief 鍬（Hoe）アイテムのスクリプト
/// @details 落ちている間はその場で回転し、農民が触れると入手して手のボーンに追従させる。
///          ボーン追従は「Joint のスケルトン空間行列 × 所有者のワールド行列」で
///          ワールド行列を直接組み立て、RC::SetModelWorldOverride で描画に渡している。
///          TransformComponent（TRS）経由だと行列をオイラー角へ分解する必要があり、
///          ジンバルロックや分解誤差でガタつくため、行列をそのまま渡す経路を使う。
class HoeItem : public ScriptableEntity {
public:
    // --- 落ちている状態の見た目 ---
    float spinSpeed = 1.5f;        ///< Y軸回転速度 (rad/s)
    float bobHeight = 0.12f;       ///< 上下に浮遊する振幅 (m)。0で浮遊なし
    float bobSpeed  = 2.0f;        ///< 浮遊の速さ (rad/s)

    // --- 入手設定 ---
    std::string ownerName = "Farmer";       ///< 拾う側のエンティティ名
    std::string handJoint = "R_Hand";       ///< 片手持ちで追従させるジョイント名
    std::string handJointSecond = "L_Hand"; ///< 両手持ちのときのもう一方の手

    /// @brief 手のジョイントから握り位置（仮想ソケット）までのオフセット (m)
    /// @details farmer.glb の R_Hand / L_Hand は手首にあり、指のボーンが存在しないため
    ///          「手のひらで握る位置」を示すジョイントがない。
    ///          そこでモデルを改造せず、手のジョイントのローカル座標系でこの分だけ
    ///          進めた点を仮想的なソケット（追加ボーン相当）として扱う。
    ///          このリグはボーンが親のローカル +Y 方向へ伸びる規約（R_Hand の
    ///          translation が (0, 0.138, 0)）なので、+Y が手首→指先の方向になる。
    ///          左右どちらの手にも同じオフセットが適用される。
    RC::Vector3 handSocketOffset = {0.0f, 0.05f, 0.0f};

    // --- 装備時のオフセット（ジョイント基準のローカル変換） ---
    /// @brief 位置オフセット (m)
    /// @details Hoe.glb の柄はローカル原点から +Y に約0.15ズレた位置を通っている
    ///          （スケール0.6で約0.09m）。equipRotation の Z=π で柄まわりに180°回すと
    ///          そのズレが手の -Y 側（＝肘側）に出てしまうため、+0.09 戻して
    ///          柄の中心線を手のジョイントに重ねる。
    RC::Vector3 equipOffset   = {0.0f, 0.09f, 0.0f};
    /// @brief 回転オフセット (rad)
    /// @details Y=π で柄の前後を反転、Z=π で柄まわりに180°回して刃の向く面を反転させている。
    ///          柄はローカル Z 軸に沿っているため、Z 回転が「柄まわりのロール」になる。
    RC::Vector3 equipRotation = {0.0f, 3.14159265f, 3.14159265f};
    float equipScale = 0.6f;                          ///< 装備時のスケール倍率

    // --- 両手持ち設定 ---
    /// @brief 両手が近づいたときに柄を両手の間に通すか
    bool twoHandEnabled = true;
    /// @brief 攻撃モーション中のみ両手持ちにするか（待機中の手の位置で誤発動するのを防ぐ）
    bool twoHandOnlyWhileAttacking = true;
    float twoHandNearDist = 0.24f;   ///< 両手間の距離がこれ以下で完全に両手持ち (m)
    float twoHandFarDist  = 0.32f;   ///< これ以上で片手持ちに戻る (m)
    float twoHandGripT = 0.5f;       ///< 柄の原点を置く位置 (0=右手, 0.5=中間, 1=左手)

    /// @brief 柄の向きを片手持ちの向きに自動で合わせるか
    /// @details true なら「両手を結ぶ軸」の符号を、片手持ち時の柄の向きと同じ側になるよう選ぶ。
    ///          これをやらないと片手持ち→両手持ちに切り替わる瞬間に鍬が前後反転して見える。
    ///          モデルのどちら側が刃かを気にしなくて済むので既定で有効。
    bool twoHandAutoAxis = true;

    /// @brief 刃を左手側に向けるか（twoHandAutoAxis が false のときのみ有効）
    /// @details 刃はエンジン空間ではローカル +Z 側にある。
    ///          Hoe.glb 自体は z=-0.5 が刃だが、ModelMesh が aiProcess_MakeLeftHanded で
    ///          読み込むため Assimp が Z 軸を反転しており、エンジン内では +Z が刃になる。
    bool twoHandBladeTowardLeft = true;
    /// @brief 両手持ち時の位置オフセット (m)
    /// @details Hoe.glb の柄はローカル原点から +Y に約0.15ズレた位置を通っている
    ///          （スケール0.6で約0.09m）。twoHandRotation で柄まわりに180°回すと
    ///          そのズレが -Y 側になるため、+0.09 上げて柄を両手の線に合わせる。
    RC::Vector3 twoHandOffset   = {0.0f, 0.09f, 0.0f};
    /// @brief 両手持ち時の回転オフセット (rad)
    /// @details Z=π で柄まわりに180°回し、刃の向く面を反転させている
    RC::Vector3 twoHandRotation = {0.0f, 0.0f, 3.14159265f};

    /// @brief 移動中（Walk/Run）に鍬の描画を消すか
    bool hideWhileMoving = true;

    /// @brief 手に炎が出ている間、鍬の描画を消すか
    /// @details 炎は鍬を握っているのと同じ R_Hand から出るため、
    ///          消さないと柄と炎が重なって見栄えが悪い。
    ///          点火状態は FarmerController が Entity タグ "farmerFire" に書き込んでいる。
    bool hideWhileFire = true;

protected:
    void OnCreate() override {
        std::cout << "[HoeItem] OnCreate\n";
        equipped_ = false;
        bobTimer_ = 0.0f;
        if (auto* tr = GetComponent<TransformComponent>()) {
            groundY_ = tr->position.y;
        }
    }

    void OnUpdate(float deltaTime) override {
        auto* tr = GetComponent<TransformComponent>();
        auto* ren = GetComponent<ModelRendererComponent>();
        if (!tr || !ren) return;

        if (!equipped_) {
            UpdateAsWorldItem(tr, ren, deltaTime);
        } else {
            UpdateAsEquipped(tr, ren);
        }
    }

    /// @brief 接触で入手する
    /// @details Hoe 側のコライダーを isTrigger にしておくことで、
    ///          押し出し（物理解決）は起きず通知だけが飛んでくる。
    void OnCollision(Entity* other, const RC::Vector3& /*contactPoint*/) override {
        if (equipped_ || !other) return;
        if (other->GetName() != ownerName) return;
        Equip(other);
    }

    void OnDestroy() override {
        // 上書きを残したままモデルを解放しないよう後始末する
        if (auto* ren = GetComponent<ModelRendererComponent>()) {
            if (ren->HasModel()) RC::ClearModelWorldOverride(ren->modelHandle);
        }
    }

public:
    void OnImGui() override {
#if RC_ENABLE_IMGUI
        ImGui::Text("State: %s", equipped_ ? "Equipped (装備中)" : "OnGround (落ちている)");
        ImGui::Text("Owner: %s / Joint: %s", ownerName.c_str(), handJoint.c_str());
        if (equipped_ && !jointFound_) {
            ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "ジョイント '%s' が見つかりません", handJoint.c_str());
        }
        ImGui::Separator();
        ImGui::Text("落下時:");
        ImGui::DragFloat("Spin Speed", &spinSpeed, 0.1f, 0.0f, 20.0f);
        ImGui::DragFloat("Bob Height", &bobHeight, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Bob Speed", &bobSpeed, 0.1f, 0.0f, 20.0f);
        ImGui::Separator();
        ImGui::Text("握り位置（仮想ソケット / 手のローカル基準）:");
        ImGui::DragFloat3("Hand Socket", &handSocketOffset.x, 0.005f, -0.5f, 0.5f);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("R_Hand は手首にあり指のボーンが無いため、\nY を増やすと手首→指先方向へ握り位置が移動する");
        }
        ImGui::Separator();
        ImGui::Text("装備時オフセット (ソケット基準):");
        ImGui::DragFloat3("Offset Pos", &equipOffset.x, 0.01f, -2.0f, 2.0f);
        ImGui::SliderAngle("Offset Rot X", &equipRotation.x);
        ImGui::SliderAngle("Offset Rot Y", &equipRotation.y);
        ImGui::SliderAngle("Offset Rot Z", &equipRotation.z);
        ImGui::DragFloat("Equip Scale", &equipScale, 0.01f, 0.01f, 5.0f);
        ImGui::Checkbox("移動中は非表示 (Walk/Run)", &hideWhileMoving);
        ImGui::Checkbox("手に炎が出ている間は非表示", &hideWhileFire);
        ImGui::Text("炎: %s", ownerFiring_ ? "点火中" : "消火");
        ImGui::Separator();
        ImGui::Text("両手持ち: 適用度 %.2f", twoHandBlend_);
        ImGui::Checkbox("両手持ちを有効にする", &twoHandEnabled);
        ImGui::Checkbox("攻撃中のみ", &twoHandOnlyWhileAttacking);
        ImGui::Checkbox("柄の向きを片手持ちに自動で合わせる", &twoHandAutoAxis);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("切り替わりで鍬が前後反転しないよう向きを自動で揃える");
        }
        if (!twoHandAutoAxis) {
            ImGui::Checkbox("刃を左手側に向ける", &twoHandBladeTowardLeft);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("柄の前後が逆なら切り替える");
            }
        }
        ImGui::DragFloat("2H Near Dist", &twoHandNearDist, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("2H Far Dist", &twoHandFarDist, 0.01f, 0.0f, 1.0f);
        ImGui::SliderFloat("2H Grip (右手0 - 左手1)", &twoHandGripT, 0.0f, 1.0f);
        ImGui::DragFloat3("2H Offset Pos", &twoHandOffset.x, 0.01f, -2.0f, 2.0f);
        ImGui::SliderAngle("2H Rot X", &twoHandRotation.x);
        ImGui::SliderAngle("2H Rot Y", &twoHandRotation.y);
        ImGui::SliderAngle("2H Rot Z (柄まわり)", &twoHandRotation.z);
        ImGui::Separator();
        if (!equipped_) {
            if (ImGui::Button("強制的に装備する")) {
                if (Entity* owner = FindOwner()) Equip(owner);
            }
        } else {
            if (ImGui::Button("その場に落とす")) Drop();
        }
#endif
    }

    nlohmann::json Serialize() override {
        return {
            {"spinSpeed", spinSpeed},
            {"bobHeight", bobHeight},
            {"bobSpeed", bobSpeed},
            {"ownerName", ownerName},
            {"handJoint", handJoint},
            {"handSocketOffset", {handSocketOffset.x, handSocketOffset.y, handSocketOffset.z}},
            {"equipOffset", {equipOffset.x, equipOffset.y, equipOffset.z}},
            {"equipRotation", {equipRotation.x, equipRotation.y, equipRotation.z}},
            {"equipScale", equipScale},
            {"hideWhileMoving", hideWhileMoving},
            {"hideWhileFire", hideWhileFire},
            {"handJointSecond", handJointSecond},
            {"twoHandEnabled", twoHandEnabled},
            {"twoHandOnlyWhileAttacking", twoHandOnlyWhileAttacking},
            {"twoHandNearDist", twoHandNearDist},
            {"twoHandFarDist", twoHandFarDist},
            {"twoHandGripT", twoHandGripT},
            {"twoHandAutoAxis", twoHandAutoAxis},
            {"twoHandBladeTowardLeft", twoHandBladeTowardLeft},
            {"twoHandOffset", {twoHandOffset.x, twoHandOffset.y, twoHandOffset.z}},
            {"twoHandRotation", {twoHandRotation.x, twoHandRotation.y, twoHandRotation.z}}
        };
    }

    void Deserialize(const nlohmann::json& j) override {
        if (j.contains("spinSpeed")) spinSpeed = j["spinSpeed"].get<float>();
        if (j.contains("bobHeight")) bobHeight = j["bobHeight"].get<float>();
        if (j.contains("bobSpeed")) bobSpeed = j["bobSpeed"].get<float>();
        if (j.contains("ownerName")) ownerName = j["ownerName"].get<std::string>();
        if (j.contains("handJoint")) handJoint = j["handJoint"].get<std::string>();
        if (j.contains("handSocketOffset")) {
            auto& v = j["handSocketOffset"];
            handSocketOffset = {v[0].get<float>(), v[1].get<float>(), v[2].get<float>()};
        }
        if (j.contains("equipOffset")) {
            auto& v = j["equipOffset"];
            equipOffset = {v[0].get<float>(), v[1].get<float>(), v[2].get<float>()};
        }
        if (j.contains("equipRotation")) {
            auto& v = j["equipRotation"];
            equipRotation = {v[0].get<float>(), v[1].get<float>(), v[2].get<float>()};
        }
        if (j.contains("equipScale")) equipScale = j["equipScale"].get<float>();
        if (j.contains("hideWhileMoving")) hideWhileMoving = j["hideWhileMoving"].get<bool>();
        if (j.contains("hideWhileFire")) hideWhileFire = j["hideWhileFire"].get<bool>();
        if (j.contains("handJointSecond")) handJointSecond = j["handJointSecond"].get<std::string>();
        if (j.contains("twoHandEnabled")) twoHandEnabled = j["twoHandEnabled"].get<bool>();
        if (j.contains("twoHandOnlyWhileAttacking")) twoHandOnlyWhileAttacking = j["twoHandOnlyWhileAttacking"].get<bool>();
        if (j.contains("twoHandNearDist")) twoHandNearDist = j["twoHandNearDist"].get<float>();
        if (j.contains("twoHandFarDist")) twoHandFarDist = j["twoHandFarDist"].get<float>();
        if (j.contains("twoHandGripT")) twoHandGripT = j["twoHandGripT"].get<float>();
        if (j.contains("twoHandAutoAxis")) twoHandAutoAxis = j["twoHandAutoAxis"].get<bool>();
        if (j.contains("twoHandBladeTowardLeft")) twoHandBladeTowardLeft = j["twoHandBladeTowardLeft"].get<bool>();
        if (j.contains("twoHandOffset")) {
            auto& v = j["twoHandOffset"];
            twoHandOffset = {v[0].get<float>(), v[1].get<float>(), v[2].get<float>()};
        }
        if (j.contains("twoHandRotation")) {
            auto& v = j["twoHandRotation"];
            twoHandRotation = {v[0].get<float>(), v[1].get<float>(), v[2].get<float>()};
        }
    }

private:
    bool equipped_ = false;     ///< 入手済みか
    bool jointFound_ = false;   ///< 直近のフレームでジョイントを取得できたか
    bool warnedJoint_ = false;  ///< ジョイント名の警告を出したか（毎フレーム出さない）
    bool ownerFiring_ = false;  ///< 直近のフレームで所有者の手に炎が出ていたか（ImGui表示用）
    float bobTimer_ = 0.0f;     ///< 浮遊アニメーションの位相
    float groundY_ = 0.0f;      ///< 設置時のY座標（浮遊の基準）
    float twoHandBlend_ = 0.0f; ///< 両手持ちの適用度（0=片手 1=両手）。ImGui表示用
    bool twoHandActive_ = false; ///< 両手持ちに入っているか（軸の符号を固定するために保持）
    int twoHandAxisSign_ = 1;    ///< 両手持ちに入ったときに確定した柄の向き

    /// @brief 落ちている状態の更新（回転＋浮遊）
    void UpdateAsWorldItem(TransformComponent* tr, ModelRendererComponent* ren, float deltaTime) {
        tr->rotation.y += spinSpeed * deltaTime;
        // 角度が際限なく増えて float 精度が落ちるのを防ぐ
        constexpr float kTwoPi = 6.28318530718f;
        if (tr->rotation.y > kTwoPi) tr->rotation.y -= kTwoPi;

        if (bobHeight > 0.0f) {
            bobTimer_ += bobSpeed * deltaTime;
            if (bobTimer_ > kTwoPi) bobTimer_ -= kTwoPi;
            tr->position.y = groundY_ + std::sin(bobTimer_) * bobHeight;
        }

        ren->visible = true;
        twoHandBlend_ = 0.0f;
        twoHandActive_ = false;
        if (ren->HasModel()) RC::ClearModelWorldOverride(ren->modelHandle);
    }

    /// @brief 装備状態の更新（手のボーンに追従）
    void UpdateAsEquipped(TransformComponent* tr, ModelRendererComponent* ren) {
        Entity* owner = FindOwner();
        if (!owner) return;
        auto* ownerTr = owner->GetComponent<TransformComponent>();
        auto* ownerRen = owner->GetComponent<ModelRendererComponent>();
        if (!ownerTr || !ownerRen || !ownerRen->HasModel()) return;

        // 移動中（Walk/Run）と、手に炎が出ている間は描画を消す
        const bool ownerMoving = (owner->GetTagInt(kTagFarmerMoving, 0) != 0);
        ownerFiring_ = (owner->GetTagInt(kTagFarmerFire, 0) != 0);
        const bool hidden = (hideWhileMoving && ownerMoving) || (hideWhileFire && ownerFiring_);
        ren->visible = !hidden;

        if (!ren->HasModel()) return;

        // ジョイントのスケルトン空間行列（モデルローカル）
        RC::Matrix4x4 jointR{};
        jointFound_ = RC::GetModelJointMatrix(ownerRen->modelHandle, handJoint, jointR);
        if (!jointFound_) {
            if (!warnedJoint_) {
                warnedJoint_ = true;
                std::cout << "[HoeItem] ジョイント '" << handJoint << "' が見つかりません。利用可能なジョイント:\n";
                for (const std::string& n : RC::GetModelJointNames(ownerRen->modelHandle)) {
                    std::cout << "    " << n << "\n";
                }
            }
            return;
        }

        // 手のワールド姿勢 = ジョイント × 所有者のワールド
        // （この engine の Multiply(m1, m2) は m1 を先に適用する行ベクトル規約）
        const RC::Matrix4x4 ownerWorld = ownerTr->GetWorldMatrix();
        // 手のジョイント（手首）から仮想ソケット（握り位置）へずらす
        const RC::Matrix4x4 rWorld = ApplyHandSocket(Multiply(jointR, ownerWorld));

        // --- 片手持ち（右手のジョイント姿勢そのまま） ---
        const RC::Vector3 scale = {equipScale, equipScale, equipScale};
        const RC::Matrix4x4 singleWorld =
            Multiply(MakeAffineMatrix(scale, equipRotation, equipOffset), rWorld);

        RC::Matrix4x4 world = singleWorld;
        twoHandBlend_ = 0.0f;

        // --- 両手持ち（柄を両手の間に通す） ---
        // 片手のジョイントに親子付けしたままでは、振り上げでもう一方の手が柄から離れて見える。
        // 両手が近づいている間は「両手を結ぶ直線」を柄の軸として姿勢を作り直す。
        RC::Matrix4x4 jointL{};
        if (twoHandEnabled &&
            RC::GetModelJointMatrix(ownerRen->modelHandle, handJointSecond, jointL)) {
            const RC::Matrix4x4 lWorld = ApplyHandSocket(Multiply(jointL, ownerWorld));
            twoHandBlend_ = ComputeTwoHandBlend(owner, rWorld, lWorld);
            if (twoHandBlend_ > 0.0f) {
                // 柄の向き（軸の符号）は両手持ちに入った最初のフレームで決めて固定する。
                // 毎フレーム判定すると、スイング中に手の向きが変わって判定が反転し、
                // 振っている最中に鍬がくるっと裏返ってしまう。
                if (!twoHandActive_) {
                    twoHandAxisSign_ = DetermineAxisSign(rWorld, lWorld, GetAxis(singleWorld, 2));
                    twoHandActive_ = true;
                }
                const RC::Matrix4x4 basis = BuildTwoHandBasis(rWorld, lWorld, twoHandAxisSign_);
                const RC::Matrix4x4 bothWorld =
                    Multiply(MakeAffineMatrix(scale, twoHandRotation, twoHandOffset), basis);
                world = (twoHandBlend_ >= 1.0f)
                    ? bothWorld
                    : BlendWorld(singleWorld, bothWorld, twoHandBlend_);
            } else {
                twoHandActive_ = false; // 次に入るときに符号を再判定させる
            }
        }

        RC::SetModelWorldOverride(ren->modelHandle, world);

        // 当たり判定やギズモが地面に置き去りにならないよう、位置だけ追従させておく
        tr->position = {world.m[3][0], world.m[3][1], world.m[3][2]};
    }

    // ============================================================
    // 両手持ちのための行列ユーティリティ
    // ============================================================

    /// @brief 手のジョイントに仮想ソケット（追加ボーン相当）のオフセットを適用する
    /// @param handWorld 手のジョイントのワールド行列
    /// @return ソケット位置に移したワールド行列（姿勢は手のまま）
    /// @details オフセットは手のローカル座標系で加算されるので、
    ///          手が回転すれば握り位置も一緒に回る（子ボーンと同じ挙動）。
    RC::Matrix4x4 ApplyHandSocket(const RC::Matrix4x4& handWorld) const {
        if (std::abs(handSocketOffset.x) < 1.0e-6f &&
            std::abs(handSocketOffset.y) < 1.0e-6f &&
            std::abs(handSocketOffset.z) < 1.0e-6f) {
            return handWorld;
        }
        return Multiply(MakeTranslateMatrix(handSocketOffset), handWorld);
    }

    /// @brief 行列から平行移動成分を取り出す
    static RC::Vector3 GetTranslation(const RC::Matrix4x4& m) {
        return {m.m[3][0], m.m[3][1], m.m[3][2]};
    }

    /// @brief 行列の指定行（基底軸）を取り出す
    static RC::Vector3 GetAxis(const RC::Matrix4x4& m, int row) {
        return {m.m[row][0], m.m[row][1], m.m[row][2]};
    }

    /// @brief 両手持ちの適用度を求める（0=片手, 1=両手）
    /// @details 両手間の距離で連続的に切り替える。距離を使うことで
    ///          「振り上げで手が合わさる瞬間」に自然に両手持ちへ移行し、
    ///          離れていくときも滑らかに戻る（切り替えのパチつきが出ない）。
    float ComputeTwoHandBlend(Entity* owner, const RC::Matrix4x4& rWorld,
                              const RC::Matrix4x4& lWorld) const {
        if (twoHandOnlyWhileAttacking) {
            if (!owner || owner->GetTagInt(kTagFarmerAttacking, 0) == 0) return 0.0f;
        }
        const float dist = RC::Length(RC::Sub(GetTranslation(lWorld), GetTranslation(rWorld)));
        const float nearD = (std::min)(twoHandNearDist, twoHandFarDist);
        const float farD  = (std::max)(twoHandNearDist, twoHandFarDist);
        if (dist <= nearD) return 1.0f;
        if (dist >= farD)  return 0.0f;
        const float u = (farD - dist) / (farD - nearD);
        return u * u * (3.0f - 2.0f * u); // smoothstep
    }

    /// @brief 柄をどちら向きに置くかを決める
    /// @param refZ 片手持ち時の柄の向き（ローカル+Zのワールド方向）
    /// @return +1 なら 右手→左手 方向、-1 なら逆向き
    /// @details 自動判定では「片手持ちのときの柄の向きと同じ側」を選ぶ。
    ///          これで片手持ち⇄両手持ちの切り替わりに前後反転が起きない。
    int DetermineAxisSign(const RC::Matrix4x4& rWorld, const RC::Matrix4x4& lWorld,
                          const RC::Vector3& refZ) const {
        if (!twoHandAutoAxis) return twoHandBladeTowardLeft ? 1 : -1;
        const RC::Vector3 axis = RC::Sub(GetTranslation(lWorld), GetTranslation(rWorld));
        return (RC::Dot(axis, refZ) < 0.0f) ? -1 : 1;
    }

    /// @brief 両手を結ぶ直線を柄の軸とした姿勢行列を作る
    /// @param rWorld 右手のワールド行列
    /// @param lWorld 左手のワールド行列
    /// @param axisSign 柄の向き（DetermineAxisSign の戻り値）
    /// @details モデルのローカル +Z を両手を結ぶ軸に合わせる。
    ///          ロール（柄まわりの回転＝刃の向く方向）は右手のY軸を基準にする。
    RC::Matrix4x4 BuildTwoHandBasis(const RC::Matrix4x4& rWorld,
                                    const RC::Matrix4x4& lWorld,
                                    int axisSign) const {
        const RC::Vector3 rPos = GetTranslation(rWorld);
        const RC::Vector3 lPos = GetTranslation(lWorld);

        // ローカル +Z が向く方向
        const RC::Vector3 axis =
            RC::Mul(RC::Sub(lPos, rPos), (axisSign < 0) ? -1.0f : 1.0f);
        const float len = RC::Length(axis);
        if (len < 1.0e-4f) return rWorld; // 両手がほぼ同一点。姿勢を作れないので右手基準に退避

        const RC::Vector3 zAxis = RC::Mul(axis, 1.0f / len);

        // ロールの基準ベクトル。柄と平行に近い場合は別の軸に切り替える
        RC::Vector3 upHint = Normalize(GetAxis(rWorld, 1));
        if (std::abs(RC::Dot(upHint, zAxis)) > 0.99f) {
            upHint = Normalize(GetAxis(rWorld, 0));
        }

        const RC::Vector3 xAxis = Normalize(Cross(upHint, zAxis));
        const RC::Vector3 yAxis = Cross(zAxis, xAxis); // 単位ベクトル同士の外積なので正規化不要

        // 柄の原点を両手の間のどこに置くか
        const float t = (std::max)(0.0f, (std::min)(1.0f, twoHandGripT));
        const RC::Vector3 origin = RC::Add(rPos, RC::Mul(RC::Sub(lPos, rPos), t));

        RC::Matrix4x4 basis = MakeIdentity4x4();
        basis.m[0][0] = xAxis.x; basis.m[0][1] = xAxis.y; basis.m[0][2] = xAxis.z;
        basis.m[1][0] = yAxis.x; basis.m[1][1] = yAxis.y; basis.m[1][2] = yAxis.z;
        basis.m[2][0] = zAxis.x; basis.m[2][1] = zAxis.y; basis.m[2][2] = zAxis.z;
        basis.m[3][0] = origin.x; basis.m[3][1] = origin.y; basis.m[3][2] = origin.z;
        return basis;
    }

    /// @brief 2つのワールド行列を補間する
    /// @details 各成分を線形補間したうえで基底を再直交化する。
    ///          単純な成分補間だけだと基底が斜交してモデルが歪むため、
    ///          スケール量を保ったまま直交化し直している。
    static RC::Matrix4x4 BlendWorld(const RC::Matrix4x4& a, const RC::Matrix4x4& b, float t) {
        RC::Matrix4x4 r{};
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                r.m[i][j] = a.m[i][j] + (b.m[i][j] - a.m[i][j]) * t;
            }
        }
        // 補間後のスケール（両者は同じ倍率なので長さの補間で十分）
        const float scaleA = RC::Length(GetAxis(a, 0));
        const float scaleB = RC::Length(GetAxis(b, 0));
        const float scale = scaleA + (scaleB - scaleA) * t;

        RC::Vector3 z = GetAxis(r, 2);
        RC::Vector3 x = GetAxis(r, 0);
        // 2つの姿勢がちょうど正反対だと補間結果の軸が消えて姿勢を復元できない。
        // その場合は潰れた行列を返さず、近い方の姿勢をそのまま使う。
        if (RC::Length(z) < 1.0e-4f || RC::Length(x) < 1.0e-4f) return (t < 0.5f) ? a : b;
        z = Normalize(z);
        // グラム・シュミット直交化: x から z 方向成分を除く
        x = RC::Sub(x, RC::Mul(z, RC::Dot(x, z)));
        if (RC::Length(x) < 1.0e-6f) return r;
        x = Normalize(x);
        const RC::Vector3 y = Cross(z, x);

        const RC::Vector3 sx = RC::Mul(x, scale);
        const RC::Vector3 sy = RC::Mul(y, scale);
        const RC::Vector3 sz = RC::Mul(z, scale);
        r.m[0][0] = sx.x; r.m[0][1] = sx.y; r.m[0][2] = sx.z;
        r.m[1][0] = sy.x; r.m[1][1] = sy.y; r.m[1][2] = sy.z;
        r.m[2][0] = sz.x; r.m[2][1] = sz.y; r.m[2][2] = sz.z;
        return r;
    }

    /// @brief 所有者エンティティを名前で探す
    Entity* FindOwner() {
        Scene* scene = GetScene();
        if (!scene) return nullptr;
        for (auto& e : scene->GetEntities()) {
            if (e && e->GetName() == ownerName) return e.get();
        }
        return nullptr;
    }

    /// @brief 入手して装備する
    void Equip(Entity* owner) {
        equipped_ = true;
        warnedJoint_ = false;
        std::cout << "[HoeItem] " << ownerName << " が鍬を入手しました\n";

        // 再度拾わないようコライダーを切る
        if (auto* col = GetComponent<ColliderComponent>()) {
            col->SetEnabled(false);
        }
        if (owner) owner->SetTag(kTagFarmerHasHoe, 1);
    }

    /// @brief 装備を解除してその場に落とす（デバッグ用）
    void Drop() {
        equipped_ = false;
        if (auto* ren = GetComponent<ModelRendererComponent>()) {
            if (ren->HasModel()) RC::ClearModelWorldOverride(ren->modelHandle);
            ren->visible = true;
        }
        if (auto* col = GetComponent<ColliderComponent>()) {
            col->SetEnabled(true);
        }
        if (auto* tr = GetComponent<TransformComponent>()) {
            groundY_ = tr->position.y;
        }
        if (Entity* owner = FindOwner()) owner->SetTag(kTagFarmerHasHoe, 0);
    }
};

REGISTER_SCRIPT(HoeItem)
