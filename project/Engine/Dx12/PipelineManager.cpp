#include "PipelineManager.h"
#include "Common/Log/Log.h"
#include <d3d12.h>
#include <cassert>
#include <format>
#include <fstream>

void PipelineManager::Init(ID3D12Device *device, DXGI_FORMAT rtvFmt,
                           DXGI_FORMAT dsvFmt) {
  // ====================
  // Device
  // ====================
  // デバイス設定
  device_ = device;
  rtvFmt_ = rtvFmt;
  dsvFmt_ = dsvFmt;

  // ====================
  // Compiler
  // ====================
  // シェーダーコンパイラ初期化
  const bool ok = compiler_.Init();
  assert(ok && "ShaderCompiler::Init failed");
}

void PipelineManager::Term() {
  // ====================
  // Pipeline Release
  // ====================
  // 登録済みパイプライン解放
  for (auto &kv : pipelines_) {
    if (kv.second.pipeline)
      kv.second.pipeline->Term();
  }
  pipelines_.clear();
  computePipelines_.clear();
  device_ = nullptr;
}

GraphicsPipeline *PipelineManager::Create(const std::string &key,
                                          const PipelineDesc &desc) {
  // ====================
  // Validate
  // ====================
  // シェーダーパス確認
  assert(!desc.vsPath.empty());
  assert(!desc.psPath.empty());

  // キャッシュヒットチェック
  const std::string cacheKey = makePsoCacheKey_(key, desc);
  D3D12_CACHED_PIPELINE_STATE cachedPSO = {};
  if (auto it = psoCache_.find(cacheKey); it != psoCache_.end()) {
    // ファイルの更新日時を確認
    bool stale = false;
    try {
      auto vsT = std::filesystem::last_write_time(desc.vsPath).time_since_epoch().count();
      auto psT = std::filesystem::last_write_time(desc.psPath).time_since_epoch().count();
      if (vsT != it->second.vsTime || psT != it->second.psTime) {
        stale = true;
      }
    } catch (...) {
      stale = true; // ファイルがない場合は古いとみなす（通常ありえないが）
    }

    if (!stale) {
      cachedPSO.pCachedBlob = it->second.psoData.data();
      cachedPSO.CachedBlobSizeInBytes = it->second.psoData.size();

      // キャッシュヒット時はコンパイル済みのバイナリを利用
      D3D12_SHADER_BYTECODE vsBC{it->second.vsData.data(), it->second.vsData.size()};
      D3D12_SHADER_BYTECODE psBC{it->second.psData.data(), it->second.psData.size()};

      Entry e;
      e.desc = desc;
      e.pipeline = std::make_unique<GraphicsPipeline>();
      e.pipeline->Init(device_);

      const D3D12_INPUT_ELEMENT_DESC *il =
          e.desc.inputLayout.empty() ? nullptr : e.desc.inputLayout.data();
      e.pipeline->BuildEx(il, static_cast<UINT>(e.desc.inputLayout.size()), vsBC,
                          psBC, rtvFmt_, dsvFmt_, e.desc.opt, cachedPSO);

      // キャッシュ不一致でフォールバックした場合に備え、新しい PSO をキャッシュに反映
      if (e.pipeline->IsCacheFallback()) {
        if (auto blob = e.pipeline->GetSerializedBlob()) {
          auto &cache = psoCache_[cacheKey];
          std::vector<uint8_t> newData(
              (uint8_t *)blob->GetBufferPointer(),
              (uint8_t *)blob->GetBufferPointer() + blob->GetBufferSize());
          cache.psoData = std::move(newData);
          cacheUpdated_ = true;
        }
      }

      auto &ref = pipelines_[key] = std::move(e);
      return ref.pipeline.get();
    }
  }

  // シェーダーコンパイル（キャッシュヒットしなかった場合）
  ShaderDesc vsDesc{};
  vsDesc.path = desc.vsPath.c_str();
  vsDesc.entry = desc.vsEntry.c_str();
  vsDesc.target = desc.vsTarget.c_str();
  vsDesc.optimize = desc.optimize;
  vsDesc.debugInfo = desc.debugInfo;

  ShaderDesc psDesc{};
  psDesc.path = desc.psPath.c_str();
  psDesc.entry = desc.psEntry.c_str();
  psDesc.target = desc.psTarget.c_str();
  psDesc.optimize = desc.optimize;
  psDesc.debugInfo = desc.debugInfo;

  CompiledShader vs = compiler_.Compile(vsDesc);
  CompiledShader ps = compiler_.Compile(psDesc);
  if (!vs.HasBlob()) {
      Log::Print("[ERROR] VS Compile Failed!");
      Log::Print(vs.Log());
  }
  if (!ps.HasBlob()) {
      Log::Print("[ERROR] PS Compile Failed!");
      Log::Print(ps.Log());
  }
  assert(vs.HasBlob() && ps.HasBlob());

  return createFromBlobs_(key, desc, vs.Blob(), ps.Blob(), cachedPSO);
}


