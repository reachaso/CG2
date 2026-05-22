#pragma once
#include <cassert>
#include <cstdint>
#include <d3d12.h>
#include <vector>

class DescriptorHeap;

/// @brief SRV（Shader Resource View）のデスクリプタスロットを管理するクラス
/// デスクリプタヒープ内の空きスロットの確保、解放、および各種SRVの生成をラップします。
class SRVManager {
public:
  /// @brief SRVハンドルの管理構造体
  struct Handle {
    UINT index = UINT_MAX;              ///< ヒープ内のインデックス
    D3D12_CPU_DESCRIPTOR_HANDLE cpu{};  ///< CPU記述子ハンドル
    D3D12_GPU_DESCRIPTOR_HANDLE gpu{};  ///< GPU記述子ハンドル
    
    /// @brief 有効なハンドルかどうか
    /// @return 有効ならtrue
    bool IsValid() const { return index != UINT_MAX; }
  };

  /// @brief 初期化
  /// @param device DirectX12デバイス
  /// @param srvHeap 管理対象のデスクリプタヒープ (ShaderVisibleなCBV/SRV/UAVヒープ)
  void Init(ID3D12Device *device, DescriptorHeap *srvHeap);

  /// @brief 終了処理
  void Term();

  /// @brief デバイスを取得
  /// @return ID3D12Deviceへのポインタ
  ID3D12Device *Device() const { return device_; }

  /// @brief デスクリプタヒープを取得
  /// @return DescriptorHeapへのポインタ
  DescriptorHeap *Heap() const { return srvHeap_; }

  /// @brief 空きスロットを1つ確保する
  /// @return 確保されたハンドル
  Handle Allocate();

  /// @brief スロットを解放して再利用可能にする
  /// @note GPUが該当スロットの参照を完全に終えている必要があります。
  /// @param h 解放するハンドル
  void Free(const Handle &h);

  /// @brief 2Dテクスチャ用のSRVを作成する
  /// @param res 対象リソース
  /// @param fmt フォーマット
  /// @param mipLevels ミップマップ数
  /// @param mostDetailedMip 最も詳細なミップレベル
  /// @return 作成されたSRVのハンドル
  Handle CreateTexture2D(ID3D12Resource *res, DXGI_FORMAT fmt, UINT mipLevels,
                         UINT mostDetailedMip = 0);

  /// @brief キューブマップ用のSRVを作成する
  /// @param res 対象リソース
  /// @param fmt フォーマット
  /// @param mipLevels ミップマップ数
  /// @param mostDetailedMip 最も詳細なミップレベル
  /// @return 作成されたSRVのハンドル
  Handle CreateTextureCube(ID3D12Resource *res, DXGI_FORMAT fmt, UINT mipLevels,
                           UINT mostDetailedMip = 0);

  /// @brief ストラクチャードバッファ用のSRVを作成する
  /// @param res 対象リソース
  /// @param elementCount 要素数
  /// @param strideBytes 1要素あたりのバイトサイズ
  /// @return 作成されたSRVのハンドル
  Handle CreateStructuredBuffer(ID3D12Resource *res, UINT elementCount,
                                UINT strideBytes);

  /// @brief ストラクチャードバッファ用のUAVを作成する
  /// @param res 対象リソース（D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS必須）
  /// @param elementCount 要素数
  /// @param strideBytes 1要素あたりのバイトサイズ
  /// @return 作成されたUAVのハンドル
  Handle CreateStructuredBufferUAV(ID3D12Resource *res, UINT elementCount,
                                   UINT strideBytes);

private:
  ID3D12Device *device_ = nullptr; ///< デバイス（非所有）
  DescriptorHeap *srvHeap_ = nullptr; ///< デスクリプタヒープ（非所有）

  std::vector<UINT> free_;    ///< 再利用用の空きインデックスリスト
  std::vector<uint8_t> used_; ///< 使用状況管理フラグ（デバッグ・整合性チェック用）
};
