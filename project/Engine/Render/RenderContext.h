#pragma once

// ============================================================================
// RenderContext
// ----------------------------------------------------------------------------
// エンジン内部専用。描画サブシステム全体の状態を保持するクラス。
// RenderCommon.cpp が唯一のインスタンスを所有し、各サブモジュール
// (RenderModel.cpp 等) は GetRenderContext() 経由で参照する。
//
// ★ このヘッダーをシーン側 (#include "RenderCommon.h" のみ) が直接
//    include することはない。
// ============================================================================

#include <d3d12.h>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>
#include <wrl.h>
#include <future>
#include <mutex>

#include "RenderQueue.h"
#include "FrameResource.h"


#include "Light/Area/AreaLightManager.h"
#include "Light/Directional/DirectionalLightManager.h"
#include "Light/Point/PointLightManager.h"
#include "Light/Spot/SpotLightManager.h"
#include "Model/ModelManager.h"
#include "Skydome/SkydomeManager.h"
#include "Skybox/SkyboxManager.h"
#include "Mesh/PrimitiveMeshManager.h"
#include "Sprite/SpriteManager.h"
#include "Texture/TextureManager/TextureManager.h"

#include "GraphicsPipeline/GraphicsPipeline.h" // BlendMode
#include "Math/Math.h"
#include "Model/ModelObject.h" // ModelManager の unique_ptr<ModelObject> に必要
#include "Graphics/Skydome/Skydome.h"    // SkydomeManager の unique_ptr<Skydome> に必要
#include "Graphics/Skybox/Skybox.h"      // SkyboxManager の unique_ptr<Skybox> に必要
#include "Graphics/Mesh/PrimitiveMesh.h"
#include "Graphics/Mesh/MeshGenerator.h"
#include "function/function.h"
#include "struct.h"

class Primitive2D;
class Primitive3D;
class GraphicsPipeline;
class DescriptorHeap;
class PostProcess;
struct SceneContext;

namespace RC {

/// @brief 描画サブシステム全体の状態と各リソースマネージャを統括するコンテキストクラス
/// エンジン内部専用であり、シングルトンとして描画パスの実行、コマンドのキューイング、マネージャへのアクセスを提供します。
class RenderContext {
public:
  /// @brief シングルトンインスタンスを取得する
  /// @return RenderContextの参照
  static RenderContext &GetInstance();

  /// @brief 初期化処理
  /// @param ctx シーンコンテキスト
  void Init(SceneContext &ctx);
  
  /// @brief 終了処理
  void Term();
  
  /// @brief 初期化済みか確認する
  /// @return 初期化済みなら true
  bool IsInitialized() const { return initialized_; }

  /// @brief フレームごとのカメラ情報を設定する
  /// @param view ビュー行列
  /// @param proj 射影行列
  /// @param camWorldPos カメラのワールド座標
  void SetCamera(const Matrix4x4 &view, const Matrix4x4 &proj,
                 const Vector3 &camWorldPos);
  
  /// @brief 現在のフレームで使用するグラフィックスコマンドリストを設定する
  /// @param cl コマンドリストへのポインタ
  void SetCommandList(ID3D12GraphicsCommandList *cl) { cl_ = cl; }
  
  /// @brief シーンコンテキストへの参照を設定する
  /// @param ctx シーンコンテキストへのポインタ
  void SetSceneContext(SceneContext *ctx) { ctxRef_ = ctx; }
  
  /// @brief 現在のブレンドモードを設定する
  /// @param mode 設定するブレンドモード
  void SetBlendMode(BlendMode mode) { currentBlendMode_ = mode; }

  /// @brief 3D描画パスの開始前処理
  /// 各種定数バッファの更新やバインドを行います。
  /// @param ctx シーンコンテキスト
  /// @param cl コマンドリスト
  void PreDraw3D(SceneContext &ctx, ID3D12GraphicsCommandList *cl);
  
  /// @brief 2D描画パスの開始前処理
  /// @param ctx シーンコンテキスト
  /// @param cl コマンドリスト
  void PreDraw2D(SceneContext &ctx, ID3D12GraphicsCommandList *cl);

  /// @brief ビューのシェーディングモード（Solid, Wireframe 等）を設定する
  /// @param mode シェーディングモード
  void SetViewShadingMode(ViewShadingMode mode) { viewShadingMode_ = mode; }
  
