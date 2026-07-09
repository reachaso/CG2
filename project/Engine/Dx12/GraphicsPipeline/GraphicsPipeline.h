#pragma once
#include <cassert>
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>

/// @brief ブレンドモードの定義
enum BlendMode {
  kBlendModeNone,           ///< ブレンド無し
  kBlendModeNormal,         ///< 通常のαブレンド (Src * SrcA + Dest * (1 - SrcA))
  kBlendModeAdd,            ///< 加算 (Src * SrcA + Dest * 1)
  kBlendModeSubtract,       ///< 減算 (Dest * 1 - Src * SrcA)
  kBlendModeMultiply,       ///< 乗算 (Src * 0 + Dest * Src)
  kBlendModeScreen,         ///< スクリーン (Src * (1 - Dest) + Dest * 1)
  kBlendModePremultiplied,  ///< アルファ乗算済みブレンド (Src * 1 + Dest * (1 - SrcA))
};

/// @brief ルートシグネチャの種類
enum class RootSignatureType {
  Object3D,           ///< 3Dオブジェクト用
  Object3DInstancing, ///< 3Dオブジェクト（インスタンシング）用
  Object3DSkin,       ///< 3Dオブジェクト（スキニング）用
  Sprite,             ///< スプライト用

  FogOverlay,         ///< フォグオーバーレイ用
  Primitive3D,        ///< 3Dプリミティブ用
  PostProcess,        ///< ポストプロセス用
  SkinningCS,         ///< Compute Shader スキニング用
  GPUParticle,        ///< GPU Particle 描画用
  InitParticleCS,     ///< GPU Particle 初期化 CS 用
  UpdateParticleCS,   ///< GPU Particle 更新 CS 用
  EmitParticleCS,     ///< GPU Particle 射出 CS 用
  Water,              ///< 水面シェーダー用（Object3D + WaterParams b6）
  WaveSimulationCS,   ///< 波のシミュレーションCS用
};

/// @brief パイプライン構築時のオプション設定構造体
struct GPipelineOptions {
  bool enableAlphaBlend = false;      ///< アルファブレンドを有効にするか
  bool enableDepth = true;            ///< 深度テストを有効にするか
  bool enableDepthWrite = true;       ///< 深度書き込みを有効にするか
  D3D12_CULL_MODE cull = D3D12_CULL_MODE_BACK; ///< カリングモード
  D3D12_FILL_MODE fill = D3D12_FILL_MODE_SOLID; ///< フィルモード（ソリッド/ワイヤーフレーム）
  BlendMode blendMode = kBlendModeNormal;       ///< ブレンド方式
  RootSignatureType rootType = RootSignatureType::Object3D; ///< ルートシグネチャの形状
  D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType =
      D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; ///< トポロジの種類 (三角形, 線, 点)
  bool disableRTV = false; ///< レンダーターゲット出力を無効にするか（ShadowMap等）
  DXGI_FORMAT dsvFormatOverride = DXGI_FORMAT_UNKNOWN; ///< デフォルトのDSVフォーマットを上書きするか
};

/// @brief ルートシグネチャとパイプラインステートオブジェクト(PSO)を構築・管理するクラス
class GraphicsPipeline {
public:
  /// @brief 初期化
  /// @param device DirectX12デバイス
  void Init(ID3D12Device *device) { device_ = device; }

  /// @brief 終了処理
  void Term();

  /// @brief ルートシグネチャとPSOを構築する
  /// @param inputElems 入力レイアウト定義の配列
  /// @param elemCount 入力レイアウト定義の要素数
  /// @param vs 頂点シェーダーのバイナリ
  /// @param ps ピクセルシェーダーのバイナリ
  /// @param rtvFmt レンダーターゲットのフォーマット
  /// @param dsvFmt 深度ステンシルのフォーマット
  /// @param cull カリングモード
  /// @param fill フィルモード
  void Build(const D3D12_INPUT_ELEMENT_DESC *inputElems, UINT elemCount,
             D3D12_SHADER_BYTECODE vs, D3D12_SHADER_BYTECODE ps,
             DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt,
             D3D12_CULL_MODE cull = D3D12_CULL_MODE_BACK,
             D3D12_FILL_MODE fill = D3D12_FILL_MODE_SOLID);

  /// @brief ルートシグネチャとPSOを構築する（ID3DBlob 版）
  void Build(const D3D12_INPUT_ELEMENT_DESC *inputElems, UINT elemCount,
             ID3DBlob *vs, ID3DBlob *ps, DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt,
             D3D12_CULL_MODE cull = D3D12_CULL_MODE_BACK,
             D3D12_FILL_MODE fill = D3D12_FILL_MODE_SOLID);

  /// @brief ルートシグネチャとPSOを構築する（IDxcBlob 版）
  void Build(const D3D12_INPUT_ELEMENT_DESC *inputElems, UINT elemCount,
             struct IDxcBlob *vs, struct IDxcBlob *ps, DXGI_FORMAT rtvFmt,
             DXGI_FORMAT dsvFmt, D3D12_CULL_MODE cull = D3D12_CULL_MODE_BACK,
             D3D12_FILL_MODE fill = D3D12_FILL_MODE_SOLID);

  /// @brief 拡張オプション付きでPSOを構築する
  /// @param inputElems 入力レイアウト
  /// @param elemCount 入力レイアウト数
  /// @param vs VSバイナリ
  /// @param ps PSバイナリ
  /// @param rtvFmt RTVフォーマット
  /// @param dsvFmt DSVフォーマット
  /// @param opt 詳細オプション
  /// @param cachedPSO キャッシュされたPSOデータ（任意）
  void BuildEx(const D3D12_INPUT_ELEMENT_DESC *inputElems, UINT elemCount,
                D3D12_SHADER_BYTECODE vs, D3D12_SHADER_BYTECODE ps,
                DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt,
                const GPipelineOptions &opt,
                const D3D12_CACHED_PIPELINE_STATE &cachedPSO);

  /// @brief 生成したPSOからキャッシュ用のシリアライズされたバイナリを取得する
  /// @return シリアライズされたPSOデータ
  Microsoft::WRL::ComPtr<ID3DBlob> GetSerializedBlob() const;

  /// @brief ルートシグネチャを取得
  /// @return ID3D12RootSignature
  ID3D12RootSignature *Root() const { return root_.Get(); }

  /// @brief パイプラインステートオブジェクト(PSO)を取得
  /// @return ID3D12PipelineState
  ID3D12PipelineState *PSO() const { return pso_.Get(); }

  /// @brief キャッシュの不一致などで再構築（フォールバック）が発生したか
  bool IsCacheFallback() const { return isCacheFallback_; }

private:
  /// @brief ルートシグネチャを構築する内部関数
  void
  buildRootSignature_(RootSignatureType type = RootSignatureType::Object3D);

  /// @brief PSOを構築する内部関数
  void buildPSO_(const D3D12_INPUT_ELEMENT_DESC *inputElems, UINT elemCount,
                 D3D12_SHADER_BYTECODE vs, D3D12_SHADER_BYTECODE ps,
                 DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt, D3D12_CULL_MODE cull,
                 D3D12_FILL_MODE fill);

private:
  Microsoft::WRL::ComPtr<ID3D12Device> device_;      ///< デバイス
  Microsoft::WRL::ComPtr<ID3D12RootSignature> root_; ///< ルートシグネチャ
  Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;  ///< パイプラインステートオブジェクト
  bool isCacheFallback_ = false;                     ///< キャッシュフォールバック発生フラグ
};
