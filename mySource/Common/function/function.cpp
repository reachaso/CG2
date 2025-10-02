#include "function.h"
#include "Log/Log.h"
#include "vector"
#include <cassert>
#include <filesystem>
#include <fstream>

extern Log logger;

ID3D12Resource *CreateBufferResource(ID3D12Device *device, size_t sizeInBytes) {
  D3D12_HEAP_PROPERTIES heapProperties{};
  heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

  D3D12_RESOURCE_DESC bufferDesc{};
  bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  bufferDesc.Width = sizeInBytes;
  bufferDesc.Height = 1;
  bufferDesc.DepthOrArraySize = 1;
  bufferDesc.MipLevels = 1;
  bufferDesc.SampleDesc.Count = 1;
  bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  ID3D12Resource *buffer = nullptr;
  HRESULT hr = device->CreateCommittedResource(
      &heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer));
  assert(SUCCEEDED(hr));
  return buffer;
}

ID3D12Resource *CreateTextureResource(ID3D12Device *device,
                                      const DirectX::TexMetadata &metadata) {
  D3D12_RESOURCE_DESC resourceDesc{};
  resourceDesc.Width = UINT(metadata.width);
  resourceDesc.Height = UINT(metadata.height);
  resourceDesc.MipLevels = UINT16(metadata.mipLevels);
  resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);
  resourceDesc.Format = metadata.format;
  resourceDesc.SampleDesc.Count = 1;
  resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

  D3D12_HEAP_PROPERTIES heapProperties{};
  heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM;
  heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
  heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

  ID3D12Resource *resource = nullptr;
  HRESULT hr = device->CreateCommittedResource(
      &heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));
  assert(SUCCEEDED(hr));
  return resource;
}

ID3D12Resource *CreateDepthStencilTextureResource(ID3D12Device *device,
                                                  int32_t width,
                                                  int32_t height) {
  D3D12_RESOURCE_DESC resourceDesc{};
  resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  resourceDesc.Width = width;
  resourceDesc.Height = height;
  resourceDesc.DepthOrArraySize = 1;
  resourceDesc.MipLevels = 1;
  resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  resourceDesc.SampleDesc.Count = 1;
  resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

  D3D12_HEAP_PROPERTIES heapProperties{};
  heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

  D3D12_CLEAR_VALUE depthClearValue{};
  depthClearValue.DepthStencil.Depth = 1.0f;
  depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

  ID3D12Resource *resource = nullptr;
  HRESULT hr = device->CreateCommittedResource(
      &heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
      D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClearValue,
      IID_PPV_ARGS(&resource));
  assert(SUCCEEDED(hr));
  return resource;
}

ID3D12DescriptorHeap *CreateDescriptorHeap(ID3D12Device *device,
                                           D3D12_DESCRIPTOR_HEAP_TYPE heapType,
                                           UINT numDescriptors,
                                           bool shaderVisible) {
  ID3D12DescriptorHeap *descriptorHeap = nullptr;
  D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
  descriptorHeapDesc.Type = heapType;
  descriptorHeapDesc.NumDescriptors = numDescriptors;
  descriptorHeapDesc.Flags = shaderVisible
                                 ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                                 : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc,
                                            IID_PPV_ARGS(&descriptorHeap));
  assert(SUCCEEDED(hr));
  return descriptorHeap;
}

void UploadTextureData(ID3D12Resource *texture,
                       const DirectX::ScratchImage &mipImages) {
  const DirectX::TexMetadata &metadata = mipImages.GetMetadata();
  for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel) {
    const DirectX::Image *img = mipImages.GetImage(mipLevel, 0, 0);
    HRESULT hr =
        texture->WriteToSubresource(UINT(mipLevel), nullptr, img->pixels,
                                    UINT(img->rowPitch), UINT(img->slicePitch));
    assert(SUCCEEDED(hr));
  }
}

