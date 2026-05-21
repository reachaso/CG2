#pragma once
#include "GraphicsPipeline/GraphicsPipeline.h"
#include "Math/Math.h"
#include "Scene.h"
#include <d3d12.h>
#include <string>
#include <future>
#include "struct.h"

// D3D12 GPUハンドルを返すために必要
struct D3D12_GPU_DESCRIPTOR_HANDLE;

// PostEffectType 前方宣言（PostProcess.h で定義）
enum class PostEffectType;

namespace RC {

// ============================================================================
// ライト生成時の「アクティブ登録」モード
// ============================================================================
enum class LightActivateMode {
  None = 0, ///< 生成するだけ（描画には使われない）
  Add,      ///< 現在のアクティブ配列に追加（最大数を超えたら追加失敗）
  Set,      ///< アクティブ配列をクリアしてから追加（= それだけを使う）
};

/// @brief RenderCommon を初期化する（起動時に一度だけ呼ぶ）
/// @param ctx シーンコンテキスト
/// @note App::Init 内など、Device / PipelineManager / SRVManager が有効になった後に呼んでください。
void Init(SceneContext &ctx);

/// @brief RenderCommon を終了する（確保したリソースを解放）
void Term();

/// @brief 今フレームのカメラ情報（View/Proj とカメラ位置）を共有する
/// @param view ビュー行列
/// @param proj プロジェクション行列
/// @param camWorldPos カメラのワールド座標
/// @note View/Proj は各オブジェクト Update に渡され、camWorldPos はシェーダー側で利用できます。
void SetCamera(const Matrix4x4 &view, const Matrix4x4 &proj,
               const RC::Vector3 camWorldPos);

// ==============================
// ライト用
// ==============================

class DirectionalLightSource;

/// @brief ディレクショナルライトを生成してハンドルを返す
/// @param activateMode アクティブ化モード
/// @return ライトハンドル（失敗時は -1）
int CreateDirectionalLight(
    LightActivateMode activateMode = LightActivateMode::Set);

/// @brief ディレクショナルライトを破棄する
/// @param lightHandle ライトハンドル
void DestroyDirectionalLight(int lightHandle);

/// @brief 3D描画で使用する「アクティブなディレクショナルライト」を切り替える
/// @param lightHandle ライトハンドル。-1 の場合は「明示的なアクティブ無し」になり、描画時は共通のデフォルトライトを使用します。
void SetActiveDirectionalLight(int lightHandle);

/// @brief 現在のアクティブなディレクショナルライトのハンドルを返す
/// @return ライトハンドル（未設定なら 0）
int GetActiveDirectionalLightHandle();

/// @brief ディレクショナルライトの実体ポインタを取得する
/// @param lightHandle ライトハンドル
/// @return DirectionalLightSource*（無効ハンドルなら nullptr）
DirectionalLightSource *GetDirectionalLightPtr(int lightHandle);

/// @brief ディレクショナルライトの ImGui 表示を行う
/// @param lightHandle ライトハンドル（-1 の場合は実効アクティブを使用）
/// @param name 表示名
void DrawImGuiDirectionalLight(int lightHandle, const char *name);

/// @brief ディレクショナルライトの ON/OFF を切り替える
/// @param lightHandle ライトハンドル
/// @param enabled 有効にするなら true
void SetDirectionalLightEnabled(int lightHandle, bool enabled);

/// @brief ディレクショナルライトが ON かどうかを確認する
/// @param lightHandle ライトハンドル
/// @return 有効なら true
bool IsDirectionalLightEnabled(int lightHandle);

/// @brief 現在の実効アクティブなディレクショナルライトの ON/OFF を切り替える
/// @param enabled 有効にするなら true
void SetActiveDirectionalLightEnabled(bool enabled);

/// @brief 現在の実効アクティブなディレクショナルライトが ON かどうかを確認する
/// @return 有効なら true
bool IsActiveDirectionalLightEnabled();

// ==============================
// ポイントライト用
// ==============================

class PointLightSource;

/// @brief ポイントライトを生成してハンドルを返す
/// @param activateMode アクティブ化モード
/// @return ポイントライトハンドル（失敗時は -1）
int CreatePointLight(LightActivateMode activateMode = LightActivateMode::Add);

/// @brief ポイントライトを破棄する
/// @param pointLightHandle ポイントライトハンドル
void DestroyPointLight(int pointLightHandle);

/// @brief 3D描画で使用する「アクティブポイントライト」を切り替える
/// @param pointLightHandle ポイントライトハンドル。-1 の場合は「明示的なアクティブ無し」になり、描画時はデフォルト（OFF）を使用します。
void SetActivePointLight(int pointLightHandle);

/// @brief 現在のアクティブポイントライトのハンドルを返す
/// @return ポイントライトハンドル（未設定なら -1）
int GetActivePointLightHandle();

/// @brief アクティブポイントライト配列をクリアする
void ClearActivePointLights();

/// @brief アクティブポイントライト配列に追加する
/// @param pointLightHandle 追加するライトハンドル
/// @return 追加に成功したら true
bool AddActivePointLight(int pointLightHandle);

/// @brief アクティブポイントライト配列から削除する
/// @param pointLightHandle 削除するライトハンドル
void RemoveActivePointLight(int pointLightHandle);

/// @brief 現在のアクティブポイントライトの数を取得する
/// @return アクティブなポイントライトの数
int GetActivePointLightCount();

/// @brief アクティブポイントライトのハンドルをインデックスで取得する
/// @param index インデックス
/// @return ポイントライトハンドル
int GetActivePointLightHandleAt(int index);

/// @brief ポイントライトの実体ポインタを取得する
/// @param pointLightHandle ポイントライトハンドル
/// @return PointLightSource*（無効ハンドルなら nullptr）
PointLightSource *GetPointLightPtr(int pointLightHandle);

/// @brief ポイントライトの ImGui 表示を行う
/// @param pointLightHandle ポイントライトハンドル
/// @param name 表示名
void DrawImGuiPointLight(int pointLightHandle, const char *name);

/// @brief ポイントライトの ON/OFF を切り替える
/// @param pointLightHandle ポイントライトハンドル
/// @param enabled 有効にするなら true
void SetPointLightEnabled(int pointLightHandle, bool enabled);

/// @brief ポイントライトが ON かどうかを確認する
/// @param pointLightHandle ポイントライトハンドル
/// @return 有効なら true
bool IsPointLightEnabled(int pointLightHandle);

/// @brief 現在の実効アクティブなポイントライト（先頭）の ON/OFF を切り替える
/// @param enabled 有効にするなら true
void SetActivePointLightEnabled(bool enabled);

/// @brief 現在の実効アクティブなポイントライトが ON かどうかを確認する
/// @return 有効なら true
bool IsActivePointLightEnabled();

// ==============================
// スポットライト用
// ==============================

class SpotLightSource;

/// @brief スポットライトを生成してハンドルを返す
/// @param activateMode アクティブ化モード
/// @return スポットライトハンドル（失敗時は -1）
int CreateSpotLight(LightActivateMode activateMode = LightActivateMode::Add);

/// @brief スポットライトを破棄する
/// @param spotLightHandle スポットライトハンドル
void DestroySpotLight(int spotLightHandle);

/// @brief 3D描画で使用する「アクティブスポットライト」を切り替える
/// @param spotLightHandle スポットライトハンドル。-1 の場合は「明示的なアクティブ無し」になり、描画時はデフォルト（OFF）を使用します。
void SetActiveSpotLight(int spotLightHandle);

/// @brief 現在のアクティブスポットライトのハンドルを返す
/// @return スポットライトハンドル（未設定なら -1）
int GetActiveSpotLightHandle();

/// @brief アクティブスポットライト配列をクリアする
void ClearActiveSpotLights();

/// @brief アクティブスポットライト配列に追加する
/// @param spotLightHandle 追加するライトハンドル
/// @return 追加に成功したら true
bool AddActiveSpotLight(int spotLightHandle);

/// @brief アクティブスポットライト配列から削除する
/// @param spotLightHandle 削除するライトハンドル
void RemoveActiveSpotLight(int spotLightHandle);

/// @brief 現在のアクティブスポットライトの数を取得する
/// @return アクティブなスポットライトの数
int GetActiveSpotLightCount();

/// @brief アクティブスポットライトのハンドルをインデックスで取得する
/// @param index インデックス
/// @return スポットライトハンドル
int GetActiveSpotLightHandleAt(int index);

/// @brief スポットライトの実体ポインタを取得する
/// @param spotLightHandle スポットライトハンドル
/// @return SpotLightSource*（無効ハンドルなら nullptr）
SpotLightSource *GetSpotLightPtr(int spotLightHandle);

/// @brief スポットライトの ImGui 表示を行う
/// @param spotLightHandle スポットライトハンドル
/// @param name 表示名
void DrawImGuiSpotLight(int spotLightHandle, const char *name);

/// @brief スポットライトの ON/OFF を切り替える
/// @param spotLightHandle スポットライトハンドル
/// @param enabled 有効にするなら true
void SetSpotLightEnabled(int spotLightHandle, bool enabled);

/// @brief スポットライトが ON かどうかを確認する
/// @param spotLightHandle スポットライトハンドル
/// @return 有効なら true
bool IsSpotLightEnabled(int spotLightHandle);

/// @brief 現在の実効アクティブなスポットライト（先頭）の ON/OFF を切り替える
/// @param enabled 有効にするなら true
void SetActiveSpotLightEnabled(bool enabled);

/// @brief 現在の実効アクティブなスポットライトが ON かどうかを確認する
/// @return 有効なら true
bool IsActiveSpotLightEnabled();


// ============================================================================
// AreaLight（Rect）
// ============================================================================

class AreaLightSource;

/// @brief エリアライト（矩形）を生成してハンドルを返す
/// @param activateMode アクティブ化モード
/// @return エリアライトハンドル（失敗時は -1）
int CreateAreaLight(LightActivateMode activateMode = LightActivateMode::Add);

/// @brief エリアライト（矩形）を破棄する
/// @param areaLightHandle エリアライトハンドル
void DestroyAreaLight(int areaLightHandle);

/// @brief エリアライトを1個だけ有効化する（既存のアクティブをクリアして追加）
/// @param areaLightHandle エリアライトハンドル
void SetActiveAreaLight(int areaLightHandle);

/// @brief 現在のアクティブエリアライトのハンドルを返す
/// @return エリアライトハンドル
int GetActiveAreaLightHandle();

/// @brief アクティブエリアライト配列をクリアする
void ClearActiveAreaLights();

/// @brief アクティブエリアライト配列に追加する
/// @param areaLightHandle 追加するライトハンドル
/// @return 追加に成功したら true
bool AddActiveAreaLight(int areaLightHandle);

/// @brief アクティブエリアライト配列から削除する
/// @param areaLightHandle 削除するライトハンドル
void RemoveActiveAreaLight(int areaLightHandle);

/// @brief 現在のアクティブエリアライトの数を取得する
/// @return アクティブなエリアライトの数
int GetActiveAreaLightCount();

/// @brief アクティブエリアライトのハンドルをインデックスで取得する
/// @param index インデックス
/// @return エリアライトハンドル
int GetActiveAreaLightHandleAt(int index);

/// @brief エリアライトの実体ポインタを取得する
/// @param areaLightHandle エリアライトハンドル
/// @return AreaLightSource*
AreaLightSource *GetAreaLightPtr(int areaLightHandle);

/// @brief エリアライトの ImGui 表示を行う
/// @param areaLightHandle エリアライトハンドル
/// @param name 表示名
void DrawImGuiAreaLight(int areaLightHandle, const char *name = nullptr);

/// @brief エリアライトの ON/OFF を切り替える
/// @param areaLightHandle エリアライトハンドル
/// @param enabled 有効にするなら true
void SetAreaLightEnabled(int areaLightHandle, bool enabled);

/// @brief エリアライトが ON かどうかを確認する
/// @param areaLightHandle エリアライトハンドル
/// @return 有効なら true
bool IsAreaLightEnabled(int areaLightHandle);

/// @brief 現在の実効アクティブなエリアライトの ON/OFF を切り替える
/// @param enabled 有効にするなら true
void SetActiveAreaLightEnabled(bool enabled);

/// @brief 現在の実効アクティブなエリアライトが ON かどうかを確認する
/// @return 有効なら true
bool IsActiveAreaLightEnabled();

// ── モデル用 ──────────────────────────────────────

/// @brief 3D描画の前処理（このフレームの SceneContext / CommandList を登録）
/// @param ctx SceneContext
/// @param cl このフレームで使うコマンドリスト
/// @note この関数を呼んだ後に DrawModel / DrawSphere などを呼んでください。
void PreDraw3D(SceneContext &ctx, ID3D12GraphicsCommandList *cl);

/// @brief モデルをロードしてハンドルを返す（Mesh は内部キャッシュで共有）
/// @param path .obj へのパス
/// @return モデルハンドル（失敗時は -1）
int LoadModel(const std::string &path);

/// @brief モデルを描画する（テクスチャ指定版）
/// @param modelHandle モデルハンドル
/// @param texHandle テクスチャハンドル（RC::LoadTex で取得）
void DrawModel(int modelHandle, int texHandle);

/// @brief モデルを描画する（モデルが参照する mtl のテクスチャを使用）
/// @param modelHandle モデルハンドル
void DrawModel(int modelHandle);

/// @brief モデルを描画する（カリング無効版）
/// @param modelHandle モデルハンドル
/// @param texHandle テクスチャハンドル（-1 なら mtl のテクスチャ）
void DrawModelNoCull(int modelHandle, int texHandle = -1);

/// @brief モデルをインスタンシング（複数Transform）で描画する
/// @param modelHandle モデルハンドル
/// @param instances インスタンスTransform配列
/// @param texHandle テクスチャハンドル（-1 なら mtl のテクスチャ）
void DrawModelBatch(int modelHandle, const std::vector<Transform> &instances,
                    int texHandle = -1);

/// @brief モデルをインスタンシング（複数Transform + 単一色）で描画する
/// @param modelHandle モデルハンドル
/// @param instances インスタンスTransform配列
/// @param color 全インスタンスに適用する乗算色
/// @param texHandle テクスチャハンドル（-1 なら mtl のテクスチャ）
void DrawModelBatchColored(int modelHandle,
                           const std::vector<Transform> &instances,
                           const Vector4 &color,
                           int texHandle = -1);

/// @brief モデルの ImGui 表示を行う
/// @param modelHandle モデルハンドル
/// @param name 表示名
void DrawImGui3D(int modelHandle, const char *name);

/// @brief モデルを解放する（ハンドルは無効化される）
/// @param modelHandle モデルハンドル
void UnloadModel(int modelHandle);

/// @brief モデルの Transform ポインタを取得する
/// @param modelHandle モデルハンドル
/// @return Transform*（無効ハンドルなら nullptr）
Transform *GetModelTransformPtr(int modelHandle);

/// @brief モデルの色（乗算カラー）を設定する
/// @param modelHandle モデルハンドル
/// @param color 色（RGBA）
void SetModelColor(int modelHandle, const Vector4 &color);

/// @brief アニメーションをアタッチする（モデルに紐づけられたファイルパスを使用）
/// @param modelHandle モデルハンドル
void AttachModelAnimation(int modelHandle);

/// @brief アニメーションを別ファイルからアタッチする
/// @param modelHandle モデルハンドル
/// @param filePath アニメーションファイルのパス
void AttachModelAnimation(int modelHandle, const std::string& filePath);

/// @brief モデルのアニメーション状態を更新する
/// @param modelHandle モデルハンドル
/// @param dt 経過時間 (負の値ならエンジンの deltaTime を自動使用)
void UpdateModelAnimation(int modelHandle, float dt = -1.0f);

/// @brief モデルのスケルトンをデバッグ描画する（Joint球 + Bone線）
/// @param modelHandle モデルハンドル
void DrawModelSkeleton(int modelHandle);

/// @brief モデルのライティングモードを設定する
/// @param modelHandle モデルハンドル
/// @param m ライティングモード
void SetModelLightingMode(int modelHandle, LightingMode m);

/// @brief モデルが参照する Mesh を差し替える（内部キャッシュで共有）
/// @param modelHandle モデルハンドル
/// @param path .obj へのパス
void SetModelMesh(int modelHandle, const std::string &path);

/// @brief DrawModelBatch の内部カーソルをリセットする
/// @param modelHandle モデルハンドル
void ResetCursor(int modelHandle);

// ── ガラスモデル ──────────────────────────────────

/// @brief ガラスモデルを描画する
/// @param modelHandle モデルハンドル
/// @param texHandle テクスチャハンドル（-1 なら mtl のテクスチャ）
void DrawModelGlass(int modelHandle, int texHandle = -1);

/// @brief ガラスモデルをインスタンシングで描画する
/// @param modelHandle モデルハンドル
/// @param instances インスタンスTransform配列
/// @param texHandle テクスチャハンドル（-1 なら mtl のテクスチャ）
void DrawModelGlassBatch(int modelHandle,
                         const std::vector<Transform> &instances,
                         int texHandle = -1);

/// @brief ガラスモデルをインスタンシング（複数Transform + 単一色）で描画する
/// @param modelHandle モデルハンドル
/// @param instances インスタンスTransform配列
/// @param color 全インスタンスに適用する乗算色
/// @param texHandle テクスチャハンドル（-1 なら mtl のテクスチャ）
void DrawModelGlassBatchColored(int modelHandle,
                                const std::vector<Transform> &instances,
                                const Vector4 &color, int texHandle = -1);

// ── ガラス（2パス：背面→表面） ──────────────────
// ※箱/ブロックみたいな「厚み」を出したい時に使う

/// @brief ガラス（2パス）を描画する
/// @param modelHandle モデルハンドル
/// @param texHandle テクスチャハンドル（-1 なら mtl のテクスチャ）
void DrawModelGlassTwoPass(int modelHandle, int texHandle = -1);

/// @brief ガラス（2パス）をインスタンシングで描画する
/// @param modelHandle モデルハンドル
/// @param instances インスタンスTransform配列
/// @param texHandle テクスチャハンドル（-1 なら mtl のテクスチャ）
void DrawModelGlassTwoPassBatch(int modelHandle,
                                const std::vector<Transform> &instances,
                                int texHandle = -1);

/// @brief ガラス（2パス）をインスタンシング（複数Transform + 単一色）で描画する
/// @param modelHandle モデルハンドル
/// @param instances インスタンスTransform配列
/// @param color 全インスタンスに適用する乗算色
/// @param texHandle テクスチャハンドル（-1 なら mtl のテクスチャ）
void DrawModelGlassTwoPassBatchColored(int modelHandle,
                                       const std::vector<Transform> &instances,
                                       const Vector4 &color,
                                       int texHandle = -1);


// ── 2D用 ──────────────────────────────────────────

/// @brief 2D描画の前処理（このフレームの SceneContext / CommandList を登録）
/// @param ctx SceneContext
/// @param cl このフレームで使うコマンドリスト
/// @note この関数を呼んだ後に DrawSprite / DrawLine などを呼んでください。
void PreDraw2D(SceneContext &ctx, ID3D12GraphicsCommandList *cl);

/// @brief スプライト用テクスチャをロードし、Sprite2D を生成してハンドルを返す
/// @param path 画像パス
/// @param ctx SceneContext（画面サイズ取得に使用）
/// @param srgb sRGBとして読み込むか
/// @return スプライトハンドル（失敗時は -1）
int LoadSprite(const std::string &path, SceneContext &ctx, bool srgb = true);

/// @brief スプライトを描画する
/// @param spriteHandle スプライトハンドル
void DrawSprite(int spriteHandle);

/// @brief スプライトを「テクスチャ内の矩形」を指定して描画する（スプライトシート用）
/// @param spriteHandle スプライトハンドル
/// @param srcX 切り出し矩形 左上X（ピクセル）
/// @param srcY 切り出し矩形 左上Y（ピクセル）
/// @param srcW 切り出し幅（ピクセル）
/// @param srcH 切り出し高さ（ピクセル）
/// @param texW テクスチャ全体の幅（ピクセル）
/// @param texH テクスチャ全体の高さ（ピクセル）
/// @param insetPx にじみ対策の内側オフセット（ピクセル）
/// @note 位置/回転/サイズは SetSpriteTransform で設定した値を使用します。
void DrawSpriteRect(int spriteHandle, float srcX, float srcY, float srcW,
                    float srcH, float texW, float texH, float insetPx = 0.0f);

/// @brief スプライトを UV(0..1) の矩形を指定して描画する
/// @param spriteHandle スプライトハンドル
/// @param u0 開始 U
/// @param v0 開始 V
/// @param u1 終了 U
/// @param v1 終了 V
void DrawSpriteRectUV(int spriteHandle, float u0, float v0, float u1, float v1);

/// @brief スプライトの Transform を設定する
/// @param spriteHandle スプライトハンドル
/// @param t Transform
void SetSpriteTransform(int spriteHandle, const Transform &t);

/// @brief スプライトの色（乗算カラー）を設定する
/// @param spriteHandle スプライトハンドル
/// @param color 色（RGBA）
void SetSpriteColor(int spriteHandle, const Vector4 &color);

/// @brief スプライトを解放する
/// @param spriteHandle スプライトハンドル
void UnloadSprite(int spriteHandle);

/// @brief スプライトのサイズを設定する
/// @param spriteHandle スプライトハンドル
/// @param w 幅（ピクセル）
/// @param h 高さ（ピクセル）
void SetSpriteScreenSize(int spriteHandle, float w, float h);

/// @brief スプライトの ImGui 表示を行う
/// @param spriteHandle スプライトハンドル
/// @param name 表示名
void DrawImGui2D(int spriteHandle, const char *name);

// ── 天球用 (Skydome) ──────────────────────────────

/// @brief 天球を生成する（最小形：半径100.0, 32x32）
/// @param textureHandle テクスチャハンドル（必須）
/// @return 天球ハンドル（失敗時は -1）
int GenerateSkydome(int textureHandle);

/// @brief 天球を生成する（パラメータ指定版）
/// @param textureHandle テクスチャハンドル（-1 なら未設定）
/// @param radius 半径
/// @param sliceCount スライス数
/// @param stackCount スタック数
/// @return 天球ハンドル（失敗時は -1）
int GenerateSkydomeEx(int textureHandle = -1, float radius = 100.0f,
                      unsigned int sliceCount = 32, unsigned int stackCount = 32);

/// @brief 天球を描画する
/// @param skydomeHandle 天球ハンドル
/// @param texHandle 一時的に差し替えるテクスチャ（-1 なら生成時のテクスチャ）
void DrawSkydome(int skydomeHandle, int texHandle = -1);

/// @brief 天球の ImGui 表示を行う
/// @param skydomeHandle 天球ハンドル
/// @param name 表示名
void DrawSkydomeImGui(int skydomeHandle, const char *name = nullptr);

/// @brief 天球を解放する
/// @param skydomeHandle 天球ハンドル
void UnloadSkydome(int skydomeHandle);

/// @brief 天球の Transform ポインタを取得する
/// @param skydomeHandle 天球ハンドル
/// @return Transform*（無効ハンドルなら nullptr）
Transform *GetSkydomeTransformPtr(int skydomeHandle);

/// @brief 天球の色（乗算カラー）を設定する
/// @param skydomeHandle 天球ハンドル
/// @param color 色（RGBA）
void SetSkydomeColor(int skydomeHandle, const Vector4 &color);


// ── スカイボックス用 (Skybox) ────────────────────────

/// @brief DDSキューブマップからスカイボックスを生成する
/// @param ddsPath DDSファイルのパス（cubemap形式）
/// @return スカイボックスハンドル（失敗時は -1）
int CreateSkyBox(const std::string &ddsPath);

/// @brief スカイボックスを描画する（3Dパス内で使用）
/// @param skyboxHandle スカイボックスハンドル
void DrawSkyBox(int skyboxHandle);

/// @brief スカイボックスの ImGui 表示を行う
/// @param skyboxHandle スカイボックスハンドル
/// @param name 表示名
void DrawSkyBoxImGui(int skyboxHandle, const char *name = nullptr);

/// @brief スカイボックスを解放する
/// @param skyboxHandle スカイボックスハンドル
void UnloadSkyBox(int skyboxHandle);

/// @brief スカイボックスの Transform ポインタを取得する
/// @param skyboxHandle スカイボックスハンドル
/// @return Transform*（無効ハンドルなら nullptr）
Transform *GetSkyBoxTransformPtr(int skyboxHandle);

/// @brief スカイボックスの色（乗算カラー）を設定する
/// @param skyboxHandle スカイボックスハンドル
/// @param color 色（RGBA）
void SetSkyBoxColor(int skyboxHandle, const Vector4 &color);


// ── 環境マップ（Environment Map） ────────────────────

/// @brief Skybox の Cubemap を環境マップとして登録する
/// @param skyboxHandle スカイボックスハンドル
/// @note DrawSkyBox() を呼んだ場合は自動的に登録されます。
void SetEnvironmentMap(int skyboxHandle);

/// @brief モデルの環境マップ映り込み係数を設定する
/// @param modelHandle モデルハンドル
/// @param coeff 映り込み係数（0=映り込みなし、1=完全鏡面）
void SetModelEnvironmentCoefficient(int modelHandle, float coeff);

// ── 汎用プロシージャルメッシュ（PrimitiveMesh） ──────────

/// @brief 平面メッシュを生成
/// @param width 幅
/// @param height 高さ
/// @param texHandle テクスチャハンドル
/// @return メッシュハンドル
int GeneratePlane(float width = 1.0f, float height = 1.0f, int texHandle = -1);

/// @brief ボックスメッシュを生成
/// @param width 幅
/// @param height 高さ
/// @param depth 奥行き
/// @param texHandle テクスチャハンドル
/// @return メッシュハンドル
int GenerateBox(float width = 1.0f, float height = 1.0f, float depth = 1.0f, int texHandle = -1);

/// @brief 球体メッシュを生成（外向き）
/// @param radius 半径
/// @param texHandle テクスチャハンドル
/// @return メッシュハンドル
int GenerateSphere(float radius = 0.5f, int texHandle = -1);

/// @brief 円柱メッシュを生成
/// @param radius 半径
/// @param height 高さ
/// @param texHandle テクスチャハンドル
/// @return メッシュハンドル
int GenerateCylinder(float radius = 0.5f, float height = 1.0f, int texHandle = -1);

/// @brief 円錐メッシュを生成
/// @param radius 半径
/// @param height 高さ
/// @param texHandle テクスチャハンドル
/// @return メッシュハンドル
int GenerateCone(float radius = 0.5f, float height = 1.0f, int texHandle = -1);

/// @brief トーラスメッシュを生成
/// @param majorRadius 大半径
/// @param minorRadius 小半径
/// @param texHandle テクスチャハンドル
/// @return メッシュハンドル
int GenerateTorus(float majorRadius = 1.0f, float minorRadius = 0.2f, int texHandle = -1);

/// @brief カプセルメッシュを生成
/// @param radius 半径
/// @param height 高さ
/// @param texHandle テクスチャハンドル
/// @return メッシュハンドル
int GenerateCapsule(float radius = 0.5f, float height = 2.0f, int texHandle = -1);

/// @brief 汎用プリミティブメッシュを描画
/// @param meshHandle メッシュハンドル
/// @param texHandle 一時的に差し替えるテクスチャ（-1 なら生成時のテクスチャ）
void DrawPrimitiveMesh(int meshHandle, int texHandle = -1);

/// @brief 汎用プリミティブメッシュを解放
/// @param meshHandle メッシュハンドル
void UnloadPrimitiveMesh(int meshHandle);

/// @brief 汎用プリミティブメッシュの Transform ポインタを取得
/// @param meshHandle メッシュハンドル
/// @return Transform*（無効ハンドルなら nullptr）
Transform *GetPrimitiveMeshTransformPtr(int meshHandle);

/// @brief 汎用プリミティブメッシュの ImGui 表示を行う
/// @param meshHandle メッシュハンドル
/// @param name 表示名
void DrawPrimitiveMeshImGui(int meshHandle, const char *name = nullptr);


// ── Primitive2D（即時描画） ──────────────────────────

/// @brief 線描画
/// @param pos1 座標1（画面ピクセル座標）
/// @param pos2 座標2（画面ピクセル座標）
/// @param color 色（RGBA）
/// @param thickness 線の太さ（ピクセル）
/// @param feather アンチエイリアス幅（ピクセル）
void DrawLine(const Vector2 &pos1, const Vector2 &pos2, const Vector4 &color,
              float thickness = 1.0f, float feather = 1.0f);

/// @brief 四角形描画
/// @param pos1 座標1（画面ピクセル座標）
/// @param pos2 座標2（画面ピクセル座標）
/// @param color 色（RGBA）
/// @param fillMode 塗りつぶし設定
/// @param feather アンチエイリアス幅（ピクセル）
void DrawBox(const Vector2 &pos1, const Vector2 &pos2, const Vector4 &color,
             kFillMode fillMode = kFill, float feather = 1.0f);

/// @brief 円描画
/// @param center 円の中心点（画面ピクセル座標）
/// @param radius 円の半径（ピクセル）
/// @param color 色（RGBA）
/// @param fillMode 塗りつぶし設定
/// @param feather アンチエイリアス幅（ピクセル）
void DrawCircle(const Vector2 &center, float radius, const Vector4 &color,
                kFillMode fillMode = kFill, float feather = 1.0f);

/// @brief 三角形描画
/// @param pos1 頂点1（画面ピクセル座標）
/// @param pos2 頂点2（画面ピクセル座標）
/// @param pos3 頂点3（画面ピクセル座標）
/// @param color 色（RGBA）
/// @param fillMode 塗りつぶし設定
/// @param feather アンチエイリアス幅（ピクセル）
void DrawTriangle(const Vector2 &pos1, const Vector2 &pos2, const Vector2 &pos3,
                  const Vector4 &color, kFillMode fillMode = kFill,
                  float feather = 1.0f);

// ── Primitive3D（デバッグ線） ──────────────────────────

/// @brief 3D線描画
/// @param a 始点（ワールド座標）
/// @param b 終点（ワールド座標）
/// @param color 色（RGBA）
/// @param depth 深度テストを行うか
void DrawLine3D(const Vector3 &a, const Vector3 &b, const Vector4 &color,
                bool depth = true);

/// @brief 3DAABB描画
/// @param mn 最小座標（ワールド座標）
/// @param mx 最大座標（ワールド座標）
/// @param color 色（RGBA）
/// @param depth 深度テストを行うか
void DrawAABB3D(const Vector3 &mn, const Vector3 &mx, const Vector4 &color,
                bool depth = true);

/// @brief 3Dグリッド - XZ描画
/// @param halfSize グリッドの半分のサイズ
/// @param step グリッドの間隔
/// @param color 色（RGBA）
/// @param depth 深度テストを行うか
void DrawGridXZ3D(int halfSize, float step, const Vector4 &color,
                  bool depth = true);

/// @brief 3Dグリッド - XY描画
/// @param halfSize グリッドの半分のサイズ
/// @param step グリッドの間隔
/// @param color 色（RGBA）
/// @param depth 深度テストを行うか
void DrawGridXY3D(int halfSize, float step, const Vector4 &color,
                  bool depth = true);

/// @brief 3Dグリッド - YZ描画
/// @param halfSize グリッドの半分のサイズ
/// @param step グリッドの間隔
/// @param color 色（RGBA）
/// @param depth 深度テストを行うか
void DrawGridYZ3D(int halfSize, float step, const Vector4 &color,
                  bool depth = true);

/// @brief 3Dワイヤー球（緯線/経線）
/// @param center 中心（ワールド座標）
/// @param radius 半径
/// @param color 色（RGBA）
/// @param slices 経度分割
/// @param stacks 緯度分割
/// @param depth 深度テストを行うか
void DrawWireSphere3D(const Vector3 &center, float radius, const Vector4 &color,
                      int slices = 24, int stacks = 12, bool depth = true);

/// @brief 3Dリング球（XY/XZ/YZ の 3本リング）
/// @param center 中心（ワールド座標）
/// @param radius 半径
/// @param color 色（RGBA）
/// @param segments 円の分割数
/// @param depth 深度テストを行うか
void DrawSphereRings3D(const Vector3 &center, float radius,
                       const Vector4 &color, int segments = 32,
                       bool depth = true);

/// @brief 3Dアーク（円弧 / 扇形）
/// @param center 中心（ワールド座標）
/// @param normal 面の法線（ワールド）
/// @param fromDir 開始方向
/// @param radius 半径
/// @param startRad 開始角（ラジアン）
/// @param endRad 終了角（ラジアン）
/// @param color 色（RGBA）
/// @param segments 分割数
/// @param depth 深度テストを行うか
/// @param drawToCenter true なら扇形の中心線も描く
void DrawArc3D(const Vector3 &center, const Vector3 &normal,
               const Vector3 &fromDir, float radius, float startRad,
               float endRad, const Vector4 &color, int segments = 32,
               bool depth = true, bool drawToCenter = false);

/// @brief ワイヤーカプセル
/// @param p0 端の球の中心（ワールド座標）
/// @param p1 端の球の中心（ワールド座標）
/// @param radius 半径
/// @param color 色（RGBA）
/// @param segments 円/半円の分割数
/// @param depth 深度テストを行うか
void DrawCapsule3D(const Vector3 &p0, const Vector3 &p1, float radius,
                   const Vector4 &color, int segments = 16,
                   bool depth = true);

/// @brief OBB（回転付き箱）
/// @param center 中心（ワールド座標）
/// @param axisX ローカルX軸（ワールド）
/// @param axisY ローカルY軸（ワールド）
/// @param axisZ ローカルZ軸（ワールド）
/// @param halfExtents 各軸方向の半サイズ
/// @param color 色（RGBA）
/// @param depth 深度テストを行うか
void DrawOBB3D(const Vector3 &center, const Vector3 &axisX,
               const Vector3 &axisY, const Vector3 &axisZ,
               const Vector3 &halfExtents, const Vector4 &color,
               bool depth = true);

/// @brief 視錐台（コーナー8点）
/// @param corners corners[0..3]=near, corners[4..7]=far
/// @param color 色
/// @param depth 深度テストを行うか
void DrawFrustumCorners3D(const Vector3 corners[8], const Vector4 &color,
                          bool depth = true);

/// @brief 視錐台（カメラパラメータから生成）
void DrawFrustum3D(const Vector3 &camPos, const Vector3 &forward,
                   const Vector3 &up, float fovYRad, float aspect,
                   float nearZ, float farZ, const Vector4 &color,
                   bool depth = true);

/// @brief 内部バッファの即時描画（通常は自動で呼ばれます）
void FlushPrimitive3D();


// ===============================
// ── 画面オーバーレイ（ポスト風） ───────────────────

/// @brief 画面全体に白い霧（寒い雰囲気）をオーバーレイする
/// @param timeSec 時間（秒）。ノイズを流すのに使用
/// @param intensity 濃さ（0..1）
/// @param scale 模様の大きさ（大きいほど細かい）
/// @param speed 流れる速度（秒あたり）
/// @param wind 流れる方向（UV空間）
/// @param feather もやの柔らかさ
/// @param bottomBias 下側を濃くする係数
/// @note 3D/2D を描いた後、最後に呼ぶと画面全体にかかります。
void DrawFogOverlay(float timeSec, float intensity, float scale, float speed,
                    const Vector2 &wind, float feather, float bottomBias);

/// @brief FogOverlay の色（霧の色味）を設定する
/// @param color RGBA(0..1)
void SetFogOverlayColor(const Vector4 &color);


// ── 共通関数 ────────────────────────────────────

/// @brief テクスチャをロードしてハンドルを返す
/// @param path 画像パス
/// @param srgb sRGBとして読み込むか
/// @return テクスチャハンドル（失敗時は -1）
int LoadTex(const std::string &path, bool srgb = true);

/// @brief テクスチャハンドルから SRV の GPU デスクリプタハンドルを取得する
/// @param texHandle テクスチャハンドル
/// @return D3D12_GPU_DESCRIPTOR_HANDLE（無効なら ptr=0）
D3D12_GPU_DESCRIPTOR_HANDLE GetSrv(int texHandle);

/// @brief RenderCommon が保持している D3D12 Device を取得する
/// @return ID3D12Device*
ID3D12Device *GetDevice();

/// @brief RenderCommon が初期化済みかどうか
/// @return 初期化済みなら true
bool IsInitialized();


// ── ブレンドモード切り替え ────────────────────────

/// @brief 以降の Draw* が使用する BlendMode を設定する
/// @param blendMode ブレンドモード
void SetBlendMode(BlendMode blendMode);

/// @brief 現在の BlendMode を取得する
/// @return ブレンドモード
BlendMode GetBlendMode();


// ── 表示モード切り替え ──────────────────────────

/// @brief 以降の 3D 描画のシェーディングモードを設定する
/// @param mode シェーディングモード（Solid / Wireframe / SolidWireframe）
void SetViewShadingMode(ViewShadingMode mode);

/// @brief 現在のシェーディングモードを取得する
/// @return シェーディングモード
ViewShadingMode GetViewShadingMode();

/// @brief シェーディングモードの ImGui 表示を行う
/// @param label ラベル名
void DrawViewShadingModeImGui(const char *label = "View Shading");


// ── ポストエフェクト切り替え ────────────────────────

/// @brief ポストエフェクトを1つだけ設定する（スタックをクリア）
/// @param type ポストエフェクトタイプ（None でエフェクトなし）
void SetPostEffect(::PostEffectType type);

/// @brief 先頭のエフェクトを返す（空なら None）
/// @return ポストエフェクトタイプ
::PostEffectType GetPostEffect();

/// @brief エフェクトをスタックに追加する（重ね掛け）
/// @param type ポストエフェクトタイプ
void AddPostEffect(::PostEffectType type);

/// @brief エフェクトをスタックから除去する
/// @param type ポストエフェクトタイプ
void RemovePostEffect(::PostEffectType type);

/// @brief エフェクトスタックを全クリアする
void ClearPostEffects();

/// @brief 指定したエフェクトが有効か確認する
/// @param type エフェクトタイプ
/// @return 有効なら true
bool HasPostEffect(::PostEffectType type);

/// @brief ポストプロセスのアウトライン色を設定する
/// @param color 色（RGBA）
void SetPostProcessOutlineColor(const float color[4]);

/// @brief ポストプロセスのアウトライン検出の重みを設定する
/// @param weight 重み
void SetPostProcessOutlineWeight(float weight);

/// @brief ポストプロセスのアウトラインのピクセル幅（太さ）を設定する
/// @param thickness 太さ
void SetPostProcessOutlineThickness(float thickness);

// ── Dissolve ポストエフェクト ────────────────────────

/// @brief Dissolve の閾値を設定する (0.0 ~ 1.0)
/// @param threshold 0.0で全表示、1.0で全消失
void SetDissolveThreshold(float threshold);

/// @brief Dissolve の Edge 発光色を設定する
/// @param r 赤 (0.0 ~ 1.0)
/// @param g 緑 (0.0 ~ 1.0)
/// @param b 青 (0.0 ~ 1.0)
void SetDissolveEdgeColor(float r, float g, float b);

/// @brief Dissolve の ベースカラー（抜けた部分の背景色）を設定する
/// @param r 赤 (0.0 ~ 1.0)
/// @param g 緑 (0.0 ~ 1.0)
/// @param b 青 (0.0 ~ 1.0)
/// @param a アルファ (0.0 ~ 1.0)
void SetDissolveBaseColor(float r, float g, float b, float a);

/// @brief Dissolve の Edge 検出幅を設定する
/// @param range Edge幅 (default: 0.03)
void SetDissolveEdgeRange(float range);

/// @brief Dissolve のノイズテクスチャを切り替える
/// @param index ノイズテクスチャのインデックス
void SetDissolveNoiseIndex(int index);


/// @brief Dissolve のノイズテクスチャリストを初期化する
/// @note RC::Init 完了後に呼び出してください
void InitDissolveNoiseTextures();

/// @brief RandomNoise の強度を設定する (0.0 ~ 1.0)
void SetRandomNoiseIntensity(float intensity);

/// @brief RandomNoise の色を設定する (RGB)
void SetRandomNoiseColor(float r, float g, float b);

/// @brief ポストエフェクトの ImGui 表示を行う
/// @param label ラベル名
void DrawPostEffectImGui(const char *label = "PostEffect");

/// @brief バックグラウンドでのロードタスクを追加する
/// @param task ロードタスク
void AddLoadingTask(std::future<void> &&task);

/// @brief 現在進行中のすべてのバックグラウンドロードタスクが完了するまで待機する
void WaitAllLoads();

/// @brief テクスチャのログ出力履歴をクリアする
void ClearTextureLogHistory();

} // namespace RC