GraphicsPipeline *PipelineManager::CreateFromFiles(
    const std::string &key, const std::wstring &vsPath,
    const std::wstring &psPath, InputLayoutType layoutType,
    const GPipelineOptions &opt) {
  // ====================
  // Desc
  // ====================
  // パイプライン設定生成
  PipelineDesc d{};
  d.vsPath = vsPath;
  d.psPath = psPath;
  d.inputLayout = MakeInputLayout(layoutType);
  d.opt = opt;

#ifdef _DEBUG
  d.optimize = false;
  d.debugInfo = true;
#else
  d.optimize = true;
  d.debugInfo = false;
#endif

  // ====================
  // Create
  // ====================
  // パイプライン作成
  return Create(key, d);
}

GraphicsPipeline *PipelineManager::Get(const std::string &key) {
  // ====================
  // Find
  // ====================
  // 登録済みパイプライン取得
  auto it = pipelines_.find(key);
  return (it == pipelines_.end()) ? nullptr : it->second.pipeline.get();
}

bool PipelineManager::Rebuild(const std::string &key) {
  // ====================
  // Find
  // ====================
  // 対象パイプライン取得
  auto it = pipelines_.find(key);
  if (it == pipelines_.end())
    return false;

  const PipelineDesc &d = it->second.desc;

  // ====================
  // Compile
  // ====================
  // シェーダー設定
  ShaderDesc vs{};
  vs.path = d.vsPath.c_str();
  vs.entry = d.vsEntry.c_str();
  vs.target = d.vsTarget.c_str();
  vs.optimize = d.optimize;
  vs.debugInfo = d.debugInfo;

  ShaderDesc ps{};
  ps.path = d.psPath.c_str();
  ps.entry = d.psEntry.c_str();
  ps.target = d.psTarget.c_str();
  ps.optimize = d.optimize;
  ps.debugInfo = d.debugInfo;

  // コンパイル
  CompiledShader VS = compiler_.Compile(vs);
  CompiledShader PS = compiler_.Compile(ps);
  if (!VS.HasBlob() || !PS.HasBlob())
    return false;

  // ====================
  // Rebuild
  // ====================
  // パイプライン再構築
  it->second.pipeline->Term();
  it->second.pipeline->Init(device_);

  D3D12_SHADER_BYTECODE vsBC{VS.Blob()->GetBufferPointer(),
                             VS.Blob()->GetBufferSize()};
  D3D12_SHADER_BYTECODE psBC{PS.Blob()->GetBufferPointer(),
                             PS.Blob()->GetBufferSize()};

  const D3D12_INPUT_ELEMENT_DESC *il =
      d.inputLayout.empty() ? nullptr : d.inputLayout.data();
  it->second.pipeline->BuildEx(il, static_cast<UINT>(d.inputLayout.size()),
                               vsBC, psBC, rtvFmt_, dsvFmt_, d.opt,
                               D3D12_CACHED_PIPELINE_STATE{});

  return true;
}

void PipelineManager::RebuildAll() {
  // ====================
  // Rebuild
  // ====================
  // 全パイプライン再ビルド
  for (auto &kv : pipelines_) {
    (void)Rebuild(kv.first);
  }
}

GraphicsPipeline *PipelineManager::createFromBlobs_(
    const std::string &key, const PipelineDesc &desc, IDxcBlob *vs,
    IDxcBlob *ps, const D3D12_CACHED_PIPELINE_STATE &cachedPSO) {
  // ====================
  // Create
  // ====================
  // パイプライン登録
  Entry e;
  e.desc = desc;
  e.pipeline = std::make_unique<GraphicsPipeline>();
  e.pipeline->Init(device_);

  D3D12_SHADER_BYTECODE vsBC =
      vs ? D3D12_SHADER_BYTECODE{vs->GetBufferPointer(), vs->GetBufferSize()}
         : D3D12_SHADER_BYTECODE{nullptr, 0};
  D3D12_SHADER_BYTECODE psBC =
      ps ? D3D12_SHADER_BYTECODE{ps->GetBufferPointer(), ps->GetBufferSize()}
         : D3D12_SHADER_BYTECODE{nullptr, 0};

  const D3D12_INPUT_ELEMENT_DESC *il =
      e.desc.inputLayout.empty() ? nullptr : e.desc.inputLayout.data();
  e.pipeline->BuildEx(il, static_cast<UINT>(e.desc.inputLayout.size()), vsBC,
                      psBC, rtvFmt_, dsvFmt_, e.desc.opt, cachedPSO);

  // キャッシュの更新（新規作成時、またはキャッシュなしで作成した場合）
  if (cachedPSO.pCachedBlob == nullptr && vs != nullptr && ps != nullptr) {
    if (auto blob = e.pipeline->GetSerializedBlob()) {
      const std::string cacheKey = makePsoCacheKey_(key, desc);
      auto &cache = psoCache_[cacheKey];

      // PSOデータ
      cache.psoData.assign((uint8_t *)blob->GetBufferPointer(),
                           (uint8_t *)blob->GetBufferPointer() +
                               blob->GetBufferSize());
      // VSデータ
      cache.vsData.assign((uint8_t *)vs->GetBufferPointer(),
                          (uint8_t *)vs->GetBufferPointer() +
                              vs->GetBufferSize());
      // PSデータ
      cache.psData.assign((uint8_t *)ps->GetBufferPointer(),
                          (uint8_t *)ps->GetBufferPointer() +
                              ps->GetBufferSize());

      // 更新日時も記録
      try {
        cache.vsTime = std::filesystem::last_write_time(desc.vsPath).time_since_epoch().count();
        cache.psTime = std::filesystem::last_write_time(desc.psPath).time_since_epoch().count();
      } catch (...) {
        cache.vsTime = 0;
        cache.psTime = 0;
      }

      cacheUpdated_ = true;
    }
  }

  auto &ref = pipelines_[key] = std::move(e);
  return ref.pipeline.get();
}

