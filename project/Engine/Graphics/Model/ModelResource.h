#pragma once
#include "Math/Math.h"
#include "Math/MathTypes.h"
#include "ModelMesh.h"
#include "function/function.h"
#include "struct.h"
#include <d3d12.h>
#include <memory>
#include <vector>
#include <wrl/client.h>
#include "ComputeShader/ComputeShader.h"

class TextureManager; // 前方宣言
class SRVManager;     // 前方宣言
class PipelineManager; // 前方宣言

namespace RC {
class FrameResource; // 前方宣言
}

// ============================================================
// ModelResource
// - GPU リソース（CB, SRV, バッファ）と描画ロジックを管理する
// - ModelObject から分離された「GPU側のデータ」を担当
// - 1つの ModelMesh (共有) をバインドし、Draw/DrawBatch を実行する
//
// ★ 毎フレームの WVP CB / InstanceData は FrameResource (リニア
//    アロケータ) から確保するため、CPU/GPU 間のレースコンディション
//    が発生しない。
// ============================================================
/// @class ModelResource
/// @brief GPU リソース（定数バッファ、SRV）とモデルの描画ロジックを管理するクラス
/// @details ModelObject から実体データを分離したもので、頂点バッファのバインドやシェーダーへの定数転送、
/// サブメッシュごとの描画実行などを担当します。
/// インスタンスデータやWVP行列の転送には FrameResource（リニアアロケータ）を利用します。
class ModelResource {
public:
  ModelResource() = default;
  ~ModelResource();

  /// @brief GPU リソースの初期化
  /// @param device D3D12 デバイス
  void Initialize(ID3D12Device *device);

  /// @brief 使用する共有メッシュを設定する
  /// @param mesh ModelMesh の shared_ptr
  void SetMesh(const std::shared_ptr<ModelMesh> &mesh);

  /// @brief 設定されているメッシュを取得する
  /// @return ModelMesh の shared_ptr
  const std::shared_ptr<ModelMesh> &GetMesh() const { return mesh_; }

  /// @brief 全サブメッシュ共通で適用するオーバーライドテクスチャを設定する
  /// @param srvGPUHandle テクスチャの GPU ハンドル
  void SetTexture(D3D12_GPU_DESCRIPTOR_HANDLE srvGPUHandle) {
    textureSrv_ = srvGPUHandle;
  }

  /// @brief テクスチャのオーバーライドを解除し、マテリアル固有のテクスチャに戻す
  void ResetTextureToMtl();

  /// @brief テクスチャマネージャを設定する
  /// @param tm テクスチャマネージャへのポインタ
  void SetTextureManager(TextureManager *tm) { texman_ = tm; }

  /// @brief マテリアルの定数バッファ（CPUマップ済み）を取得する
  /// @return Material 構造体へのポインタ
  Material *Mat() { return cbMat_.mapped; }

  /// @brief ライトの定数バッファ（CPUマップ済み）を取得する
  /// @return DirectionalLight 構造体へのポインタ
  DirectionalLight *Light() { return cbLight_.mapped; }

  /// @brief 外部で管理されているライトCBのアドレスを設定する
  /// @param addr ライトCBの GPU 仮想アドレス
  void SetExternalLightCBAddress(D3D12_GPU_VIRTUAL_ADDRESS addr) {
    externalLightCBAddress_ = addr;
  }

  /// @brief 外部ライトCBのアドレスを取得する
  /// @return アドレス値
  D3D12_GPU_VIRTUAL_ADDRESS GetExternalLightCBAddress() const {
    return externalLightCBAddress_;
  }

  /// @brief ライティングの初期パラメータを適用する
  /// @param lightingMode ライティングモード
  /// @param color ライトカラー（RGB）
  /// @param dir ライト方向（XYZ）
  /// @param intensity ライト強度
  void ApplyLighting(int lightingMode, const float color[3],
                     const float dir[3], float intensity);

