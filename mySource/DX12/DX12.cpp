#include "DX12.h"
#include "../Log/Log.h"
#include <format>

void DX12::Initialize() {

  Log logger;

  // DXGIファクトリーの生成
  hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
  // 初期化の根本的な部分でエラーが出た場合はプログラムが間違っているか、どうにもできない場合多いのでassertでエラーを出す
  assert(SUCCEEDED(hr));

  // 使用すうるアダプタ用の変数。最初にnullptrを入れておく
  IDXGIAdapter4 *useAdapter = nullptr;

  // 良い順にアダプタを頼む
  for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(
                       i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                       IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND;
       ++i) {
    // アダプタの情報を取得するための変数
    DXGI_ADAPTER_DESC3 adapterDesc{};
    hr = useAdapter->GetDesc3(&adapterDesc);
    assert(SUCCEEDED(hr)); // 取得できないのは一大事
    // ソフトウェアアダプタは除外
    if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
      // 採用したアダプタの情報をログに出力。wstringの方なので注意
      logger.WriteLog(logger.ConvertString(
          std::format(L"Use Adapater:{}\n", adapterDesc.Description)));
      break;
    }
    useAdapter = nullptr; // ソフトウェアアダプタは除外するのでnullptrに戻す
  }
  // アダプタが見つからなかった場合はエラー
  assert(useAdapter != nullptr);

  // 機能レベルとログの出力用の文字列
  D3D_FEATURE_LEVEL featureLevel[] = {
      D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0};

  const char *featureLevelStrings[] = {"12.2", "12.1", "12.0"};

  // 高い順に機能レベルを試す
  for (size_t i = 0; i < std::size(featureLevel); ++i) {
    // デバイスの生成
    hr = D3D12CreateDevice(useAdapter, featureLevel[i], IID_PPV_ARGS(&device));
    // 指定した機能レベルでデバイスが生成できた場合
    if (SUCCEEDED(hr)) {
      // 生成できたのでログ出力を行ってループを抜ける
      logger.WriteLog(
          std::format("FeatureLevel : {}\n", featureLevelStrings[i]));
      break;
    }
  }
  // デバイスが生成できなかった場合はエラー
  assert(device != nullptr);
  logger.WriteLog("Complete create D3D12Device!!!\n"); // 初期化完了のログ出力
}