void PipelineManager::LoadCache(const std::string &filePath) {
  std::ifstream ifs(filePath, std::ios::binary);
  if (!ifs)
    return;

  size_t count = 0;
  ifs.read((char *)&count, sizeof(count));

  for (size_t i = 0; i < count; ++i) {
    size_t keyLen = 0;
    ifs.read((char *)&keyLen, sizeof(keyLen));
    std::string key(keyLen, '\0');
    ifs.read(&key[0], keyLen);

    PsoCache cache;

    // PSO
    size_t psoSize = 0;
    ifs.read((char *)&psoSize, sizeof(psoSize));
    cache.psoData.resize(psoSize);
    ifs.read((char *)cache.psoData.data(), psoSize);

    // VS
    size_t vsSize = 0;
    ifs.read((char *)&vsSize, sizeof(vsSize));
    cache.vsData.resize(vsSize);
    ifs.read((char *)cache.vsData.data(), vsSize);

    // PS
    size_t psSize = 0;
    ifs.read((char *)&psSize, sizeof(psSize));
    cache.psData.resize(psSize);
    ifs.read((char *)cache.psData.data(), psSize);

    // 日時
    ifs.read((char *)&cache.vsTime, sizeof(cache.vsTime));
    ifs.read((char *)&cache.psTime, sizeof(cache.psTime));

    psoCache_[key] = std::move(cache);
  }

  Log::Print(std::format("[PipelineManager] PSOキャッシュロード完了 (Count: {})",
                         psoCache_.size()));
}

void PipelineManager::SaveCache(const std::string &filePath) {
  if (!cacheUpdated_)
    return;

  // ディレクトリ作成
  std::filesystem::path path(filePath);
  if (path.has_parent_path()) {
    std::filesystem::create_directories(path.parent_path());
  }

  std::ofstream ofs(filePath, std::ios::binary);
  if (!ofs)
    return;

  size_t count = psoCache_.size();
  ofs.write((const char *)&count, sizeof(count));

  for (const auto &kv : psoCache_) {
    size_t keyLen = kv.first.length();
    ofs.write((const char *)&keyLen, sizeof(keyLen));
    ofs.write(kv.first.data(), keyLen);

    // PSO
    size_t psoSize = kv.second.psoData.size();
    ofs.write((const char *)&psoSize, sizeof(psoSize));
    ofs.write((const char *)kv.second.psoData.data(), psoSize);

    // VS
    size_t vsSize = kv.second.vsData.size();
    ofs.write((const char *)&vsSize, sizeof(vsSize));
    ofs.write((const char *)kv.second.vsData.data(), vsSize);

    // PS
    size_t psSize = kv.second.psData.size();
    ofs.write((const char *)&psSize, sizeof(psSize));
    ofs.write((const char *)kv.second.psData.data(), psSize);

    // 日時
    ofs.write((const char *)&kv.second.vsTime, sizeof(kv.second.vsTime));
    ofs.write((const char *)&kv.second.psTime, sizeof(kv.second.psTime));
  }
  cacheUpdated_ = false;
  Log::Print("[PipelineManager] PSOキャッシュ保存完了");
}

std::string PipelineManager::makePsoCacheKey_(const std::string &key,
                                              const PipelineDesc &desc) {
  // 本来は desc の全メンバをハッシュ化すべきだが、
  // 今回は簡易的に「パイプライン名」をキーとする。
  // (シェーダーが変更された場合は key が変わるかキャッシュを消す運用)
  return key;
}