  /// @brief 現在のビューシェーディングモードを取得する
  /// @return シェーディングモード
  ViewShadingMode GetViewShadingMode() const { return viewShadingMode_; }

  /// @brief テクスチャをロードする（マネージャへのラップ）
  /// @param path 画像パス
  /// @param srgb sRGBとして読み込むか
  /// @return テクスチャハンドル
  int LoadTex(const std::string &path, bool srgb);
  
  /// @brief テクスチャのSRVハンドルを取得する
  /// @param texHandle テクスチャハンドル
  /// @return GPUディスクリプタハンドル
  D3D12_GPU_DESCRIPTOR_HANDLE GetSrv(int texHandle);

  // ── アクセッサ ──
  /// @brief 現在のコマンドリストを取得
  ID3D12GraphicsCommandList *CL() const { return cl_; }
  /// @brief シーンコンテキストを取得
  SceneContext *Ctx() const { return ctxRef_; }
  /// @brief D3D12デバイスを取得
  ID3D12Device *Device() const { return device_.Get(); }
  /// @brief 現在のブレンドモードを取得
  BlendMode CurrentBlendMode() const { return currentBlendMode_; }
  /// @brief ビュー行列を取得
  const Matrix4x4 &View() const { return view_; }
  /// @brief 射影行列を取得
  const Matrix4x4 &Proj() const { return proj_; }

  // ── マネージャへの参照 ──
  /// @brief モデルマネージャを取得
  ModelManager &Models() { return modelMan_; }
  /// @brief スプライトマネージャを取得
  SpriteManager &Sprites() { return spriteMan_; }
  /// @brief スカイドームマネージャを取得
  SkydomeManager &Skydomes() { return skydomeMan_; }
  /// @brief スカイボックスマネージャを取得
  SkyboxManager &SkyBoxes() { return skyboxMan_; }
  /// @brief プリミティブメッシュマネージャを取得
  PrimitiveMeshManager &PrimitiveMeshes() { return primitiveMeshMan_; }
  /// @brief 平行光源マネージャを取得
  DirectionalLightManager &DirLights() { return dirLightMan_; }
  /// @brief 点光源マネージャを取得
  PointLightManager &PtLights() { return ptLightMan_; }
  /// @brief スポットライトマネージャを取得
  SpotLightManager &SpLights() { return spLightMan_; }
  /// @brief エリアライトマネージャを取得
  AreaLightManager &ArLights() { return arLightMan_; }
  /// @brief テクスチャマネージャを取得
  TextureManager &Textures() { return texMan_; }

  /// @brief ポストプロセスオブジェクトを取得
  /// @return PostProcessへのポインタ
  PostProcess *GetPostProcess() const { return postProcess_; }

  /// @brief プレフィックスと現在のブレンドモードに基づいてPSOをバインドする
  /// @param prefix パイプラインのプレフィックス（"Object3D"等）
  /// @return バインドされたGraphicsPipelineへのポインタ
  GraphicsPipeline *BindPipeline(std::string_view prefix);
  
  /// @brief 条件に合うGraphicsPipelineを取得する（バインドはしない）
  /// @param prefix プレフィックス
  /// @param mode ブレンドモード
  /// @return GraphicsPipelineへのポインタ
  GraphicsPipeline *GetPipeline(std::string_view prefix, BlendMode mode);
  
  /// @brief カメラ定数バッファをバインドする
  void BindCameraCB();
  
  /// @brief 全種類のライト定数バッファを一括でバインドする
  void BindAllLightCBs();

  /// @brief 環境マップ用のSRVを設定する（PBR等で使用）
  /// @param srv GPUディスクリプタハンドル
  void SetEnvironmentMap(D3D12_GPU_DESCRIPTOR_HANDLE srv) { environmentMapSrv_ = srv; }
  
  /// @brief 現在設定されている環境マップのSRVを取得する
  /// @return GPUディスクリプタハンドル
  D3D12_GPU_DESCRIPTOR_HANDLE GetEnvironmentMapSrv() const { return environmentMapSrv_; }
  
  /// @brief 環境マップを現在のスロット（b6等）にバインドする
  void BindEnvironmentMap();

  /// @brief 2Dプリミティブ描画オブジェクトを遅延生成・取得する
  Primitive2D *EnsurePrimitive2D();
  
  /// @brief 3Dプリミティブ描画オブジェクトを遅延生成・取得する
  Primitive3D *EnsurePrimitive3D();

