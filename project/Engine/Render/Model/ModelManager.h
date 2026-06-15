#pragma once

// ModelManager
// -----------------------------------------------------------------------------
// 「モデル周り」の管理クラス
// - モデルはハンドル制（int）
// - 同じ .obj は Mesh を共有（weak_ptr キャッシュ）
// - ModelObject 生成時に TextureManager を注入して mtl テクスチャも維持
// -----------------------------------------------------------------------------

#include <d3d12.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <future>

#include "struct.h"

class ModelObject;
class ModelMesh;

class TextureManager;   // 前方宣言
class SRVManager;       // 前方宣言
class PipelineManager;  // 前方宣言

namespace RC {

class FrameResource; // 前方宣言

/// @brief モデルリソースと描画オブジェクトを一括管理するマネージャクラス
/// モデルをハンドル(int)で管理し、同一ファイルのメッシュ共有や非同期ロードをサポートします。
class ModelManager {
public:
  /// @brief 初期化処理
  /// @param device DirectX12デバイス
  /// @param texman テクスチャマネージャ（マテリアル用テクスチャのロードに使用）
  void Init(ID3D12Device *device, TextureManager *texman);

  /// @brief CS スキニング用のパイプラインを設定する
  /// @param pm PipelineManager
  /// @param srvMgr SRVマネージャ
  void SetSkinningCS(PipelineManager *pm, SRVManager *srvMgr);

  /// @brief デストラクタ
  ~ModelManager();

  /// @brief 終了処理。管理している全モデルとメッシュキャッシュを解放します。
  void Term();

  /// @brief モデルファイルをロードしてハンドルを返す
  /// すでにロード済みのメッシュがある場合はキャッシュを再利用します。
  /// @param path モデルファイルのパス (.obj など)
  /// @return モデルハンドル（失敗時は -1）
  int Load(const std::string &path);

  /// @brief 指定したハンドルに対応するモデルを解放する
  /// @param handle モデルハンドル
  void Unload(int handle);

  /// @brief 有効なハンドルか確認する
  /// @param handle モデルハンドル
  /// @return 有効なら true
  bool IsValid(int handle) const;

  /// @brief ハンドルからモデルオブジェクトの実体を取得する
  /// @param handle モデルハンドル
  /// @return ModelObjectへのポインタ。無効なハンドルの場合は nullptr
  ::ModelObject *Get(int handle);
  
  /// @brief ハンドルからモデルオブジェクトの実体を取得する (const)
  /// @param handle モデルハンドル
  /// @return ModelObjectへのconstポインタ
  const ::ModelObject *Get(int handle) const;

  /// @brief ハンドルからモデルのトランスフォーム情報ポインタを取得する
  /// @param handle モデルハンドル
  /// @return Transform構造体へのポインタ
  Transform *GetTransformPtr(int handle);

  /// @brief モデルの乗算カラーを設定する
  /// @param handle モデルハンドル
  /// @param color 乗算するRGBAカラー
  void SetColor(int handle, const Vector4 &color);

  /// @brief モデルのライティングモードを設定する
  /// @param handle モデルハンドル
  /// @param m ライティングモード (Phong, Lambert, なし等)
  void SetLightingMode(int handle, LightingMode m);

  /// @brief 指定したモデルハンドルのメッシュを別のファイルで差し替える
  /// @param handle モデルハンドル
  /// @param path 新しいモデルファイルのパス
  void SetMesh(int handle, const std::string &path);

  /// @brief バッチ描画用の内部カーソルをリセットする
  /// 同一モデルを複数箇所で描画する際のインスタンスカウント管理に使用します。
  /// @param handle モデルハンドル
  void ResetCursor(int handle);

  /// @brief アニメーションをアタッチする（モデルに紐づけられたファイルパスを使用）
  /// @param handle モデルハンドル
  void AttachAnimation(int handle);

  /// @brief アニメーションを別ファイルからアタッチする
  /// @param handle モデルハンドル
  /// @param filePath アニメーションファイルのパス
  void AttachAnimation(int handle, const std::string& filePath);

  /// @brief モデルのアニメーション状態を更新する
  /// @param handle モデルハンドル
  /// @param dt 経過時間
  void UpdateAnimation(int handle, float dt);

  /// @brief 管理している全モデルのバッチカーソルをリセットする
  /// 毎フレームの描画開始前に呼び出すことを想定しています。
  void ResetAllBatchCursors();

  /// @brief 管理している全モデルのうち、CSスキニングが有効かつ未実行のものを一括でDispatchする
  /// @param cl コマンドリスト
  /// @param frame フレームリソース
  void DispatchAllSkinning(ID3D12GraphicsCommandList* cl, RC::FrameResource& frame);

private:
  /// @brief モデルオブジェクト保持用スロット
  struct Slot {
    std::unique_ptr<::ModelObject> ptr; ///< モデルオブジェクト実体
    bool inUse = false;                ///< 使用中フラグ
  };

  /// @brief 未使用スロットを検索・確保する
  int AllocSlot_();

  /// @brief メッシュをキャッシュから取得するか、新規にロードする
  /// @param path メッシュファイルのパス
  /// @return 共有メッシュオブジェクト
  std::shared_ptr<::ModelMesh> GetOrLoadMesh_(const std::string &path);

private:
  ID3D12Device *device_ = nullptr;     ///< デバイス
  TextureManager *texman_ = nullptr;   ///< テクスチャマネージャ

  std::vector<Slot> models_;           ///< モデルオブジェクトのリスト
  std::unordered_map<std::string, std::weak_ptr<::ModelMesh>> meshCache_; ///< メッシュの共有キャッシュ

  mutable std::recursive_mutex mtx_;   ///< 排他制御用ミューテックス
  std::unordered_map<std::string, std::shared_future<void>> loadingTasks_; ///< ロードタスクの管理

  // CS スキニング用
  PipelineManager *pipelineMgr_ = nullptr;
  SRVManager *srvMgr_ = nullptr;
};

} // namespace RC
