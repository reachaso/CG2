#pragma once
#include <Windows.h>
#include <cstdint>
#include <d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <memory>

// 既存ヘッダ（プロジェクトの実体に合わせて）
#include "DebugCamera/DebugCamera.h"
#include "Input/Input.h"
#include "Log/Log.h"
#include "MainCamera/MainCamera.h"
#include "Sound/Sound.h"
#include "Window/Window.h"
#include "../../externals/imgui/imgui.h"
#include "../../externals/imgui/imgui_impl_dx12.h"
#include "../../externals/imgui/imgui_impl_win32.h"
#include "function/function.h"
#include "struct.h"

class MyGame {
public:
  MyGame();
  ~MyGame();

  // 起動・終了
  bool Initialize(const char *title, int width, int height);
  void Finalize();

  // 1フレーム
  void Update(); // 入力・カメラ・UIなど
  void Draw();   // コマンド記録～Present
  bool IsRunning() const { return running_; }

private:
  // ========== サブ処理 ==========
  void ProcessSystemMessage_(); // PeekMessage()
  void BeginImGui_();
  void EndImGui_();
  void BeginFrame_(); // バリア遷移, RTV/DSV設定, クリア
  void EndFrame_();   // バリア戻し, キック, フェンス待ち

  // ---- 初期化下請け（main.cpp の該当ブロックを移植）----
  void InitDXGI_();            // Factory, Adapter, Device 生成
  void InitCommandObjects_();  // CommandQueue/List/Allocator
  void InitSwapchainAndRTV_(); // SwapChain, RTV
  void InitDSV_();             // DSV
  void InitDescriptorHeaps_(); // SRV/CBV/UAV/ImGui用
  void InitImGui_();
  void InitAssets_(); // テクスチャ/モデル/バッファ等
  void InitScene_();  // カメラ/サウンド/その他シーン初期値

  // ---- ImGui ドック（既存UIをここへ）----
  void DrawImGui_All_(); // 「各種設定」「カメラモード」ほか

private:
  // 実行制御
  bool running_ = true;
  MSG msg_{};

  // ログ
  Log logger_;

  // Window / 入力
  Window window_;
  Input input_{nullptr};

  // Camera
  MainCamera mainCamera_;
  DebugCamera debugCamera_;
  bool useDebugCamera_ = false;
  bool isGuide_ = false;

  // D3D12 基本
  IDXGIFactory6 *dxgiFactory_ = nullptr;
  IDXGIAdapter4 *adapter_ = nullptr;
  ID3D12Device *device_ = nullptr;
  ID3D12CommandQueue *commandQueue_ = nullptr;
  ID3D12CommandAllocator *commandAllocator_ = nullptr;
  ID3D12GraphicsCommandList *commandList_ = nullptr;
  IDXGISwapChain4 *swapChain_ = nullptr;

  // RT/DS/Descriptor
  static constexpr uint32_t kBufferCount_ = 2;
  ID3D12Resource *swapChainBuffer_[kBufferCount_] = {};
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[kBufferCount_]{};
  ID3D12DescriptorHeap *rtvHeap_ = nullptr;
  ID3D12DescriptorHeap *dsvHeap_ = nullptr;
  ID3D12Resource *depthBuffer_ = nullptr;
  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_{};
  ID3D12DescriptorHeap *srvHeap_ = nullptr;   // シェーダ用
  ID3D12DescriptorHeap *imguiHeap_ = nullptr; // ImGui 専用

  // フェンス
  ID3D12Fence *fence_ = nullptr;
  uint64_t fenceValue_ = 0;
  HANDLE fenceEvent_ = nullptr;

  // サウンド例
  Sound alarm01_;
  Sound alarm02_;
  Sound fanfare_;

  // 描画に使う各種（元 main.cpp の変数群をここに）
  // 例：三角形/球/モデル/スプライト/各種 Transform, Material, Light など
  // Transform transform_{}, modelTransform_{}, ...;
  // bool isTriangle1=false, isTriangle2=false, isSprite=false, isSphere=false;
  // 等々 ※ 実体はあなたの main.cpp の変数をここへ丸ごと移動してください
};
