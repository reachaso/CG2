#pragma once
#include "GraphicsPipeline/GraphicsPipeline.h"
#include "ShaderCompiler/ShaderCompiler.h"

#include <d3d12.h>
#include <dxcapi.h>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <filesystem>

/// @brief 入力レイアウトの事前定義タイプ
enum class InputLayoutType {
  Object3D,      ///< 標準的な3Dオブジェクト (Pos, Normal, UV)
  Object3DSkin,  ///< スキニング付き3Dオブジェクト (Pos, UV, Normal, BlendIndices, BlendWeight)
  Sprite,        ///< 2Dスプライト (Pos, UV, Color)
  Particle,      ///< パーティクル (Pos, Scale, Color)
  Primitive3D,   ///< 3Dプリミティブ (Pos, Color)
  None,          ///< 入力レイアウトなし（SV_VertexID 等を使用する場合）
};

/// @brief パイプライン作成に必要な情報をまとめる構造体
struct PipelineDesc {
  // Shader files
  std::wstring vsPath; ///< 頂点シェーダーのファイルパス
  std::wstring psPath; ///< ピクセルシェーダーのファイルパス

  // Compile settings
  std::wstring vsEntry = L"main"; ///< 頂点シェーダーのエントリポイント
  std::wstring psEntry = L"main"; ///< ピクセルシェーダーのエントリポイント
  std::wstring vsTarget = L"vs_6_0"; ///< 頂点シェーダーのターゲットプロファイル
  std::wstring psTarget = L"ps_6_0"; ///< ピクセルシェーダーのターゲットプロファイル
  bool optimize = true;  ///< 最適化を有効にするか
  bool debugInfo = false; ///< デバッグ情報を付与するか

  // InputLayout
  std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout; ///< 入力レイアウト定義（実体保持）

  // Root/Blend/Depth/Raster など
  GPipelineOptions opt{}; ///< パイプラインの詳細設定
};

/// @brief GraphicsPipeline の生成・登録・管理・再構築を行うマネージャクラス
/// シェーダーのコンパイル結果やPSOのキャッシュ管理も担当します。
class PipelineManager {
public:
  /// @brief 初期化
  /// @param device DirectX12デバイス
  /// @param rtvFmt デフォルトのレンダーターゲットフォーマット
  /// @param dsvFmt デフォルトの深度ステンシルフォーマット
  void Init(ID3D12Device *device, DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt);

  /// @brief 終了処理
  void Term();

  /// @brief 指定したキー名でパイプラインを作成・登録する
  /// @param key 登録名
  /// @param desc パイプライン設定
  /// @return 生成されたGraphicsPipelineへのポインタ
  GraphicsPipeline *Create(const std::string &key, const PipelineDesc &desc);

  /// @brief 簡易的なファイル指定によるパイプラインの作成・登録
  /// @param key 登録名
  /// @param vsPath 頂点シェーダーパス
  /// @param psPath ピクセルシェーダーパス
  /// @param layoutType 事前定義の入力レイアウトタイプ
  /// @param opt パイプライン設定
  /// @return 生成されたGraphicsPipelineへのポインタ
  GraphicsPipeline *CreateFromFiles(const std::string &key,
                                    const std::wstring &vsPath,
                                    const std::wstring &psPath,
                                    InputLayoutType layoutType,
                                    const GPipelineOptions &opt);

  /// @brief 登録済みのパイプラインを取得
  /// @param key 登録名
  /// @return 見つかった場合はGraphicsPipelineへのポインタ、なければnullptr
  GraphicsPipeline *Get(const std::string &key);

  /// @brief 指定したパイプラインを再ビルドする（シェーダーのホットリロード等）
  /// @param key 登録名
  /// @return 成功すればtrue
  bool Rebuild(const std::string &key);

  /// @brief 登録済みのすべてのパイプラインを再ビルドする
  void RebuildAll();

