#pragma once
#include "Math/Math.h"
#include "Math/MathTypes.h"
#include "ModelMesh.h"
#include "ModelResource.h"
#include "Animation.h"
#include "function/function.h"
#include "struct.h"
#include <array>
#include <d3d12.h>
#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include <wrl/client.h>

class TextureManager; // 前方宣言

namespace RC {
class FrameResource; // 前方宣言
}

// ============================================================
// ModelObject
// - 1つのModelMesh(共有)を配置して描画する
// - ModelMeshが持つ Node/DrawItem を使って「Node行列を反映」して描画できる
// - Textureは「override（SetTexture）」があればそれを優先。
//   overrideが無ければ、materialIndexに応じてTextureManagerでロードして使う。
//
// ★ 内部的に ModelResource（GPUリソース管理）に委譲している。
//    このクラスは Transform / 可視性 / ImGui / ライティング設定 を保持するラッパー。
// ============================================================
/// @class ModelObject
/// @brief 共有モデルリソース（ModelMesh）を配置・管理するためのオブジェクトクラス
/// @details トランスフォーム（座標・回転・スケール）、可視性、ライティング設定、
/// およびテクスチャのオーバーライド機能を持ちます。
/// 描画ロジックの詳細は内部の ModelResource クラスに委譲されています。
class ModelObject {
public:
  /// @struct LightingConfig
  /// @brief オブジェクト固有のライティング設定を保持する構造体
  struct LightingConfig {
    LightingMode mode = HalfLambert;      ///< ライティングモード
    float color[3] = {1.0f, 1.0f, 1.0f}; ///< ライトカラー
    float dir[3] = {0.0f, -1.0f, 0.0f};  ///< ライト方向
    float intensity = 1.0f;              ///< ライト強度

    constexpr LightingConfig() = default;
    constexpr LightingConfig(LightingMode m) : mode(m) {}
    constexpr LightingConfig(int m)
        : mode(static_cast<LightingMode>(m < 0 ? 0 : (m > 3 ? 3 : m))) {}
  };

  ModelObject() = default;
  explicit ModelObject(const LightingConfig &cfg) : initialLighting_(cfg) {}
  ~ModelObject() = default;

  /// @brief オブジェクトを初期化する
  /// @param device D3D12 デバイス
  void Initialize(ID3D12Device *device);

  /// @brief 使用するモデルメッシュを設定する
  /// @param mesh 設定する ModelMesh の shared_ptr
  void SetMesh(const std::shared_ptr<ModelMesh> &mesh) {
    resource_.SetMesh(mesh);
  }

  /// @brief 設定されているモデルメッシュを取得する
  /// @return ModelMesh の shared_ptr
  const std::shared_ptr<ModelMesh> &GetMesh() const {
    return resource_.GetMesh();
  }

  /// @brief モデルにUV座標がない場合、球面投影UVを生成する
  void EnsureSphericalUVIfMissing();

  /// @brief モデル全体のテクスチャを1つのハンドルでオーバーライドする
  /// @param srvGPUHandle 使用するテクスチャの GPU ハンドル
  void SetTexture(D3D12_GPU_DESCRIPTOR_HANDLE srvGPUHandle) {
    resource_.SetTexture(srvGPUHandle);
  }

  /// @brief テクスチャのオーバーライドを解除し、モデル本来のマテリアル設定に戻す
  void ResetTextureToMtl() { resource_.ResetTextureToMtl(); }

  /// @brief ライティング設定を一括で適用する
  /// @param cfg ライティング設定構造体
  /// @return 自身への参照（メソッドチェーン用）
  ModelObject &SetLightingConfig(const LightingConfig &cfg) {
    initialLighting_ = cfg;
    ApplyLightingIfReady_();
    return *this;
  }

  /// @brief ライティング設定を個別のパラメータで指定する
  /// @param mode ライティングモード
  /// @param color ライトカラー
  /// @param dir ライト方向
  /// @param intensity ライト強度
  /// @return 自身への参照
  ModelObject &SetLightingConfig(LightingMode mode,
                                 const std::array<float, 3> &color,
                                 const std::array<float, 3> &dir,
                                 float intensity);

