#pragma once
#include "EngineConfig.h"
#include "Math/MathTypes.h"
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

/// @brief オブジェクトの基本変換情報（スケール、回転、並進）
struct Transform {
  RC::Vector3 scale;       ///< スケール
  RC::Vector3 rotation;    ///< 回転角 (ラジアン)
  RC::Vector3 translation; ///< 座標 (並進)

  /// @brief 移動ベクトルを座標に加算する
  /// @param velocity 加算する移動量
  /// @return 自身の参照
  Transform &operator+=(const RC::Vector3 &velocity) {
    this->translation.x += velocity.x;
    this->translation.y += velocity.y;
    this->translation.z += velocity.z;
    return *this;
  }
};

/// @brief GPU転送用のカメラデータ構造体
struct CameraForGPU {
  RC::Vector3 worldPosition; ///< カメラのワールド座標位置
};

/// @brief 軸に平行な境界ボックス (Axis-Aligned Bounding Box)
struct AABB {
  RC::Vector3 min = {-1.0f, -1.0f, -1.0f}; ///< 最小座標
  RC::Vector3 max = {1.0f, 1.0f, 1.0f};    ///< 最大座標
};

/// @brief 矩形領域（2D座標等に使用）
struct Rect {
  float left;   ///< 左端
  float right;  ///< 右端
  float bottom; ///< 下端
  float top;    ///< 上端
};

/// @brief パーティクル放出器の設定
struct Emitter {
  Transform transform; ///< 放出器の位置・姿勢
  uint32_t count;      ///< 一度に放出するパーティクル数
  float frequency;     ///< 放出間隔（秒）
  float frequencyTime; ///< 放出間隔タイマー用経過時間

  // 一括制御パラメータ
  RC::Vector3 globalScale = {1.0f, 1.0f, 1.0f};    ///< パーティクル全体のスケール倍率
  RC::Vector4 globalColor = {1.0f, 1.0f, 1.0f, 1.0f}; ///< パーティクル全体の色
  float timeScale = 1.0f;                          ///< パーティクル全体の時間軸倍率
};

/// @brief 加速度フィールド（空間内の特定領域に加速度を与える）
struct AccelerationField {
  RC::Vector3 acceleration; ///< 加速度ベクトル
  AABB area;                ///< 加速度が適用される範囲
};

/// @brief パーティクルの個別データ
struct ParticleData {
  Transform transform; ///< パーティクルの位置・回転・スケール
  RC::Vector3 velocity; ///< 速度ベクトル
  RC::Vector4 color;    ///< パーティクルの色 (RGBA)
  float lifeTime;       ///< 生存時間 (秒)
  float currentTime;    ///< 現在の経過時間 (秒)
};

/// @brief 線分データ（デバッグ描画等に使用）
struct Segment {
  RC::Vector3 origin = {0.0f, 0.0f, 0.0f}; ///< 始点
  RC::Vector3 diff = {1.0f, 1.0f, 1.0f};   ///< 終点への差分ベクトル
  uint32_t color = 0x000000FF;            ///< 色 (RGBA8)
};

/// @brief 頂点データ構造体
struct VertexData {
  RC::Vector4 position; ///< 頂点座標 (x, y, z, w)
  RC::Vector2 texcoord; ///< テクスチャ座標 (u, v)
  RC::Vector3 normal;   ///< 法線ベクトル
  // === Skinning ===
  int32_t boneIndices[4] = {0, 0, 0, 0}; ///< 影響するボーンのインデックス (最大4)
  float   boneWeights[4] = {0, 0, 0, 0}; ///< 各ボーンのウェイト (合計1.0)
};

/// @brief マテリアルの基本データ（ファイルパス等）
struct MaterialData {
  std::string textureFilePath; ///< テクスチャファイルのパス
};

/// @brief クォータニオンベースの変換情報（SRT）
/// @details Euler回転ではなくQuaternionで回転を保持する。
/// アニメーション補間やスケルトンのジョイント管理に使用する。
struct QuaternionTransform {
  RC::Vector3 scale;       ///< スケール
  RC::Quaternion rotate;   ///< 回転 (Quaternion)
  RC::Vector3 translate;   ///< 並進
};

/// @brief モデル内のノード構造体（階層構造管理用）
struct Node {
  QuaternionTransform transform;     ///< SRT変換情報 (Quaternion回転)
  RC::Matrix4x4 localMatrix;         ///< 親ノードからの相対変換行列
  std::string name;                  ///< ノード名
  std::vector<uint32_t> meshIndices; ///< このノードが参照するメッシュのインデックス
  std::vector<Node> children;        ///< 子ノードの配列
};