IDxcBlob *CompileShader(
    // CompilerするShaderファイルへのパス
    const std::wstring &filePath,
    // CompilerにしようするProfile
    const wchar_t *profile,
    // 初期化で生成したものを3つ
    IDxcUtils *dxcUtils, IDxcCompiler3 *dxcCompiler,
    IDxcIncludeHandler *includeHandler) {
  // 　これからシェーダーをコンパイルする旨をログに出力
  logger.WriteLog(logger.ConvertString(std::format(
      L"Begin CompileShader, path:{},profile: {}\n", filePath, profile)));
  //================================
  // hlslファイルを読む
  //================================

  IDxcBlobEncoding *shaderSource = nullptr;
  HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
  // 読み込めなかったらエラー
  assert(SUCCEEDED(hr));
  // 読み込んだファイルの内容を設定する
  DxcBuffer shaderSourceBuffer;
  shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
  shaderSourceBuffer.Size = shaderSource->GetBufferSize();
  shaderSourceBuffer.Encoding = DXC_CP_UTF8;

  //================================
  // コンパイルする
  //================================

  LPCWSTR arguments[] = {
      filePath.c_str(), // コンパイル対象のhlslファイル名
      L"-E",
      L"main", // エントリーポイントの指定。基本的にmain以外にはしない
      L"-T",
      profile, // ShaderProfileの設定
      L"-Zi",
      L"-Qembed_debug", // デバッグ用の情報を埋め込む
      L"-Od",           // 最適化をしない
      L"-Zpr",          // メモリレイアウトは行優先
  };

  // 実際にShaderをコンパイルする
  IDxcResult *shaderResult = nullptr;
  hr = dxcCompiler->Compile(&shaderSourceBuffer, arguments, _countof(arguments),
                            includeHandler, IID_PPV_ARGS(&shaderResult));
  // コンパイルに失敗したらエラー
  assert(SUCCEEDED(hr));

  //================================
  // 警告・エラーを確認する
  //================================

  // 警告・エラーが出てたらログに出力して止める
  IDxcBlobUtf8 *shaderError = nullptr;
  shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
  if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
    logger.WriteLog(shaderError->GetStringPointer());
    // 警告・エラーだめ絶対
    assert(false);
  }

  //================================
  // コンパイル結果を取得して返す
  //================================

  // コンパイル結果から実行用のバイナリ部分を取得する
  IDxcBlob *shaderBlob = nullptr;
  hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob),
                               nullptr);
  assert(SUCCEEDED(hr));

  // 成功したログを出す
  logger.WriteLog(logger.ConvertString(std::format(
      L"Compile Suceeded, path:{},profile: {}\n", filePath, profile)));
  // もう使わないリソースを解放
  shaderSource->Release();
  shaderResult->Release();

  // 実行用のバイナリを返す
  return shaderBlob;
}

DirectX::ScratchImage LoadTexture(const std::string &filePath) {

  // —————————————————————————————
  // ★ パスが空、またはファイルが存在しないなら
  //   真っ白画像を生成してそのまま返す
  // —————————————————————————————
  if (filePath.empty() || !std::filesystem::exists(filePath)) {
    logger.WriteLog("[Warn] テクスチャが見つかりません。白画像を生成します");
    return CreateWhiteTextureImage();
  }

  DirectX::ScratchImage image{};
  std::wstring filePathW = logger.ConvertString(filePath);
  HRESULT hr = DirectX::LoadFromWICFile(
      filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
  if (FAILED(hr)) {
    return CreateWhiteTextureImage(); // ここも白画像にフォールバック
  }

  DirectX::ScratchImage mipImages{};
  hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(),
                                image.GetMetadata(), DirectX::TEX_FILTER_SRGB,
                                0, mipImages);
  if (FAILED(hr)) {
    return image; // ミップだけ落ちても最低オリジナル画像は返す
  }

  return mipImages;
}

MaterialData LoadMaterialTemplateFile(const std::string &directoryPath,
                                      const std::string &filename) {
  MaterialData materialData{};
  std::string line; // ファイルから読んだ 1 行
  std::ifstream file(directoryPath + "/" + filename);
  assert(file.is_open()); // 開けなかったら止める

  // ファイルを読みMaterialDataを構築
  while (std::getline(file, line)) {
    std::string identifier;
    std::istringstream s(line);
    s >> identifier;

    // identifierに応じた処理
    if (identifier == "map_Kd") {
      std::string textureFilename;
      s >> textureFilename;
      // 連結してファイルパスにする
      materialData.textureFilePath = directoryPath + "/" + textureFilename;
    }
  }

  return materialData;
}


DirectX::ScratchImage CreateWhiteTextureImage() {
  DirectX::ScratchImage whiteImage;
  // 1×1、ミップレベル１、配列サイズ１、フォーマットは RGBA8
  HRESULT hr = whiteImage.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM,
                                       /*width*/ 1, /*height*/ 1,
                                       /*arraySize*/ 1, /*mipLevels*/ 1);
  assert(SUCCEEDED(hr));

  // 先頭レベルの画像データ取得
  auto img = whiteImage.GetImage(0, 0, 0);
  // ピクセル 4 バイトずつすべて 255 に
  uint8_t *p = img->pixels;
  p[0] = 255; // R
  p[1] = 255; // G
  p[2] = 255; // B
  p[3] = 255; // A

  return whiteImage;
}