  /// @brief ライティングモードのみを更新する
  /// @param m 設定する LightingMode
  /// @return 自身への参照
  ModelObject &SetLightingMode(LightingMode m) {
    return SetLightingConfig(LightingConfig{m});
  }

  /// @brief トランスフォーム情報を取得する（読み書き可能）
  /// @return Transform への参照
  Transform &T() { return transform_; }

  /// @brief マテリアル情報を取得する（読み書き可能）
  /// @return Material ポインタ
  Material *Mat() { return resource_.Mat(); }

  /// @brief オブジェクトのベースカラーを設定する
  /// @param color 設定するカラーベクトル
  void SetColor(const RC::Vector4 &color);

  /// @brief ライト情報を取得する（読み書き可能）
  /// @return DirectionalLight ポインタ
  DirectionalLight *Light() { return resource_.Light(); }

  /// @brief 環境マップ映り込み係数を設定する（非同期ロード対応）
  /// @param coeff 係数 (0.0: なし, 1.0: 最大)
  void SetEnvironmentCoefficient(float coeff) {
    initialEnvCoeff_ = coeff;
    if (Material *mat = resource_.Mat()) {
      mat->environmentCoefficient = coeff;
    }
  }

  /// @brief 環境マップ映り込み係数を取得する
  /// @return 係数値
  float GetEnvironmentCoefficient() const { return initialEnvCoeff_; }

  /// @brief 外部のライトCBアドレスを指定してライト設定を共有する
  /// @param addr ライトCBの GPU 仮想アドレス（0 で自前ライトに戻る）
  void SetExternalLightCBAddress(D3D12_GPU_VIRTUAL_ADDRESS addr) {
    resource_.SetExternalLightCBAddress(addr);
  }

  /// @brief 外部ライトCBの GPU 仮想アドレスを取得する
  /// @return アドレス値
  D3D12_GPU_VIRTUAL_ADDRESS GetExternalLightCBAddress() const {
    return resource_.GetExternalLightCBAddress();
  }

  /// @brief オブジェクトの可視状態を設定する
  /// @param v true で表示
  void SetVisible(bool v) { visible_ = v; }

  /// @brief オブジェクトが可視状態か取得する
  /// @return 可視なら true
  bool Visible() const { return visible_; }

  /// @brief テクスチャマネージャを設定する（モデル読み込み時のテクスチャ解決に使用）
  /// @param tm テクスチャマネージャへのポインタ
  void SetTextureManager(TextureManager *tm) { resource_.SetTextureManager(tm); }

  /// @brief モデルのファイルパスを保存する
  /// @param path ファイルパス
  void SetFilePath(const std::string &path) { filePath_ = path; }

  /// @brief モデルのファイルパスを取得する
  /// @return ファイルパス文字列
  const std::string &GetFilePath() const { return filePath_; }

  /// @brief 準備完了フラグを設定する
  /// @param r 準備完了状態
  void SetReady(bool r) { resource_.SetReady(r); }

  /// @brief モデルデータの準備が完了しているか確認する
  /// @return 準備完了なら true
  bool IsReady() const { return resource_.IsReady(); }

  /// @brief 毎フレームの更新処理（行列計算等）
  /// @param view ビュー行列
  /// @param proj プロジェクション行列
  void Update(const RC::Matrix4x4 &view, const RC::Matrix4x4 &proj);

  /// @brief ワールド行列を指定してオブジェクトを描画する
  /// @param cmdList グラフィックスコマンドリスト
  /// @param world 描画に使用するワールド行列
  /// @param frame フレームリソース（一時メモリ用）
  void Draw(ID3D12GraphicsCommandList *cmdList, const RC::Matrix4x4 &world,
            RC::FrameResource &frame);

  /// @brief 複数のインスタンス（Transformリスト）を一括描画する
  /// @param cmdList グラフィックスコマンドリスト
  /// @param view ビュー行列
  /// @param proj プロジェクション行列
  /// @param instances インスタンスごとの Transform 配列
  /// @param frame フレームリソース
  void DrawBatch(ID3D12GraphicsCommandList *cmdList, const RC::Matrix4x4 &view,
                 const RC::Matrix4x4 &proj,
                 const std::vector<Transform> &instances,
                 RC::FrameResource &frame);

