#pragma once

// ============================================================================
// RenderQueue
// ----------------------------------------------------------------------------
// 描画コマンドをソートキーで並べ替えてから一括実行する仕組み。
// 同じ PSO / テクスチャのコマンドが連続するようにソートし、
// ステート変更回数を最小化する。
//
// 既存の PushCommand3D（ラムダ方式）と並行運用可能。
// RenderModel 系の描画のみ段階的に RenderQueue に移行する。
// ============================================================================

#include <cstdint>
#include <d3d12.h>
#include <functional>
#include <string_view>
#include <vector>

#include "Math/MathTypes.h"
#include "GraphicsPipeline/GraphicsPipeline.h" // BlendMode

namespace RC {

class RenderContext; // 前方宣言

// ── ソートキー構成（64bit）──
// [63..56] レイヤー：不透明=0, 半透明=1, ガラス=2
// [55..40] PSO ID（パイプライン種別のハッシュ）
// [39..24] テクスチャ ID（SRV ptr の下位ビット）
// [23..0]  深度（front-to-back / back-to-front）
/// @namespace RC::SortKey
/// @brief 描画順序を決定する64bitソートキーの構築用名前空間
/// キー構成: [63..56] レイヤー, [55..40] PSOハッシュ, [39..24] テクスチャハッシュ, [23..0] 深度
namespace SortKey {

/// @brief 描画レイヤーの定義
enum Layer : uint8_t {
  kLayerOpaque = 0,      ///< 不透明
  kLayerTranslucent = 1, ///< 半透明
  kLayerGlass = 2,       ///< ガラス・加算等
  kLayerOverlay = 3,     ///< オーバーレイ（ギズモなど常に最後に描画）
};

/// @brief PSOプレフィックス文字列から16bitハッシュを生成する
/// @param prefix PSOの識別文字列
/// @return 16bitハッシュ値
inline uint16_t HashPSO(std::string_view prefix) {
  // FNV-1a 32bit → 上位16bitと下位16bitを XOR fold
  uint32_t h = 2166136261u;
  for (char c : prefix) {
    h ^= static_cast<uint32_t>(c);
    h *= 16777619u;
  }
  return static_cast<uint16_t>((h >> 16) ^ (h & 0xFFFF));
}

/// @brief テクスチャのGPUハンドルから16bitハッシュを生成する
/// @param srv GPUディスクリプタハンドル
/// @return 16bitハッシュ値
inline uint16_t HashTexture(D3D12_GPU_DESCRIPTOR_HANDLE srv) {
  // ptr の下位ビットをXOR fold
  uint64_t v = srv.ptr;
  return static_cast<uint16_t>((v >> 16) ^ (v & 0xFFFF));
}

/// @brief 各要素を組み合わせて64bitソートキーを生成する
/// @param layer 描画レイヤー
/// @param psoHash PSOハッシュ
/// @param texHash テクスチャハッシュ
/// @param depth24 24bit精度の深度値（フロントトゥバック等の制御用）
/// @return 64bitソートキー
inline uint64_t Make(Layer layer, uint16_t psoHash, uint16_t texHash,
                     uint32_t depth24 = 0) {
  return (uint64_t(layer) << 56) | (uint64_t(psoHash) << 40) |
         (uint64_t(texHash) << 24) | uint64_t(depth24 & 0x00FFFFFF);
}

} // namespace SortKey

/// @brief ソート可能な描画単位（パケット）構造体
/// 実行時にラムダを呼ぶ方式で、ステート変更のグルーピングを実現します。
struct RenderPacket {
  uint64_t sortKey = 0; ///< ソートキー（SortKey::Makeで生成）

  // PSO binding 情報（同じ PSO なら切替をスキップするためのヒント）
  std::string_view psoPrefix; ///< パイプラインのプレフィックス ("object3d", "object3d_inst" 等)
  BlendMode blend = kBlendModeNone; ///< ブレンドモード

  /// @brief 描画実行コールバック
  /// PSO/Camera/Lights バインド完了後に呼ばれます。
  std::function<void(ID3D12GraphicsCommandList *)> execute;
};

/// @brief 描画コマンドをソートキーで並べ替えてから一括実行する仕組み
/// 同じ PSO / テクスチャのコマンドが連続するようにソートし、ステート変更回数を最小化します。
class RenderQueue {
public:
  /// @brief パケットをキューに追加
  /// @param packet 描画パケット
  void Push(RenderPacket &&packet) {
    packets_.push_back(std::move(packet));
  }

  /// @brief ソートキーでパケットをソート
  void Sort();

  /// @brief 全パケットを実行（PSO の重複切替を最小化）
  /// @param ctx 描画コンテキスト
  /// @param cl コマンドリスト
  void Execute(RenderContext &ctx, ID3D12GraphicsCommandList *cl);

  /// @brief キューをクリア
  void Clear() { packets_.clear(); }

  /// @brief パケット数
  /// @return パケット数
  size_t Size() const { return packets_.size(); }

  /// @brief 空かどうか
  /// @return 空なら true
  bool Empty() const { return packets_.empty(); }

private:
  std::vector<RenderPacket> packets_; ///< 蓄積されたパケット
};

} // namespace RC