/// @brief スケルトンの1つのジョイント（関節）
/// @details ノード階層からフラット配列に変換されたジョイントデータ。
/// parentのIndexは必ず自身より若い値になるため、配列先頭から順に処理できる。
struct Joint {
  QuaternionTransform transform;          ///< Transform情報
  RC::Matrix4x4 localMatrix;              ///< localMatrix
  RC::Matrix4x4 skeletonSpaceMatrix;      ///< skeletonSpaceでの変換行列
  std::string name;                       ///< 名前
  std::vector<int32_t> children;          ///< 子JointのIndexのリスト。いなければ空
  int32_t index;                          ///< 自身のIndex
  std::optional<int32_t> parent;          ///< 親JointのIndex。いなければnull
};

/// @brief スケルトン（ジョイント階層を管理する構造体）
/// @details Nodeの階層構造をフラットなJoint配列に変換し、
/// 名前からIndexへの高速検索を提供する。
struct Skeleton {
  int32_t root;                                 ///< RootJointのIndex
  std::map<std::string, int32_t> jointMap;      ///< Joint名とIndexとの辞書
  std::vector<Joint> joints;                    ///< 所属しているジョイント
};

/// @brief モデル全体のデータ構造体
struct ModelData {
  std::vector<VertexData> vertices; ///< 頂点データの配列
  std::vector<uint32_t> indices;    ///< インデックスデータの配列
  MaterialData material;            ///< マテリアル情報
  Node rootNode;                    ///< ルートノード
};

/// @brief 3Dオブジェクト用マテリアル設定（GPU転送用）
struct Material {
  RC::Vector4 color;                    ///< 基本色 (RGBA)
  int lightingMode;                     ///< ライティングモード (0:なし, 1:Lambert, 2:Half Lambert)
  float shininess;                      ///< 光沢度
  float environmentCoefficient = 0.0f;  ///< 環境マップ映り込み係数 (0.0 ~ 1.0)
  int useNormalMap = 0;                 ///< 1なら法線マップを使用
  int useRoughnessMap = 0;              ///< 1ならラフネスマップを使用
  float padding[3];                     ///< パディング（16byteアラインメント用）
  RC::Matrix4x4 uvTransform;            ///< UV座標の変換行列
};

/// @brief スプライト用マテリアル設定
struct SpriteMaterial {
  RC::Vector4 color;         ///< 色 (RGBA)
  RC::Matrix4x4 uvTransform; ///< UV変換行列
};

/// @brief GPU転送用の座標変換行列データ（b0番）
struct TransformationMatrix {
  RC::Matrix4x4 WVP;                    ///< World-View-Projection 行列
  RC::Matrix4x4 World;                  ///< World 行列
  RC::Matrix4x4 worldInverseTranspose;  ///< World行列の逆転置（法線変換用）
};

/// @brief GPU転送用のパーティクルインスタンシングデータ
struct ParticleForGPU {
  RC::Matrix4x4 WVP;   ///< World-View-Projection 行列
  RC::Matrix4x4 World; ///< World 行列
  RC::Vector4 color;   ///< 色 (RGBA)
};

/// @brief GPU Particle 用の粒子データ（HLSL の Particle 構造体と共通レイアウト）
/// @details Compute Shader で初期化・更新し、Vertex Shader で参照するデータ。
/// DEFAULT ヒープに配置される。
struct ParticleCS {
  RC::Vector3 translate;   ///< 位置
  float pad0;              ///< パディング（16byteアラインメント）
  RC::Vector3 scale;       ///< スケール
  float lifeTime;          ///< 生存時間 (秒)
  RC::Vector3 velocity;    ///< 速度ベクトル
  float currentTime;       ///< 経過時間 (秒)
  RC::Vector4 color;       ///< 色 (RGBA)
};

/// @brief GPU Particle 描画用のビュー定数バッファ（VS b0）
struct GPUParticlePerView {
  RC::Matrix4x4 viewProjection;  ///< View-Projection 行列
  RC::Matrix4x4 billboardMatrix; ///< ビルボード回転行列
};

/// @brief エミッタ形状の種類
enum class EmitterShape : uint32_t {
  Point = 0,   ///< 一点から放射
  Sphere = 1,  ///< 球状に発生
  Box = 2,     ///< 矩形領域から発生
  Cone = 3,    ///< 円錐状に噴射
  Count
};

/// @brief GPU Particle 更新 CS 用のフレーム定数バッファ（CS b0）
/// @details Particle Editor から設定されるエミッタパラメータも含む。
/// HLSL の PerFrame 定数バッファと完全に一致するレイアウトにすること。
struct GPUParticlePerFrame {
  // --- 基本パラメータ (16 bytes) ---
  float deltaTime;       ///< フレーム間隔 (秒)
  uint32_t maxParticles; ///< 最大パーティクル数
  float minLifeTime;     ///< 最小寿命 (秒)
  float maxLifeTime;     ///< 最大寿命 (秒)

  // --- スケール (16 bytes) ---
  float minScale;        ///< 最小スケール
  float maxScale;        ///< 最大スケール
  float gravity;         ///< 重力の強さ (下方向が正)
  uint32_t emitterShape; ///< エミッタ形状 (EmitterShape)