  /// @brief エンジン既定のパイプライン群を一括登録する
  void RegisterDefaultPipelines();

  /// @brief パイプラインキャッシュファイルをロードする
  /// @param filePath キャッシュファイルのパス
  void LoadCache(const std::string &filePath);

  /// @brief パイプラインキャッシュファイルを保存する
  /// @param filePath キャッシュファイルのパス
  void SaveCache(const std::string &filePath);

  /// @brief Compute Pipeline を作成・登録する
  /// @param key 登録名
  /// @param csPath Compute Shader ファイルパス
  /// @param rootType ルートシグネチャの種類
  void CreateCompute(const std::string &key, const std::wstring &csPath,
                     RootSignatureType rootType);

  /// @brief 登録済みの Compute PSO を取得する
  /// @param key 登録名
  /// @return PSO ポインタ (見つからなければ nullptr)
  ID3D12PipelineState *GetComputePSO(const std::string &key);

  /// @brief 登録済みの Compute Root Signature を取得する
  /// @param key 登録名
  /// @return RootSignature ポインタ (見つからなければ nullptr)
  ID3D12RootSignature *GetComputeRoot(const std::string &key);

  /// @brief 接頭辞とブレンドモードを組み合わせて一意なキーを生成するヘルパー関数
  /// @param prefix 接頭辞 (例: "Object3D")
  /// @param mode ブレンドモード
  /// @return 合成されたキー文字列
  static std::string MakeKey(std::string_view prefix, BlendMode mode);

private:
  /// @brief 入力レイアウト定義を生成する内部関数
  static std::vector<D3D12_INPUT_ELEMENT_DESC>
  MakeInputLayout(InputLayoutType type);

  /// @brief バイナリデータからパイプラインを構築する内部関数
  GraphicsPipeline *createFromBlobs_(const std::string &key,
                                     const PipelineDesc &desc, IDxcBlob *vs,
                                     IDxcBlob *ps,
                                     const D3D12_CACHED_PIPELINE_STATE &cachedPSO);

  /// @brief PSOおよびシェーダーのキャッシュ情報
  struct PsoCache {
    std::vector<uint8_t> psoData; ///< PSOキャッシュバイナリ
    std::vector<uint8_t> vsData;  ///< VSバイナリ
    std::vector<uint8_t> psData;  ///< PSバイナリ
    int64_t vsTime = 0; ///< VSファイルの最終更新日時
    int64_t psTime = 0; ///< PSファイルの最終更新日時
  };

  /// @brief パイプライン設定から一意なキャッシュキーを生成する
  static std::string makePsoCacheKey_(const std::string &key, const PipelineDesc &desc);

private:
  /// @brief 登録済みパイプラインのエントリ
  struct Entry {
    PipelineDesc desc; ///< 作成時の設定
    std::unique_ptr<GraphicsPipeline> pipeline; ///< パイプライン本体
  };

  /// @brief Compute Pipeline のエントリ
  struct ComputeEntry {
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root; ///< ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;  ///< PSO
  };

  ID3D12Device *device_ = nullptr; ///< デバイス（非所有）
  DXGI_FORMAT rtvFmt_{DXGI_FORMAT_R8G8B8A8_UNORM_SRGB}; ///< デフォルトRTVフォーマット
  DXGI_FORMAT dsvFmt_{DXGI_FORMAT_D24_UNORM_S8_UINT};   ///< デフォルトDSVフォーマット

  ShaderCompiler compiler_; ///< シェーダーコンパイラ
  std::unordered_map<std::string, Entry> pipelines_; ///< 登録済みパイプラインマップ
  std::unordered_map<std::string, ComputeEntry> computePipelines_; ///< Compute パイプライン
  std::unordered_map<std::string, PsoCache> psoCache_; ///< PSOキャッシュマップ
  bool cacheUpdated_ = false; ///< キャッシュが更新されたかどうかのフラグ
};
