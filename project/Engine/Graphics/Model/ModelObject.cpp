#include "ModelObject.h"
#include "Common/Log/Log.h"
#include "Render/FrameResource.h"
#include "Texture/TextureManager/TextureManager.h"
#include "imgui/imgui.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <filesystem>

using namespace RC;
namespace fs = std::filesystem;

void ModelObject::Initialize(ID3D12Device *device) {
  resource_.Initialize(device);

  // 初期ライティングを反映
  ApplyLightingIfReady_();
}

void ModelObject::EnsureSphericalUVIfMissing() {
  if (resource_.GetMesh()) {
    resource_.GetMesh()->EnsureSphericalUVIfMissing();
  }
}

void ModelObject::SetColor(const Vector4 &color) {
  if (Material *mat = resource_.Mat()) {
    mat->color = color;
  }
}

void ModelObject::Update(const Matrix4x4 &view, const Matrix4x4 &proj) {
  cachedView_ = view;
  cachedProj_ = proj;
  hasVP_ = true;
}

void ModelObject::Draw(ID3D12GraphicsCommandList *cmdList,
                       const Matrix4x4 &world, FrameResource &frame) {
  if (!visible_)
    return;

  // View/Proj が無いなら単位行列で描く（事故防止）
  const Matrix4x4 view = hasVP_ ? cachedView_ : MakeIdentity4x4();
  const Matrix4x4 proj = hasVP_ ? cachedProj_ : MakeIdentity4x4();

  // スキニングモデルの場合
  if (HasSkinData()) {
    // CS スキニングが有効かつ Dispatch 済みの場合
    if (resource_.HasCSSkinning() && resource_.IsSkinningDispatched()) {
      // CS スキニング済み頂点で通常描画（object3d パイプラインで描画）
      resource_.DrawSkinnedCS(cmdList, world, view, proj, frame);
    } else if (!resource_.HasCSSkinning()) {
      // フォールバック: 従来の VS スキニング
      resource_.DrawSkinned(cmdList, world, view, proj, skinMatrices_, frame);
    }
    // CS 有効だが未 Dispatch → スキップ（次フレームで描画）
  } else {
    resource_.Draw(cmdList, world, view, proj, frame);
  }
}

void ModelObject::DrawBatch(ID3D12GraphicsCommandList *cmdList,
                            const Matrix4x4 &view, const Matrix4x4 &proj,
                            const std::vector<Transform> &instances,
                            FrameResource &frame) {
  resource_.DrawBatch(cmdList, view, proj, instances, frame);
}

void ModelObject::DrawBatch(ID3D12GraphicsCommandList *cmdList,
                            const Matrix4x4 &view, const Matrix4x4 &proj,
                            const std::vector<Transform> &instances,
                            const RC::Vector4 &color,
                            FrameResource &frame) {
  resource_.DrawBatch(cmdList, view, proj, instances, color, frame);
}

ModelObject &ModelObject::SetLightingConfig(LightingMode mode,
                                            const std::array<float, 3> &color,
                                            const std::array<float, 3> &dir,
                                            float intensity) {
  initialLighting_.mode = mode;
  initialLighting_.color[0] = color[0];
  initialLighting_.color[1] = color[1];
  initialLighting_.color[2] = color[2];

  initialLighting_.dir[0] = dir[0];
  initialLighting_.dir[1] = dir[1];
  initialLighting_.dir[2] = dir[2];

  initialLighting_.intensity = intensity;

  ApplyLightingIfReady_();
  return *this;
}

void ModelObject::ApplyLightingIfReady_() {
  resource_.ApplyLighting(static_cast<int>(initialLighting_.mode),
                          initialLighting_.color, initialLighting_.dir,
                          initialLighting_.intensity);

  // 環境マップ係数を再適用（Initialize後のリセット対策）
  if (Material *mat = resource_.Mat()) {
    mat->environmentCoefficient = initialEnvCoeff_;
  }
}

// ============================================================================
// ImGui
// ============================================================================