  /// @brief ワールド行列を指定してモデルを描画する
  /// @param cmdList コマンドリスト
  /// @param world ワールド行列
  /// @param view ビュー行列
  /// @param proj プロジェクション行列
  /// @param frame フレームリソース（動的CB確保用）
  void Draw(ID3D12GraphicsCommandList *cmdList, const RC::Matrix4x4 &world,
            const RC::Matrix4x4 &view, const RC::Matrix4x4 &proj,
            RC::FrameResource &frame);

  /// @brief インスタンシングによる一括描画を実行する
  /// @param cmdList コマンドリスト
  /// @param view ビュー行列
  /// @param proj プロジェクション行列
  /// @param instances インスタンスごとのトランスフォームリスト
  /// @param frame フレームリソース
  void DrawBatch(ID3D12GraphicsCommandList *cmdList, const RC::Matrix4x4 &view,
                 const RC::Matrix4x4 &proj,
                 const std::vector<Transform> &instances,
                 RC::FrameResource &frame);

  /// @brief インスタンシング描画（単色指定付き）
  /// @param cmdList コマンドリスト
  /// @param view ビュー行列
  /// @param proj プロジェクション行列
  /// @param instances インスタンスごとのトランスフォームリスト
  /// @param color 全インスタンスに適用するオーバーライドカラー
  /// @param frame フレームリソース
  void DrawBatch(ID3D12GraphicsCommandList *cmdList, const RC::Matrix4x4 &view,
                 const RC::Matrix4x4 &proj,
                 const std::vector<Transform> &instances,
                 const RC::Vector4 &color,
                 RC::FrameResource &frame);

  /// @brief スキニング付きモデルを描画する
  /// @param cmdList コマンドリスト
  /// @param world ワールド行列
  /// @param view ビュー行列
  /// @param proj プロジェクション行列
  /// @param skinMatrices スキニング行列パレット
  /// @param frame フレームリソース
  void DrawSkinned(ID3D12GraphicsCommandList *cmdList,
                   const RC::Matrix4x4 &world,
                   const RC::Matrix4x4 &view, const RC::Matrix4x4 &proj,
                   const std::vector<RC::Matrix4x4> &skinMatrices,
                   RC::FrameResource &frame);

  /// @brief バッチ描画用のカーソルをリセットする
  /// @note FrameResource 移行後は内部で管理されるため、現在は実質的な処理を行いません。
  void ResetBatchCursor() { /* FrameResource 側で管理 */ }

  /// @brief 準備完了状態（メッシュと初期リソースが有効か）を取得する
  /// @return 準備完了なら true
  bool IsReady() const { return isReady_; }

  /// @brief 準備完了状態を設定する
  /// @param r 状態
  void SetReady(bool r) { isReady_ = r; }

  // === CS スキニング ===

  /// @brief CS スキニングをセットアップする
  /// @param device D3D12 デバイス
  /// @param pm PipelineManager
  /// @param srvMgr SRVマネージャ
  void SetSkinningCS(ID3D12Device *device, PipelineManager *pm,
                     SRVManager *srvMgr);

  /// @brief CS スキニングを実行する (Dispatch)
  /// @param cmdList コマンドリスト
  /// @param skinMatrices スキニング行列パレット
  /// @param frame フレームリソース
  void DispatchSkinning(ID3D12GraphicsCommandList *cmdList,
                        const std::vector<RC::Matrix4x4> &skinMatrices,
                        RC::FrameResource &frame);

  /// @brief CS スキニングが有効か
  bool HasCSSkinning() const { return skinningCS_.IsReady(); }

  /// @brief CS スキニング済み VBV を取得する
  const D3D12_VERTEX_BUFFER_VIEW &GetSkinnedVBV() const { return skinnedVBV_; }

  /// @brief CS スキニングが完了しているか (Dispatch済み)
  bool IsSkinningDispatched() const { return skinningDispatched_; }