std::vector<D3D12_INPUT_ELEMENT_DESC>
PipelineManager::MakeInputLayout(InputLayoutType type) {
  // ====================
  // InputLayout
  // ====================
  // レイアウト生成
  switch (type) {
  case InputLayoutType::Object3D:
    return {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

  case InputLayoutType::Object3DSkin:
    return {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_SINT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

  case InputLayoutType::Sprite:
    return {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

  case InputLayoutType::Particle:
    return {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

  case InputLayoutType::Primitive3D:
    return {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

  case InputLayoutType::None:
    return {};
  }
  return {};
}

std::string PipelineManager::MakeKey(std::string_view prefix, BlendMode mode) {
  // ====================
  // Key
  // ====================
  // ブレンドモード別キー生成
  const char *suffix = "normal";
  switch (mode) {
  case kBlendModeNone:
    suffix = "none";
    break;
  case kBlendModeNormal:
    suffix = "normal";
    break;
  case kBlendModeAdd:
    suffix = "add";
    break;
  case kBlendModeSubtract:
    suffix = "sub";
    break;
  case kBlendModeMultiply:
    suffix = "mul";
    break;
  case kBlendModeScreen:
    suffix = "screen";
    break;
  case kBlendModePremultiplied:
    suffix = "premul";
    break;
  default:
    suffix = "normal";
    break;
  }

  std::string key;
  key.reserve(prefix.size() + 1 + 8);
  key.append(prefix);
  key.push_back('.');
  key.append(suffix);
  return key;
}

void PipelineManager::RegisterDefaultPipelines() {
  // キャッシュロード
  LoadCache("Resources/Shader/pso_cache.bin");

  // ====================
  // Shader Paths
  // ====================
  // 既定シェーダーパス
  const std::wstring objVs =
      L"Resources/Shader/Object3d/Object3D_Single.VS.hlsl";
  const std::wstring objVsInst =
      L"Resources/Shader/Object3d/Object3D_Inst.VS.hlsl";
  const std::wstring objVsSkin =
      L"Resources/Shader/Object3d/Object3D_Skin.VS.hlsl";
  const std::wstring objPs = L"Resources/Shader/Object3d/Object3D.PS.hlsl";
  const std::wstring glassPs =
      L"Resources/Shader/Object3d/Object3D_Glass.PS.hlsl";
  const std::wstring wirePs =
      L"Resources/Shader/Object3d/Object3D_Wireframe.PS.hlsl";
  const std::wstring sprVs = L"Resources/Shader/Sprite/Sprite.VS.hlsl";
  const std::wstring sprPs = L"Resources/Shader/Sprite/Sprite.PS.hlsl";
  const std::wstring ptlVs = L"Resources/Shader/Particle/Particle.VS.hlsl";
  const std::wstring ptlPs = L"Resources/Shader/Particle/Particle.PS.hlsl";
  const std::wstring primVs =
      L"Resources/Shader/Primitive2D/Primitive2D.VS.hlsl";
  const std::wstring primPs =
      L"Resources/Shader/Primitive2D/Primitive2D.PS.hlsl";
  const std::wstring prim3dVs =
      L"Resources/Shader/Primitive3D/Primitive3D.VS.hlsl";
  const std::wstring prim3dPs =
      L"Resources/Shader/Primitive3D/Primitive3D.PS.hlsl";

  // fullscreen fog overlay (cold mist)
  const std::wstring fogFx = L"Resources/Shader/Post/FogOverlay.hlsl";

  // ====================
  // Helpers
  // ====================
  // 登録ヘルパー
  auto regSet = [&](std::string_view prefix, const std::wstring &vs,
                    const std::wstring &ps, InputLayoutType layout,
                    RootSignatureType root, bool depth, bool depthWrite,
                    D3D12_CULL_MODE cull) {
    for (int m = (int)kBlendModeNone; m <= (int)kBlendModePremultiplied; ++m) {
      const BlendMode mode = (BlendMode)m;

      GPipelineOptions opt{};
      opt.rootType = root;
      opt.enableDepth = depth;
      opt.enableDepthWrite = depthWrite;
      opt.enableAlphaBlend = (mode != kBlendModeNone);
      opt.blendMode = mode;
      opt.cull = cull;

      CreateFromFiles(MakeKey(prefix, mode), vs, ps, layout, opt);
    }
  };

  auto regPrim3D = [&](std::string_view prefix, bool depth) {
    for (int m = (int)kBlendModeNone; m <= (int)kBlendModeScreen; ++m) {
      const BlendMode mode = (BlendMode)m;

      GPipelineOptions opt{};
      opt.rootType = RootSignatureType::Primitive3D;
      opt.enableDepth = depth;
      opt.enableDepthWrite = false;
      opt.enableAlphaBlend = (mode != kBlendModeNone);
      opt.blendMode = mode;
      opt.cull = D3D12_CULL_MODE_NONE;
      opt.topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

      CreateFromFiles(MakeKey(prefix, mode), prim3dVs, prim3dPs,
                      InputLayoutType::Primitive3D, opt);
    }
  };

  // ====================
  // Register
  // ====================
  // object3d：深度ON、書き込みON、基本BACKカリング
  regSet("object3d", objVs, objPs, InputLayoutType::Object3D,
         RootSignatureType::Object3D, true, true, D3D12_CULL_MODE_BACK);

  // object3d_nocull：深度ON、書き込みON、カリング無し
  regSet("object3d_nocull", objVs, objPs, InputLayoutType::Object3D,
         RootSignatureType::Object3D, true, true, D3D12_CULL_MODE_NONE);

  regSet("object3d_inst", objVsInst, objPs, InputLayoutType::Object3D,
         RootSignatureType::Object3DInstancing, true, true,
         D3D12_CULL_MODE_BACK);

  // object3d_skin: スキニング付きモデル描画
  regSet("object3d_skin", objVsSkin, objPs, InputLayoutType::Object3DSkin,
         RootSignatureType::Object3DSkin, true, true, D3D12_CULL_MODE_BACK);

  // object3d_skin_nocull: スキニング付きモデル描画（カリング無し）
  regSet("object3d_skin_nocull", objVsSkin, objPs, InputLayoutType::Object3DSkin,
         RootSignatureType::Object3DSkin, true, true, D3D12_CULL_MODE_NONE);

  // sprite：深度OFF、基本BACK（必要なら NONE に）
  regSet("sprite", sprVs, sprPs, InputLayoutType::Sprite,
         RootSignatureType::Sprite, false, false, D3D12_CULL_MODE_BACK);

  // particle：深度ON、書き込みOFF
  regSet("particle", ptlVs, ptlPs, InputLayoutType::Particle,
         RootSignatureType::Particle, true, false, D3D12_CULL_MODE_NONE);

  // 汎用2D：基本は画面オーバーレイ想定
  regSet("primitive2d", primVs, primPs, InputLayoutType::Sprite,
         RootSignatureType::Sprite, false, false, D3D12_CULL_MODE_NONE);

  // 汎用3Dライン：深度ON版
  regPrim3D("primitive3d", true);

  // 汎用3Dライン：深度OFF版
  regPrim3D("primitive3d_nodepth", false);

  // skybox：深度ON、書き込みOFF（最遠に配置）、カリング無し（内側を見る）
  {
    const std::wstring skyboxVs =
        L"Resources/Shader/Skybox/Skybox.VS.hlsl";
    const std::wstring skyboxPs =
        L"Resources/Shader/Skybox/Skybox.PS.hlsl";

    GPipelineOptions opt{};
    opt.rootType = RootSignatureType::Object3D;
    opt.enableDepth = true;
    opt.enableDepthWrite = false; // DepthWriteMask = ZERO
    opt.enableAlphaBlend = false;
    opt.blendMode = kBlendModeNone;
    opt.cull = D3D12_CULL_MODE_NONE; // 内側を見るためカリング無し

    CreateFromFiles(MakeKey("skybox", kBlendModeNone), skyboxVs, skyboxPs,
                    InputLayoutType::Object3D, opt);
  }

  // fog overlay：深度OFF、αブレンドON、InputLayout無し、Rootは FogOverlay
  {
    PipelineDesc d{};
    d.vsPath = fogFx;
    d.psPath = fogFx;
    d.vsEntry = L"VS";
    d.psEntry = L"PS";
    d.inputLayout = MakeInputLayout(InputLayoutType::None);

    GPipelineOptions opt{};
    opt.rootType = RootSignatureType::FogOverlay;
    opt.enableDepth = false;
    opt.enableDepthWrite = false;
    opt.enableAlphaBlend = true;
    opt.blendMode = kBlendModeNormal;
    opt.cull = D3D12_CULL_MODE_NONE;
    opt.fill = D3D12_FILL_MODE_SOLID;
    d.opt = opt;

#ifdef _DEBUG
    d.optimize = false;
    d.debugInfo = true;
#else
    d.optimize = true;
    d.debugInfo = false;
#endif

    Create(MakeKey("fog", kBlendModeNormal), d);
  }

  // 単体
  {
    GPipelineOptions opt{};
    opt.rootType = RootSignatureType::Object3D;
    opt.enableDepth = true;
    opt.enableDepthWrite = false; // ←重要
    opt.enableAlphaBlend = true;
    opt.blendMode = kBlendModePremultiplied; // ←重要
    opt.cull = D3D12_CULL_MODE_BACK;         // 窓板みたいな薄い板ならおすすめ

    CreateFromFiles(MakeKey("object3d_glass", kBlendModePremultiplied), objVs,
                    glassPs, InputLayoutType::Object3D, opt);
  }

  // 2パス用（背面描画 = FRONTカリング
  {
    GPipelineOptions opt{};
    opt.rootType = RootSignatureType::Object3D;
    opt.enableDepth = true;
    opt.enableDepthWrite = false;
    opt.enableAlphaBlend = true;
    opt.blendMode = kBlendModePremultiplied;
    opt.cull = D3D12_CULL_MODE_FRONT; // ←ここが追加ポイント

    CreateFromFiles(MakeKey("object3d_glass_front", kBlendModePremultiplied),
                    objVs, glassPs, InputLayoutType::Object3D, opt);
  }

  // instancing版も欲しければ
  {
    GPipelineOptions opt{};
    opt.rootType = RootSignatureType::Object3DInstancing;
    opt.enableDepth = true;
    opt.enableDepthWrite = false;
    opt.enableAlphaBlend = true;
    opt.blendMode = kBlendModePremultiplied;
    opt.cull = D3D12_CULL_MODE_BACK;

    CreateFromFiles(MakeKey("object3d_glass_inst", kBlendModePremultiplied),
                    objVsInst, glassPs, InputLayoutType::Object3D, opt);
  }

  // instancing背面用
  {
    GPipelineOptions opt{};
    opt.rootType = RootSignatureType::Object3DInstancing;
    opt.enableDepth = true;
    opt.enableDepthWrite = false;
    opt.enableAlphaBlend = true;
    opt.blendMode = kBlendModePremultiplied;
    opt.cull = D3D12_CULL_MODE_FRONT;

    CreateFromFiles(
        MakeKey("object3d_glass_inst_front", kBlendModePremultiplied),
        objVsInst, glassPs, InputLayoutType::Object3D, opt);
  }

  // ワイヤーフレーム用
  {
    for (int m = (int)kBlendModeNone; m <= (int)kBlendModePremultiplied; ++m) {
      const BlendMode mode = (BlendMode)m;
      GPipelineOptions opt{};
      opt.rootType = RootSignatureType::Object3D;
      opt.enableDepth = true;
      opt.enableDepthWrite = false; // ソリッドの上に重なるのでDepthWriteはOFFで運用
      opt.enableAlphaBlend = (mode != kBlendModeNone);
      opt.blendMode = mode;
      opt.cull = D3D12_CULL_MODE_NONE; // 両面のワイヤーフレームを描く
      opt.fill = D3D12_FILL_MODE_WIREFRAME;

      // 単体用
      CreateFromFiles(MakeKey("object3d_wire", mode),
                      objVs, wirePs, InputLayoutType::Object3D, opt);

      // インスタンシング用
      opt.rootType = RootSignatureType::Object3DInstancing;
      CreateFromFiles(MakeKey("object3d_wire_inst", mode),
                      objVsInst, wirePs, InputLayoutType::Object3D, opt);

      // スキニング用
      opt.rootType = RootSignatureType::Object3DSkin;
      CreateFromFiles(MakeKey("object3d_wire_skin", mode),
                      objVsSkin, wirePs, InputLayoutType::Object3DSkin, opt);
    }
  }
  
  // ============================================================
  // View Shading デバッグパイプライン
  // ============================================================
  {
    const std::wstring faceOriPs =
        L"Resources/Shader/Object3d/Object3D_FaceOrientation.PS.hlsl";
    const std::wstring randColorPs =
        L"Resources/Shader/Object3d/Object3D_RandomColor.PS.hlsl";
    const std::wstring solidShadingPs =
        L"Resources/Shader/Object3d/Object3D_SolidShading.PS.hlsl";

    // --- FaceOrientation (カリング無し：裏面を可視化する必要がある) ---
    {
      GPipelineOptions opt{};
      opt.rootType = RootSignatureType::Object3D;
      opt.enableDepth = true;
      opt.enableDepthWrite = true;
      opt.enableAlphaBlend = false;
      opt.blendMode = kBlendModeNone;
      opt.cull = D3D12_CULL_MODE_NONE; // 裏面も描画する

      CreateFromFiles("object3d_faceori.none",
                      objVs, faceOriPs, InputLayoutType::Object3D, opt);

      // インスタンシング用
      opt.rootType = RootSignatureType::Object3DInstancing;
      CreateFromFiles("object3d_faceori_inst.none",
                      objVsInst, faceOriPs, InputLayoutType::Object3D, opt);

      // スキニング用
      opt.rootType = RootSignatureType::Object3DSkin;
      CreateFromFiles("object3d_faceori_skin.none",
                      objVsSkin, faceOriPs, InputLayoutType::Object3DSkin, opt);
    }

    // --- RandomColor ---
    {
      GPipelineOptions opt{};
      opt.rootType = RootSignatureType::Object3D;
      opt.enableDepth = true;
      opt.enableDepthWrite = true;
      opt.enableAlphaBlend = false;
      opt.blendMode = kBlendModeNone;
      opt.cull = D3D12_CULL_MODE_BACK;

      CreateFromFiles("object3d_randcolor.none",
                      objVs, randColorPs, InputLayoutType::Object3D, opt);

      // インスタンシング用
      opt.rootType = RootSignatureType::Object3DInstancing;
      CreateFromFiles("object3d_randcolor_inst.none",
                      objVsInst, randColorPs, InputLayoutType::Object3D, opt);

      // スキニング用
      opt.rootType = RootSignatureType::Object3DSkin;
      CreateFromFiles("object3d_randcolor_skin.none",
                      objVsSkin, randColorPs, InputLayoutType::Object3DSkin, opt);
    }

    // --- SolidShading (Half-Lambert) ---
    {
      GPipelineOptions opt{};
      opt.rootType = RootSignatureType::Object3D;
      opt.enableDepth = true;
      opt.enableDepthWrite = true;
      opt.enableAlphaBlend = false;
      opt.blendMode = kBlendModeNone;
      opt.cull = D3D12_CULL_MODE_BACK;

      CreateFromFiles("object3d_solid.none",
                      objVs, solidShadingPs, InputLayoutType::Object3D, opt);

      // インスタンシング用
      opt.rootType = RootSignatureType::Object3DInstancing;
      CreateFromFiles("object3d_solid_inst.none",
                      objVsInst, solidShadingPs, InputLayoutType::Object3D, opt);

      // スキニング用
      opt.rootType = RootSignatureType::Object3DSkin;
      CreateFromFiles("object3d_solid_skin.none",
                      objVsSkin, solidShadingPs, InputLayoutType::Object3DSkin, opt);
    }
  }
  // ====================
  // PostProcess
  // ====================
  // 共通フルスクリーンVS
  const std::wstring fullscreenVs =
      L"Resources/Shader/Fullscreen/Fullscreen.VS.hlsl";

  // copyimage：ポストプロセス転送用
  {
    GPipelineOptions opt{};
    opt.rootType = RootSignatureType::PostProcess;
    opt.enableDepth = false;
    opt.enableDepthWrite = false;
    opt.enableAlphaBlend = false; // 転送なので基本 OFF
    opt.cull = D3D12_CULL_MODE_NONE;

    CreateFromFiles("copyimage.none",
                    fullscreenVs,
                    L"Resources/Shader/CopyImage/CopyImage.PS.hlsl",
                    InputLayoutType::None, opt);
  }

  // grayscale：グレースケール
  {
    GPipelineOptions opt{};
    opt.rootType = RootSignatureType::PostProcess;
    opt.enableDepth = false;
    opt.enableDepthWrite = false;
    opt.enableAlphaBlend = false;
    opt.cull = D3D12_CULL_MODE_NONE;

    CreateFromFiles("grayscale.none",
                    fullscreenVs,
                    L"Resources/Shader/Grayscale/Grayscale.PS.hlsl",
                    InputLayoutType::None, opt);
  }

  // sepia：セピア調
  {
    GPipelineOptions opt{};
    opt.rootType = RootSignatureType::PostProcess;
    opt.enableDepth = false;
    opt.enableDepthWrite = false;
    opt.enableAlphaBlend = false;
    opt.cull = D3D12_CULL_MODE_NONE;

    CreateFromFiles("sepia.none",
                    fullscreenVs,
                    L"Resources/Shader/Sepia/Sepia.PS.hlsl",
                    InputLayoutType::None, opt);
  }

  // vignette：ビネット（周辺減光）
  {
    GPipelineOptions opt{};
    opt.rootType = RootSignatureType::PostProcess;
    opt.enableDepth = false;
    opt.enableDepthWrite = false;
    opt.enableAlphaBlend = false;
    opt.cull = D3D12_CULL_MODE_NONE;

    CreateFromFiles("vignette.none",
                    fullscreenVs,
                    L"Resources/Shader/Vignette/Vignette.PS.hlsl",
                    InputLayoutType::None, opt);
  }

  // boxfilter：ボックスフィルタ（平滑化）
  {
    GPipelineOptions opt{};
    opt.rootType = RootSignatureType::PostProcess;
    opt.enableDepth = false;
    opt.enableDepthWrite = false;
    opt.enableAlphaBlend = false;
    opt.cull = D3D12_CULL_MODE_NONE;

    CreateFromFiles("boxfilter.none",
                    fullscreenVs,
                    L"Resources/Shader/BoxFilter/BoxFilter.PS.hlsl",
                    InputLayoutType::None, opt);
  }

  // depthbasedoutline：深度ベースアウトライン
  {
    GPipelineOptions opt{};
    opt.rootType = RootSignatureType::PostProcess;
    opt.enableDepth = false;
    opt.enableDepthWrite = false;
    opt.enableAlphaBlend = false;
    opt.cull = D3D12_CULL_MODE_NONE;

    CreateFromFiles("depthbasedoutline.none",
                    fullscreenVs,
                    L"Resources/Shader/DepthBasedOutline/DepthBasedOutline.PS.hlsl",
                    InputLayoutType::None, opt);
  }

  // radialblur：ラジアルブラー
  {
    GPipelineOptions opt{};
    opt.rootType = RootSignatureType::PostProcess;
    opt.enableDepth = false;
    opt.enableDepthWrite = false;
    opt.enableAlphaBlend = false;
    opt.cull = D3D12_CULL_MODE_NONE;

    CreateFromFiles("radialblur.none",
                    fullscreenVs,
                    L"Resources/Shader/RadialBlur/RadialBlur.PS.hlsl",
                    InputLayoutType::None, opt);
  }

  // dissolve：ディゾルブ（ノイズマスクによる消失演出）
  {
    GPipelineOptions opt{};
    opt.rootType = RootSignatureType::PostProcess;
    opt.enableDepth = false;
    opt.enableDepthWrite = false;
    opt.enableAlphaBlend = false;
    opt.cull = D3D12_CULL_MODE_NONE;

    CreateFromFiles("dissolve.none",
                    fullscreenVs,
                    L"Resources/Shader/Dissolve/Dissolve.PS.hlsl",
                    InputLayoutType::None, opt);
  }

  // random：ランダムノイズ（プロシージャル生成ノイズ）
  {
    GPipelineOptions opt{};
    opt.rootType = RootSignatureType::PostProcess;
    opt.enableDepth = false;
    opt.enableDepthWrite = false;
    opt.enableAlphaBlend = false;
    opt.cull = D3D12_CULL_MODE_NONE;

    CreateFromFiles("random.none",
                    fullscreenVs,
                    L"Resources/Shader/Random/Random.PS.hlsl",
                    InputLayoutType::None, opt);
  }

  // ====================
  // Compute Shader
  // ====================
  // skinning_cs: Compute Shader スキニング
  CreateCompute("skinning_cs",
                L"Resources/Shader/Compute/Skinning.CS.hlsl",
                RootSignatureType::SkinningCS);

  // init_particle_cs: GPU Particle 初期化用 Compute Shader
  CreateCompute("init_particle_cs",
                L"Resources/Shader/Compute/InitializeParticle.CS.hlsl",
                RootSignatureType::InitParticleCS);

  // gpu_particle: GPU Particle 描画用（ブレンドモード別）
  {
    const std::wstring gpuPtlVs = L"Resources/Shader/Particle/GPUParticle.VS.hlsl";
    const std::wstring gpuPtlPs = L"Resources/Shader/Particle/GPUParticle.PS.hlsl";

    for (int m = (int)kBlendModeNone; m <= (int)kBlendModePremultiplied; ++m) {
      const BlendMode mode = (BlendMode)m;

      GPipelineOptions opt{};
      opt.rootType = RootSignatureType::GPUParticle;
      opt.enableDepth = true;
      opt.enableDepthWrite = false;
      opt.enableAlphaBlend = (mode != kBlendModeNone);
      opt.blendMode = mode;
      opt.cull = D3D12_CULL_MODE_NONE;

      CreateFromFiles(MakeKey("gpu_particle", mode), gpuPtlVs, gpuPtlPs,
                      InputLayoutType::Particle, opt);
    }
  }

  Log::Print(std::format("[PipelineManager] デフォルトパイプライン登録完了 (Graphics: {}, Compute: {})", pipelines_.size(), computePipelines_.size()));

  // キャッシュ保存
  SaveCache("Resources/Shader/pso_cache.bin");
}

// ============================================================================
// Compute Pipeline
// ============================================================================

void PipelineManager::CreateCompute(const std::string &key,
                                     const std::wstring &csPath,
                                     RootSignatureType rootType) {
  // シェーダーコンパイル
  ShaderDesc cs{};
  cs.path = csPath.c_str();
  cs.entry = L"main";
  cs.target = L"cs_6_0";
#ifdef _DEBUG
  cs.optimize = false;
  cs.debugInfo = true;
#else
  cs.optimize = true;
  cs.debugInfo = false;
#endif

  CompiledShader CS = compiler_.Compile(cs);
  if (!CS.HasBlob()) {
    Log::Print(std::format("[PipelineManager] CS compile failed: {}", key));
    return;
  }

  // --- Root Signature 構築 ---
  D3D12_ROOT_PARAMETER params[4] = {};
  D3D12_DESCRIPTOR_RANGE ranges[1] = {};
  UINT paramCount = 0;

  if (rootType == RootSignatureType::InitParticleCS) {
    // InitParticleCS: UAV u0 のみ
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[0].BaseShaderRegister = 0;
    ranges[0].NumDescriptors = 1;
    ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    params[0].DescriptorTable.NumDescriptorRanges = 1;
    params[0].DescriptorTable.pDescriptorRanges = &ranges[0];

    paramCount = 1;
  } else {
    // SkinningCS: SRV t0, SRV t1, UAV u0, CBV b0
    // 0: SRV t0 MatrixPalette
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    params[0].Descriptor.ShaderRegister = 0;

    // 1: SRV t1 InputVertices
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    params[1].Descriptor.ShaderRegister = 1;

    // 2: UAV u0 OutputVertices (descriptor table)
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[0].BaseShaderRegister = 0;
    ranges[0].NumDescriptors = 1;
    ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    params[2].DescriptorTable.NumDescriptorRanges = 1;
    params[2].DescriptorTable.pDescriptorRanges = &ranges[0];

    // 3: CBV b0 SkinningInformation
    params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    params[3].Descriptor.ShaderRegister = 0;

    paramCount = 4;
  }

  D3D12_ROOT_SIGNATURE_DESC rsDesc{};
  rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
  rsDesc.pParameters = params;
  rsDesc.NumParameters = paramCount;
  rsDesc.pStaticSamplers = nullptr;
  rsDesc.NumStaticSamplers = 0;

  Microsoft::WRL::ComPtr<ID3DBlob> sig, err;
  HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                            sig.GetAddressOf(), err.GetAddressOf());
  if (FAILED(hr)) {
    if (err) OutputDebugStringA((char *)err->GetBufferPointer());
    Log::Print(std::format("[PipelineManager] CS root sig serialize failed: {}", key));
    return;
  }

  ComputeEntry entry{};
  hr = device_->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(),
                                    IID_PPV_ARGS(&entry.root));
  if (FAILED(hr)) {
    Log::Print(std::format("[PipelineManager] CS root sig create failed: {}", key));
    return;
  }

  // デバッグ名設定
  std::wstring rootName = std::wstring(key.begin(), key.end()) + L"::RootSignature";
  entry.root->SetName(rootName.c_str());

  // --- Compute PSO 構築 ---
  D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
  psoDesc.pRootSignature = entry.root.Get();
  psoDesc.CS = {CS.Blob()->GetBufferPointer(), CS.Blob()->GetBufferSize()};

  hr = device_->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&entry.pso));
  if (FAILED(hr)) {
    Log::Print(std::format("[PipelineManager] CS PSO create failed: {}", key));
    return;
  }

  std::wstring psoName = std::wstring(key.begin(), key.end()) + L"::PSO";
  entry.pso->SetName(psoName.c_str());

  computePipelines_[key] = std::move(entry);
  Log::Print(std::format("[PipelineManager] Compute Pipeline created: {}", key));
}

ID3D12PipelineState *PipelineManager::GetComputePSO(const std::string &key) {
  auto it = computePipelines_.find(key);
  return (it != computePipelines_.end()) ? it->second.pso.Get() : nullptr;
}

ID3D12RootSignature *PipelineManager::GetComputeRoot(const std::string &key) {
  auto it = computePipelines_.find(key);
  return (it != computePipelines_.end()) ? it->second.root.Get() : nullptr;
}