  /// @brief 実行中のすべての非同期ロードタスクの完了を待機する
  void WaitAllLoads();
  
  /// @brief 非同期ロードタスクを追加する
  /// @param task futureオブジェクト
  void AddLoadingTask(std::future<void> &&task);

  /// @brief 描画コマンドの履歴保存用構造体
  struct RenderCommandHistory {
    std::string debugName;
    int debugIndex;
    uint64_t sortKey;
  };

  /// @brief 描画履歴にコマンドを追加する
  void AddCommandHistory(const char* name, int index, uint64_t sortKey = 0) {
    currentCommandHistory_.push_back({name, index, sortKey});
  }

  /// @brief 3D描画用コマンドの構造体
  struct RenderCommand3D {
    enum Type {
      Other,      ///< 汎用ラムダ実行
      Primitive,  ///< プリミティブ描画のマージ用
    } type = Other;

    uint64_t sortKey = 0; ///< ソート順（0は追加順維持）
    std::function<void(ID3D12GraphicsCommandList *)> func; ///< 実行する処理

    // Primitive マージ用の情報
    bool primDepth = false;
    uint32_t primStart = 0;
    uint32_t primCount = 0;

    const char* debugName = "Unknown"; ///< デバッグ用コマンド名
    int debugIndex = -1; ///< デバッグ用ハンドル番号(Index)
  };

  /// @brief 3D描画コマンドをキューに追加する
  /// @param func 実行する関数
  /// @param debugName デバッグ用コマンド名
  /// @param debugIndex デバッグ用ハンドル番号
  void PushCommand3D(std::function<void(ID3D12GraphicsCommandList *)> func, const char* debugName = "Unknown", int debugIndex = -1) {
    RenderCommand3D cmd;
    cmd.type = RenderCommand3D::Other;
    cmd.func = std::move(func);
    cmd.debugName = debugName;
    cmd.debugIndex = debugIndex;
    commandQueue3D_.push_back(std::move(cmd));
  }

  /// @brief ソートキーを指定して3D描画コマンドをキューに追加する
  /// @param sortKey ソート順序
  /// @param func 実行する関数
  /// @param debugName デバッグ用コマンド名
  /// @param debugIndex デバッグ用ハンドル番号
  void PushCommand3D(uint64_t sortKey,
                     std::function<void(ID3D12GraphicsCommandList *)> func, const char* debugName = "Unknown", int debugIndex = -1) {
    RenderCommand3D cmd;
    cmd.type = RenderCommand3D::Other;
    cmd.sortKey = sortKey;
    cmd.func = std::move(func);
    cmd.debugName = debugName;
    cmd.debugIndex = debugIndex;
    commandQueue3D_.push_back(std::move(cmd));
  }

  /// @brief 3Dプリミティブ描画コマンドを追加する
  void PushPrimitive3DCommand(bool depth, uint32_t start, uint32_t count,
                              uint64_t sortKey = 0);
  
  /// @brief 次のフレームの Execute3DCommands 時にコマンドの実行順序をログ出力するよう要求する
  void RequestDumpCommandOrder() { dumpCommandOrder_ = true; }

  /// @brief キューに積まれた全3D描画コマンドを実行する
  void Execute3DCommands();
  
  /// @brief 直前のフレームで実行された描画コマンド(3D/2D)のリストを取得する（デバッグ用）
  const std::vector<RenderCommandHistory>& GetLastCommandHistory() const { return lastCommandHistory_; }

  /// @brief 3D描画コマンドキューをクリアする
  void Clear3DCommands() { commandQueue3D_.clear(); }

  /// @brief オーバーレイモードを設定する
  void SetOverlayMode(bool enabled) { overlayMode_ = enabled; }
  /// @brief オーバーレイモードかどうか
  bool IsOverlayMode() const { return overlayMode_; }

  /// @brief 現在のフレーム用リソースアロケータを取得する
  FrameResource &CurrentFrame() { return frameResources_[frameIndex_]; }
  
  /// @brief 現在のフレームインデックスを取得
  uint32_t FrameIndex() const { return frameIndex_; }
  
  /// @brief フレームインデックスを進める
  void AdvanceFrame() { frameIndex_ = (frameIndex_ + 1) % FrameResource::kFrameCount; }

  /// @brief フォグ(Fog)用定数バッファを更新する
  void UpdateFogCB(float timeSec, float intensity, float scale, float speed,
                   const Vector2 &wind, float feather, float bottomBias);
  