// ツールチップ用のヘルパ
static void HelpMarker(const char *desc) {
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
    ImGui::TextUnformatted(desc);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

// 長い文字列：折り返し + ホバーで全文 + クリックでコピー
static void ShowLongTextJP(const std::string &s) {
  const char *text = s.empty() ? "(なし)" : s.c_str();

  // 今いるセル幅で折り返す
  const float wrap = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
  ImGui::PushTextWrapPos(wrap);
  ImGui::TextUnformatted(text);
  ImGui::PopTextWrapPos();

  // ホバーで全文だけ出す（コピーはしない）
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

void ModelObject::DrawImGui(const char *name, bool showLightingUi) {
#if RC_ENABLE_IMGUI

  const auto &mesh_ = resource_.GetMesh();

  std::string label = name ? std::string(name) : std::string("ModelObject");
  if (!ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
    return;

  bool vis = Visible();
  if (ImGui::Checkbox((std::string("表示##") + label).c_str(), &vis))
    SetVisible(vis);

  if (vis) {
    ImGui::TextUnformatted("Transform");
    ImGui::DragFloat3((std::string("位置(x,y,z)##") + label).c_str(),
                      &transform_.translation.x, 0.1f, -4096.0f, 4096.0f,
                      "%.2f");
    ImGui::SliderAngle((std::string("回転X##") + label).c_str(),
                       &transform_.rotation.x);
    ImGui::SliderAngle((std::string("回転Y##") + label).c_str(),
                       &transform_.rotation.y);
    ImGui::SliderAngle((std::string("回転Z##") + label).c_str(),
                       &transform_.rotation.z);
    ImGui::DragFloat3((std::string("スケール(x,y,z)##") + label).c_str(),
                      &transform_.scale.x, 0.01f, 0.001f, 4096.0f, "%.3f");

    ImGui::Dummy(ImVec2(0, 6));

    DirectionalLight *light = resource_.Light();
    Material *mat = resource_.Mat();

    if (showLightingUi) {
      if (light && mat) {
        ImGui::TextUnformatted("Lighting / Material");
        ImGui::ColorEdit3((std::string("ライト色##") + label).c_str(),
                          &light->color.x, ImGuiColorEditFlags_Float);
        ImGui::DragFloat3((std::string("ライト方向##") + label).c_str(),
                          &light->direction.x, 0.01f, -1.0f, 1.0f);
        ImGui::DragFloat((std::string("強さ##") + label).c_str(),
                         &light->intensity, 0.01f, 0.0f, 64.0f);

        ImGui::Dummy(ImVec2(0, 4));
        ImGui::ColorEdit4((std::string("カラー(乗算)##") + label).c_str(),
                          &mat->color.x, ImGuiColorEditFlags_Float);
      } else {
        ImGui::TextDisabled("Light / Material CB not ready.");
      }
      ImGui::Dummy(ImVec2(0, 6));

      if (mat && mat->lightingMode != None) {
        ImGui::DragFloat((std::string("Shininess##") + label).c_str(),
                         &mat->shininess, 0.5f, 0.0f, 256.0f, "%.1f");
        ImGui::SameLine();
        ImGui::TextDisabled("(0で鏡面なし)");
      }
    }

    // 環境マップ映り込み（ライティングモードに関係なく表示）
    if (mat) {
      ImGui::Dummy(ImVec2(0, 4));
      ImGui::TextUnformatted("環境マップ");

      bool envOn = (mat->environmentCoefficient > 0.0f);
      if (ImGui::Checkbox((std::string("映り込み有効##") + label).c_str(),
                          &envOn)) {
        if (envOn) {
          // ON: 前回値がなければデフォルト 0.5
          float val = (lastEnvCoeff_ > 0.0f) ? lastEnvCoeff_ : 0.5f;
          SetEnvironmentCoefficient(val);
        } else {
          // OFF: 現在の値を記憶してから 0 にする
          lastEnvCoeff_ = mat->environmentCoefficient;
          SetEnvironmentCoefficient(0.0f);
        }
      }

      if (envOn) {
        if (ImGui::SliderFloat(
                (std::string("映り込み係数##") + label).c_str(),
                &mat->environmentCoefficient, 0.01f, 1.0f, "%.2f")) {
          // スライダー変更時も initialEnvCoeff_ を同期
          initialEnvCoeff_ = mat->environmentCoefficient;
        }
      }
    }

    if (mesh_) {
      ImGui::TextUnformatted("テクスチャ情報");

      const auto &mats = mesh_->Materials();

      int count = 0;
      if (!mats.empty()) {
        count = (int)mats.size();
      } else if (!mesh_->MaterialFile().textureFilePath.empty()) {
        count = 1;
      }

      const ImGuiTableFlags flags =
          ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp |
          ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg;

      if (ImGui::BeginTable("##TexInfo", 2, flags)) {
        ImGui::TableSetupColumn("項目", ImGuiTableColumnFlags_WidthFixed,
                                140.0f);
        ImGui::TableSetupColumn("値", ImGuiTableColumnFlags_WidthStretch);

        // マテリアル数
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("マテリアル数");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%d", count);

        // 各マテリアルが「今参照してる」テクスチャパス
        const int kMaxShow = 32;
        for (int i = 0; i < count && i < kMaxShow; ++i) {
          std::string path;
          if (!mats.empty()) {
            path = mats[i].textureFilePath;
          } else {
            path = mesh_->MaterialFile().textureFilePath;
          }

          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          char labelBuf[64];
          snprintf(labelBuf, sizeof(labelBuf), "[%d] 参照テクスチャ", i);
          ImGui::TextDisabled("%s", labelBuf);

          ImGui::TableSetColumnIndex(1);
          ShowLongTextJP(path);
        }

        if (count > kMaxShow) {
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::TextDisabled("...");
          ImGui::TableSetColumnIndex(1);
          ImGui::TextDisabled("残り %d 件", count - kMaxShow);
        }

        ImGui::EndTable();
      }

      ImGui::Dummy(ImVec2(0, 6));
    }

    // ----------------------------------------------------------
    // モデル参照情報（日本語表示）
    // ----------------------------------------------------------
    if (mesh_) {
      ImGui::TextUnformatted("モデル参照");

      const auto &file = mesh_->SourceFilePath();
      const auto &input = mesh_->SourceInputPath();

      std::string rootName;
      if (!file.empty()) {
        rootName = fs::path(file).filename().string();
      } else if (!input.empty()) {
        rootName = fs::path(input).filename().string();
      } else {
        rootName = "(不明)";
      }

      const ImGuiTableFlags flags =
          ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp |
          ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg;

      if (ImGui::BeginTable("##ModelRef", 2, flags)) {
        ImGui::TableSetupColumn("項目", ImGuiTableColumnFlags_WidthFixed,
                                140.0f);
        ImGui::TableSetupColumn("値", ImGuiTableColumnFlags_WidthStretch);

        auto RowText = [&](const char *k, const std::string &v) {
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::TextDisabled("%s", k);
          ImGui::TableSetColumnIndex(1);
          ShowLongTextJP(v);
        };

        RowText("読み込んだファイル", file);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("共有参照数");

        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%d", (int)mesh_.use_count());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("頂点数");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%u", mesh_->VertexCount());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("描画パーツ数");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%u", (uint32_t)mesh_->DrawItems().size());

        RowText("モデル名", rootName);

        ImGui::EndTable();
      }

      ImGui::Dummy(ImVec2(0, 6));
    }
  }

  ImGui::Dummy(ImVec2(0, 6));

#endif
}

// ============================================================================
// アニメーション・スケルトン関連
// ============================================================================

// ------------------------------------------------------------------
// CreateJoint: NodeからJointを再帰的に作成する
// ------------------------------------------------------------------
// 深さ優先探索で、必ず親のIndexが自身より若くなるようにJointsに登録する。
// これにより、配列先頭から順にUpdateすれば階層計算が正しく行える。
static int32_t CreateJoint(const Node& node,
                           const std::optional<int32_t>& parent,
                           std::vector<Joint>& joints) {
    Joint joint;
    joint.name = node.name;
    joint.localMatrix = node.localMatrix;
    joint.skeletonSpaceMatrix = MakeIdentity4x4();
    joint.transform = node.transform;
    joint.index = int32_t(joints.size()); // 現在登録されている数をIndexに
    joint.parent = parent;

    // まず自分をJoint列に追加（子より先に追加することでIndex順を保証）
    joints.push_back(joint);

    // 子Nodeに対して再帰
    for (const Node& child : node.children) {
        // 子Jointを作成し、そのIndexを取得
        int32_t childIndex = CreateJoint(child, joint.index, joints);
        // 自分（既にpush済み）のchildren配列にchildIndexを追加
        joints[joint.index].children.push_back(childIndex);
    }

    // 自身のIndexを返す
    return joint.index;
}

// ------------------------------------------------------------------
// CreateSkeleton: Node階層からSkeletonを構築する
// ------------------------------------------------------------------
static Skeleton CreateSkeleton(const Node& rootNode) {
    Skeleton skeleton;

    // RootNodeからJoint配列を構築（深さ優先探索）
    skeleton.root = CreateJoint(rootNode, {}, skeleton.joints);

    // 名前とIndexのマッピングを行い、アクセスしやすくする
    for (const Joint& joint : skeleton.joints) {
        skeleton.jointMap.emplace(joint.name, joint.index);
    }

    return skeleton;
}

// ------------------------------------------------------------------
// UpdateSkeleton: 全JointのlocalMatrixとskeletonSpaceMatrixを更新
// ------------------------------------------------------------------
// jointsは親が必ず先（若いIndex）に入っているため、先頭から順に処理すれば
// 親のskeletonSpaceMatrixは必ず計算済み。
static void UpdateSkeleton(Skeleton& skeleton) {
    for (Joint& joint : skeleton.joints) {
        // TransformからlocalMatrixを再構築
        joint.localMatrix = MakeAffineMatrix(
            joint.transform.scale,
            joint.transform.rotate,
            joint.transform.translate);

        // skeletonSpaceMatrixの計算
        if (joint.parent) {
            // 親がいれば親の行列を掛ける
            joint.skeletonSpaceMatrix = Multiply(
                joint.localMatrix,
                skeleton.joints[*joint.parent].skeletonSpaceMatrix);
        } else {
            // 親がいないのでlocalMatrixとskeletonSpaceMatrixは一致する
            joint.skeletonSpaceMatrix = joint.localMatrix;
        }
    }
}

// ------------------------------------------------------------------
// ApplyAnimation: SkeletonにAnimationを適用する
// ------------------------------------------------------------------
// 全Jointをループし、対象のJointにNodeAnimationがあれば
// キーフレーム補間して transform に書き込む。
static void ApplyAnimation(Skeleton& skeleton,
                           const RC::Animation& animation,
                           float animationTime) {
    for (Joint& joint : skeleton.joints) {
        // 対象のJointのAnimationがあれば、値の適用を行う
        // C++17の初期化付きif文
        if (auto it = animation.nodeAnimations.find(joint.name);
            it != animation.nodeAnimations.end()) {
            const RC::NodeAnimation& nodeAnim = it->second;

            joint.transform.translate =
                RC::CalculateValue(nodeAnim.translate, animationTime);
            joint.transform.rotate =
                RC::CalculateValue(nodeAnim.rotate, animationTime);
            joint.transform.scale =
                RC::CalculateValue(nodeAnim.scale, animationTime);
        }
    }
}

// ------------------------------------------------------------------
// AttachAnimation
// ------------------------------------------------------------------

void ModelObject::AttachAnimation() {
    // filePath_がディレクトリの場合、ModelMeshが解決した実ファイルパスを使う
    std::string animPath = filePath_;
    if (resource_.GetMesh() && !resource_.GetMesh()->SourceFilePath().empty()) {
        animPath = resource_.GetMesh()->SourceFilePath();
    }
    AttachAnimation(animPath);
}

void ModelObject::AttachAnimation(const std::string& filePath) {
    if (filePath.empty()) return;
    animationRequested_ = true;
    animation_ = RC::LoadAnimationFile(filePath);
    isAnimated_ = (animation_.duration > 0.0f && !animation_.nodeAnimations.empty());
    animationTime_ = 0.0f;

    // メッシュが既にロード済みならSkeletonを即座に構築
    if (resource_.GetMesh() && resource_.GetMesh()->Ready()) {
        skeleton_ = CreateSkeleton(resource_.GetMesh()->RootNode());
        UpdateSkeleton(skeleton_);
        hasSkeleton_ = true;
    }
    // メッシュ未ロードの場合、UpdateAnimation内で遅延構築する
}

// ------------------------------------------------------------------
// UpdateAnimation
// ------------------------------------------------------------------

void ModelObject::UpdateAnimation(float dt) {
    if (!animationRequested_) return;

    // メッシュがまだロードされていない場合はスキップ
    if (!resource_.GetMesh() || !resource_.GetMesh()->Ready()) return;

    // アニメーションが未ロードの場合、メッシュの実ファイルパスでリトライ
    if (!isAnimated_) {
        const std::string& meshPath = resource_.GetMesh()->SourceFilePath();
        if (!meshPath.empty()) {
            animation_ = RC::LoadAnimationFile(meshPath);
            isAnimated_ = (animation_.duration > 0.0f && !animation_.nodeAnimations.empty());
        }
        if (!isAnimated_) {
            animationRequested_ = false; // リトライ失敗、以降は試みない
            return;
        }
    }

    // 遅延Skeleton構築（メッシュの非同期ロード完了を待つ）
    if (!hasSkeleton_) {
        skeleton_ = CreateSkeleton(resource_.GetMesh()->RootNode());
        UpdateSkeleton(skeleton_);
        hasSkeleton_ = true;
    }

    // アニメーション時間を進める
    animationTime_ += dt;
    if (animation_.duration > 1e-6f) {
        animationTime_ = std::fmod(animationTime_, animation_.duration);
    } else {
        animationTime_ = 0.0f;
    }

    // Skeleton パス: 全Jointの階層行列を更新
    if (hasSkeleton_) {
        ApplyAnimation(skeleton_, animation_, animationTime_);
        UpdateSkeleton(skeleton_);

        // スキニング行列パレット計算: T_i = InverseBindPose_i * SkeletonSpaceMatrix_i
        const auto mesh = resource_.GetMesh();
        if (mesh && mesh->HasSkinData()) {
            const auto &skinData = mesh->GetSkinData();
            const auto &ibpMatrices = skinData.inverseBindPoseMatrices;
            skinMatrices_.resize(ibpMatrices.size());

            for (const auto &[jointName, boneIdx] : skinData.jointNameToIndex) {
                // jointMapを使って高速検索（O(1)）
                auto jit = skeleton_.jointMap.find(jointName);
                if (jit != skeleton_.jointMap.end()) {
                    // T_i = IBP_i * SSM_i
                    skinMatrices_[boneIdx] = Multiply(
                        ibpMatrices[boneIdx],
                        skeleton_.joints[jit->second].skeletonSpaceMatrix);
                } else {
                    // ボーン名がSkeletonに無い場合は単位行列
                    skinMatrices_[boneIdx] = MakeIdentity4x4();
                }
            }
        }
    }

    // レガシーパス: アニメーションチャンネルが1つだけの場合
    // （AnimatedCube等の単純アニメーション）は transform_ に適用する。
    // 複数チャンネルの場合は骨格アニメーションなのでtransform_は変更しない。
    if (animation_.nodeAnimations.size() == 1) {
        auto it = animation_.nodeAnimations.begin();
        RC::NodeAnimation& rootNodeAnim = it->second;
        transform_.translation = RC::CalculateValue(rootNodeAnim.translate, animationTime_);
        transform_.rotation = QuaternionToEuler(RC::CalculateValue(rootNodeAnim.rotate, animationTime_));
        transform_.scale = RC::CalculateValue(rootNodeAnim.scale, animationTime_);
    }
}

// ------------------------------------------------------------------
// DrawSkeleton: デバッグ描画
// ------------------------------------------------------------------
// 各Jointの skeletonSpaceMatrix × worldMatrix でワールド座標を算出し、
// Jointを球で、親子間を線で描画する。

// RenderCommon.h の RC 名前空間関数を使う前方宣言
namespace RC {
void DrawLine3D(const Vector3& a, const Vector3& b, const Vector4& color,
                bool depth);
void DrawSphereRings3D(const Vector3& center, float radius,
                       const Vector4& color, int segments,
                       bool depth);
} // namespace RC

void ModelObject::DrawSkeleton() {
    if (!hasSkeleton_ || skeleton_.joints.empty()) return;

    // モデルのワールド行列を構築
    const RC::Matrix4x4 world = MakeAffineMatrix(
        transform_.scale, transform_.rotation, transform_.translation);

    // 色定義
    const RC::Vector4 jointColor = {1.0f, 1.0f, 0.0f, 1.0f}; // 黄色（Joint球）
    const RC::Vector4 boneColor  = {1.0f, 1.0f, 1.0f, 1.0f}; // 白（Bone線）
    const float jointRadius = 0.02f;

    for (const Joint& joint : skeleton_.joints) {
        // jointWorldMatrix = skeletonSpaceMatrix * worldMatrix
        const RC::Matrix4x4 jointWorld = Multiply(
            joint.skeletonSpaceMatrix, world);

        // 行列の平行移動成分からワールド座標を取得
        const RC::Vector3 jointPos = {
            jointWorld.m[3][0],
            jointWorld.m[3][1],
            jointWorld.m[3][2]
        };

        // Joint位置に球を描画
        RC::DrawSphereRings3D(jointPos, jointRadius, jointColor, 8, false);

        // 親がいれば親Jointとの間にBone線を描画
        if (joint.parent) {
            const Joint& parentJoint = skeleton_.joints[*joint.parent];
            const RC::Matrix4x4 parentWorld = Multiply(
                parentJoint.skeletonSpaceMatrix, world);

            const RC::Vector3 parentPos = {
                parentWorld.m[3][0],
                parentWorld.m[3][1],
                parentWorld.m[3][2]
            };

            RC::DrawLine3D(parentPos, jointPos, boneColor, false);
        }
    }
}

bool ModelObject::HasSkinData() const {
    const auto mesh = resource_.GetMesh();
    return mesh && mesh->HasSkinData() && !skinMatrices_.empty();
}