  /// @brief CS スキニングの Dispatch フラグをリセットする
  void ResetSkinningDispatched() { skinningDispatched_ = false; }

  /// @brief CS スキニング済み頂点を使って描画する（通常の object3d パイプラインで）
  /// @param cmdList コマンドリスト
  /// @param world ワールド行列
  /// @param view ビュー行列
  /// @param proj プロジェクション行列
  /// @param frame フレームリソース
  void DrawSkinnedCS(ID3D12GraphicsCommandList *cmdList,
                     const RC::Matrix4x4 &world, const RC::Matrix4x4 &view,
                     const RC::Matrix4x4 &proj, RC::FrameResource &frame);

private:
  /// @struct CB_Material
  /// @brief マテリアル定数バッファのリソースとポインタを保持する内部構造体
  struct CB_Material {
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    Material *mapped = nullptr;
  };

  /// @struct CB_Light
  /// @brief ライト定数バッファのリソースとポインタを保持する内部構造体
  struct CB_Light {
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    DirectionalLight *mapped = nullptr;
  };

  /// @struct InstanceDataGPU
  /// @brief シェーダーに送るインスタンスごとのデータ構造
  struct InstanceDataGPU {
    RC::Matrix4x4 WVP;                   ///< ワールド・ビュー・プロジェクション行列
    RC::Matrix4x4 World;                 ///< ワールド行列
    RC::Matrix4x4 WorldInverseTranspose; ///< ワールド逆転置行列（法線用）
    RC::Vector4 color;                   ///< インスタンスカラー
  };

  /// @brief メッシュに含まれる全マテリアルのテクスチャがロードされていることを保証する
  void EnsureMaterialSrvsLoaded_();

  /// @brief 指定されたマテリアルインデックスに対応する SRV ハンドルを取得する
  /// @param materialIndex マテリアルインデックス
  /// @return GPU 記述子ハンドル
  D3D12_GPU_DESCRIPTOR_HANDLE GetSrvForMaterial_(uint32_t materialIndex) const;

  Microsoft::WRL::ComPtr<ID3D12Device> device_;
  std::shared_ptr<ModelMesh> mesh_;

  CB_Material cbMat_{};
  CB_Light cbLight_{};

  D3D12_GPU_DESCRIPTOR_HANDLE textureSrv_{}; ///< オーバーライド用 SRV（共通テクスチャ）

  std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> materialSrvs_; ///< マテリアルごとの SRV リスト

  D3D12_GPU_VIRTUAL_ADDRESS externalLightCBAddress_ = 0; ///< 外部ライトCBのアドレス

  TextureManager *texman_ = nullptr;
  bool isReady_ = false;

  // === CS スキニング用 ===
  ComputeShader skinningCS_;               ///< CS スキニング用ラッパ
  SRVManager *srvMgr_ = nullptr;               ///< SRVマネージャ

  Microsoft::WRL::ComPtr<ID3D12Resource> skinnedVertexBuffer_;  ///< スキニング済み頂点バッファ (UAV)
  D3D12_VERTEX_BUFFER_VIEW skinnedVBV_{};      ///< スキニング済み VBV
  D3D12_GPU_DESCRIPTOR_HANDLE skinnedUAVHandle_{}; ///< UAV ディスクリプタハンドル
  uint32_t skinnedVertexCount_ = 0;            ///< スキニング対象頂点数
  bool skinningResourcesReady_ = false;        ///< CS リソースが初期化済みか
  bool skinningDispatched_ = false;            ///< 今フレーム Dispatch 済みか

  Microsoft::WRL::ComPtr<ID3D12Resource> skinningInfoCB_;  ///< SkinningInfo CB
  uint32_t *skinningInfoMapped_ = nullptr;     ///< CB のマップ済みポインタ

  /// @brief CS スキニング用リソースを初期化する
  void InitSkinningResources_();
};