  /// @brief フォグの色を設定する
  void SetFogColor(const Vector4 &color);
  
  /// @brief フォグ用定数バッファのリソースを取得する
  ID3D12Resource *FogCBResource() const { return fogCB_.Get(); }

private:
  bool initialized_ = false; ///< 初期化フラグ
  bool overlayMode_ = false; ///< オーバーレイモード（ギズモ用）
  bool dumpCommandOrder_ = false; ///< 1フレームだけコマンド実行順をダンプするフラグ

  Microsoft::WRL::ComPtr<ID3D12Device> device_;    ///< デバイス
  DescriptorHeap *srvHeap_ = nullptr;              ///< SRVヒープ
  ID3D12GraphicsCommandList *cl_ = nullptr;        ///< コマンドリスト
  SceneContext *ctxRef_ = nullptr;                 ///< シーンコンテキストへのポインタ
  PostProcess *postProcess_ = nullptr;             ///< ポストプロセス

  Matrix4x4 view_;                                 ///< ビュー行列
  Matrix4x4 proj_;                                 ///< 射影行列
  BlendMode currentBlendMode_ = kBlendModeNone;    ///< 現在のブレンドモード
  ViewShadingMode viewShadingMode_ = ViewShadingMode::Solid; ///< シェーディング設定

  // マネージャー群
  ModelManager modelMan_;                          ///< モデル管理
  SpriteManager spriteMan_;                        ///< スプライト管理
  SkydomeManager skydomeMan_;                      ///< スカイドーム管理
  SkyboxManager skyboxMan_;                        ///< スカイボックス管理
  PrimitiveMeshManager primitiveMeshMan_;          ///< プリミティブメッシュ管理
  DirectionalLightManager dirLightMan_;            ///< 平行光源管理
  PointLightManager ptLightMan_;                   ///< 点光源管理
  SpotLightManager spLightMan_;                    ///< スポットライト管理
  AreaLightManager arLightMan_;                    ///< エリアライト管理
  TextureManager texMan_;                          ///< テクスチャ管理

  D3D12_GPU_DESCRIPTOR_HANDLE environmentMapSrv_{}; ///< 環境マップSRV

  // Camera CB
  struct CameraCB {
    Vector3 worldPos;
    float _pad = 0.0f;
  };
  Microsoft::WRL::ComPtr<ID3D12Resource> cameraCB_; ///< カメラCB
  CameraCB *cameraCBMapped_ = nullptr;              ///< カメラCBマップ済みポインタ

  // Fog CB
  struct FogOverlayCB {
    float timeSec = 0.0f;
    float intensity = 0.25f;
    float scale = 4.0f;
    float speed = 0.05f;
    Vector2 wind = {0.08f, 0.03f};
    float feather = 0.18f;
    float bottomBias = 0.35f;
    Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};
  };
  Microsoft::WRL::ComPtr<ID3D12Resource> fogCB_;   ///< フォグCB
  FogOverlayCB *fogCBMapped_ = nullptr;            ///< フォグCBマップ済みポインタ

  std::unique_ptr<Primitive2D> prim2D_;             ///< 2Dプリミティブ描画器
  std::unique_ptr<Primitive3D> prim3D_;             ///< 3Dプリミティブ描画器

  std::vector<RenderCommand3D> commandQueue3D_;    ///< 3D描画コマンドキュー
  std::vector<RenderCommandHistory> currentCommandHistory_; ///< 現在フレームの描画キュー履歴
  std::vector<RenderCommandHistory> lastCommandHistory_;    ///< デバッグ表示用の前フレーム描画キュー履歴

  std::array<FrameResource, FrameResource::kFrameCount> frameResources_; ///< フレーム別リソース
  uint32_t frameIndex_ = 0;                         ///< 現在のフレームリソースインデックス

  std::vector<std::future<void>> ongoingTasks_;    ///< 実行中の非同期タスク
  std::mutex mtxTasks_;                            ///< タスク管理用ミューテックス

  std::mutex mtxResource_;                         ///< リソース生成同期用ミューテックス

public:
  /// @brief リソース生成時などの排他制御用ミューテックスを取得する
  std::mutex &ResourceMutex() { return mtxResource_; }
};

/// @brief グローバルなRenderContextインスタンスを取得する（ヘルパー）
RenderContext &GetRenderContext();

} // namespace RC