  /// @brief インスタンス一括描画（単色オーバーライド付き）
  /// @param cmdList グラフィックスコマンドリスト
  /// @param view ビュー行列
  /// @param proj プロジェクション行列
  /// @param instances インスタンスごとの Transform 配列
  /// @param color 全インスタンスに適用するオーバーライドカラー
  /// @param frame フレームリソース
  void DrawBatch(ID3D12GraphicsCommandList *cmdList, const RC::Matrix4x4 &view,
                 const RC::Matrix4x4 &proj,
                 const std::vector<Transform> &instances,
                 const RC::Vector4 &color,
                 RC::FrameResource &frame);

  /// @brief ImGui を使用したデバッグ用 UI を表示する
  /// @param name 表示名
  /// @param showLightingUi ライティング設定を表示するか
  void DrawImGui(const char *name, bool showLightingUi);

  /// @brief バッチ描画用のカーソルをリセットする
  void ResetBatchCursor() { resource_.ResetBatchCursor(); }

  /// @brief 内部の ModelResource インスタンスを取得する
  /// @return ModelResource への参照
  ModelResource &Resource() { return resource_; }
  const ModelResource &Resource() const { return resource_; }

private:
  /// @brief ライティング設定を GPU リソースに反映する（準備完了時のみ）
  void ApplyLightingIfReady_();

private:
  ModelResource resource_; ///< 描画リソース・ロジック本体

  Transform transform_{{1, 1, 1}, {0, 0, 0}, {0, 0, 0}}; ///< オブジェクトの座標・回転・スケール
  bool visible_ = true; ///< 可視フラグ

  RC::Matrix4x4 cachedView_ = {}; ///< Update で受け取ったビュー行列のキャッシュ
  RC::Matrix4x4 cachedProj_ = {}; ///< Update で受け取ったプロジェクション行列のキャッシュ
  bool hasVP_ = false;           ///< ビュー・プロジェクション行列がセットされているか

  LightingConfig initialLighting_{}; ///< 初期化時または外部から設定されたライティング情報

  float initialEnvCoeff_ = 0.0f; ///< 環境マップ映り込み係数の初期設定値
  float lastEnvCoeff_ = 0.5f;    ///< ImGui用：環境マップトグル時の係数保存用

  std::string filePath_; ///< モデルのアセットパス

  // === アニメーション関連 ===
public:
  /// @brief 自身のファイルパスを使ってアニメーションをロード・アタッチする
  void AttachAnimation();
  
  /// @brief 指定したファイルからアニメーションをロード・アタッチする
  /// @param filePath アニメーションファイル(.gltf等)のパス
  void AttachAnimation(const std::string& filePath);

  /// @brief アニメーションを更新し、Skeletonに適用する
  /// @param dt 経過時間
  void UpdateAnimation(float dt);

  /// @brief スケルトンのデバッグ描画を行う
  /// @details 各Jointを球で、親子関係のあるJoint同士を線で描画する
  void DrawSkeleton();

  /// @brief Skeleton が有効かどうか
  bool HasSkeleton() const { return hasSkeleton_; }

  /// @brief スキンデータが有効かどうか（ボーンウェイト付きモデルか）
  bool HasSkinData() const;

  /// @brief スキニング行列パレットを取得する
  const std::vector<RC::Matrix4x4> &GetSkinMatrices() const { return skinMatrices_; }

private:
  RC::Animation animation_;   ///< ロードしたアニメーションデータ
  float animationTime_ = 0.0f;///< アニメーション再生時間
  bool isAnimated_ = false;   ///< アニメーションが有効かどうか
  bool animationRequested_ = false; ///< AttachAnimationが呼ばれたか（遅延ロード用）

  Skeleton skeleton_;          ///< スケルトンデータ
  bool hasSkeleton_ = false;   ///< スケルトンが構築済みか

  // === スキニング関連 ===
  std::vector<RC::Matrix4x4> skinMatrices_; ///< スキンクラスター行列パレット (T_i = IBP_i * SSM_i)
};