  // --- 速度 (16 bytes) ---
  RC::Vector3 baseVelocity;   ///< 基本速度ベクトル
  float velocityVariance;     ///< 速度のランダム分散

  // --- エミッタ形状パラメータ (16 bytes) ---
  float shapeRadius;     ///< Sphere/Cone の半径
  float coneAngle;       ///< Cone の半角 (ラジアン)
  RC::Vector2 shapePad;  ///< パディング

  // --- 開始色 (16 bytes) ---
  RC::Vector4 startColor;    ///< パーティクル開始色 (RGBA)

  // --- 終了色 (16 bytes) ---
  RC::Vector4 endColor;      ///< パーティクル終了色 (RGBA)

  // --- エミッタ位置 (16 bytes) ---
  RC::Vector3 emitterPosition; ///< エミッタのワールド座標
  uint32_t emitCount;          ///< 1フレームの射出数

  // --- Box形状サイズ (16 bytes) ---
  RC::Vector3 shapeBoxSize;  ///< Box 形状のサイズ (xyz)
  float shapeBoxPad;         ///< パディング
};

/// @brief 平行光源データ
struct DirectionalLight {
  RC::Vector4 color;     ///< 光の色 (RGBA)
  RC::Vector3 direction; ///< 光の方向
  float intensity;       ///< 光の強度
};

/// @brief 点光源データ
struct PointLight {
  RC::Vector4 color;    ///< 光の色 (RGBA)
  RC::Vector3 position; ///< 光の座標
  float intensity;      ///< 光の強度
  float radius;         ///< 影響半径
  float decay;          ///< 減衰率
  float padding[2];     ///< パディング
};

/// @brief スポットライトデータ
struct SpotLight {
  RC::Vector4 color;     ///< 光の色 (RGBA)
  RC::Vector3 position;  ///< 光の座標
  float intensity;       ///< 光の強度
  RC::Vector3 direction; ///< 光の方向
  float distance;        ///< 影響距離
  float decay;           ///< 減衰率
  float cosAngle;        ///< スポットライトの照射角のコサイン
  float padding[2];      ///< パディング
};

/// @brief 面光源 (Area Light) データ
struct AreaLight {
  RC::Vector4 color;    ///< 光の色 (RGBA)
  RC::Vector3 position; ///< 中心座標
  float intensity;      ///< 光の強度

  RC::Vector3 right; ///< 面の右方向ベクトル
  float halfWidth;   ///< 幅の半分

  RC::Vector3 up;   ///< 面の上方向ベクトル
  float halfHeight; ///< 高さの半分

  float range;       ///< 影響距離
  float decay;       ///< 減衰指数
  uint32_t twoSided; ///< 1なら両面発光、0なら片面
  uint32_t padding;  ///< パディング
};

/// @brief 点光源群の定数バッファ構造体 (b3番)
struct PointLightsCB {
  uint32_t count = 0;                     ///< 有効なライト数 (0 ~ 4)
  float padding0[3] = {0.0f, 0.0f, 0.0f}; ///< 16byte境界合わせ用
  PointLight lights[4]{};                ///< 点光源の配列
};

/// @brief スポットライト群の定数バッファ構造体 (b4番)
struct SpotLightsCB {
  uint32_t count = 0;                     ///< 有効なライト数 (0 ~ 4)
  float padding0[3] = {0.0f, 0.0f, 0.0f}; ///< 16byte境界合わせ用
  SpotLight lights[4]{};                 ///< スポットライトの配列
};

/// @brief 面光源群の定数バッファ構造体
struct AreaLightsCB {
  uint32_t count = 0;            ///< 有効なライト数 (0 ~ 4)
  float padding0[3] = {0, 0, 0}; ///< 16byte境界合わせ用
  AreaLight lights[4]{};        ///< 面光源の配列
};

/// @brief ライティングの計算方式
enum LightingMode {
  None = 0,        ///< ライティングなし
  Lambert = 1,     ///< ランバート反射
  HalfLambert = 2, ///< ハーフランバート反射
};

/// @brief ビューポートのデバッグ表示モード
enum class ViewShadingMode {
  Solid = 0,            ///< 通常のソリッド表示
  Wireframe = 1,        ///< ワイヤーフレーム表示
  SolidWireframe = 2,   ///< ソリッド表示にワイヤーを重ねる
  FaceOrientation = 3,  ///< 法線デバッグ（表=青, 裏=赤）
  RandomColor = 4,      ///< オブジェクト毎のランダムカラー
  SolidShading = 5,     ///< テクスチャなしの形状確認用表示
};

/// @brief 2D図形の描画塗りつぶしモード
enum kFillMode {
  kFill = 0, ///< 塗りつぶしあり
  kWire = 1, ///< 枠線のみ（塗りつぶしなし）
};
