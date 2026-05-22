# Chaso Engine

DirectX 12ベースの自作ゲームエンジンです。
エンジン層 (`ChasoEngine`) とアプリケーション層 (`ChasoApp`) に分離されており、描画・入力・ウィンドウ管理・各種リソース管理の基盤を提供します。

[![DebugBuild](https://github.com/taiga-0505/Chaso/actions/workflows/DebugBuild.yml/badge.svg)](https://github.com/taiga-0505/Chaso/actions/workflows/DebugBuild.yml)
[![ReleaseBuild](https://github.com/taiga-0505/Chaso/actions/workflows/ReleaseBuild.yml/badge.svg)](https://github.com/taiga-0505/Chaso/actions/workflows/ReleaseBuild.yml)
[![EngineDebugBuild](https://github.com/taiga-0505/Chaso/actions/workflows/EngineDebugBuild.yml/badge.svg?branch=Engine)](https://github.com/taiga-0505/Chaso/actions/workflows/EngineDebugBuild.yml)
[![EngineReleaseBuild](https://github.com/taiga-0505/Chaso/actions/workflows/EngineReleaseBuild.yml/badge.svg?branch=Engine)](https://github.com/taiga-0505/Chaso/actions/workflows/EngineReleaseBuild.yml)
[![CheckUnwantedFiles](https://github.com/taiga-0505/Chaso/actions/workflows/CheckUnwantedFiles.yml/badge.svg)](https://github.com/taiga-0505/Chaso/actions/workflows/CheckUnwantedFiles.yml)

## 概要 (Overview)

本プロジェクトは、DirectX 12を用いた低レイヤーのグラフィックスプログラミングの学習および、高性能なゲーム制作を目的として開発されています。
エンジン機能は静的ライブラリ (`ChasoEngine`) として分離されており、描画パスの最適化・リソース管理・モダンなC++20の機能を活用したコアアーキテクチャを備えています。
ゲームロジックはアプリケーション層 (`ChasoApp`) に実装されています。

## 主な機能 (Features)

- **レンダリング演算の最適化**:
  - `std::async` を活用した自動並列リソースロード基盤。
  - **ノンブロッキング**: メインスレッドを止めずにモデルやテクスチャをバックグラウンドでロード。
  - **自動同期**: シーン遷移時に未完了のロードをエンジンが自動で待機（`WaitAllLoads`）。
  - **安全設計**: ロード未完了のオブジェクトに対する描画ガードの実装。
- **3Dグラフィックス**:
  - [Assimp](https://github.com/assimp/assimp) による多種多様な3Dモデル（.obj, .gltf, .glb, .fbx等）の読み込み。
  - `aiProcess_MakeLeftHanded` による一貫した右手系→左手系座標変換。
  - カスタム頂点バッファ・インデックスバッファによる高速な描画。
  - テクスチャマッピングおよびUVアニメーション。
  - スケルタルアニメーション・スキニング対応（ボーン階層、InverseBindPose、キーフレーム補間）。
  - GPU Compute Shader によるスキニング計算の高速化（汎用 `ComputeShader` クラス）。
- **2Dグラフィックス**:
  - スプライト描画、2Dプリミティブ描画（線、矩形、球体デバッグ表示等）。
- **パーティクルシステム**:
  - インスタンス描画による大量のパーティクル制御（爆発、花火、雨、雪など）。
  - 高精細なマルチレイヤーエフェクト（CenterGlow, Flash, Ring, Debris, Shockwave等）を組み合わせたリッチなHitEffectなどの表現。
- **ライティング**:
  - 平行光源 (Directional Light)、点光源 (Point Light)、スポットライト (Spot Light)、面光源 (Area Light) の制御。
- **ポストプロセッシング (Post-Processing)**:
  - グレースケール、セピア調などの画面エフェクト。
  - ピンポンバッファを用いたマルチパス処理によるエフェクトのスタック（重ねがけ）に対応。

### システム (System)
- **アーキテクチャとデザインパターン**:
  - Factory Pattern による直感的なリソース生成 (Model, Sprite等)。
  - State Pattern による柔軟なシーン遷移。
- **シーン管理**: `SceneManager` によるフェードイン・フェードアウトを利用したスムーズなトランジション。
- **ウィンドウ管理**: Win32 APIによる堅牢なウィンドウ制御とメッセージループ。
- **入力管理**: `Input` クラスによる統括管理。`Keyboard`, `Mouse`, `Controller` (XInput) の各デバイスを個別に取得・制御可能。
- **FPS制御**: デルタタイム計算および60FPS固定機能。
- **シェーダ**: HLSL (Shader Model 6.0+) 対応。Compute Shader (CS) による GPU 汎用計算サポート。

### オーディオ (Audio)
- **サウンド再生**: `Media Foundation` を利用したBGMおよび効果音 (SE) の再生管理。
- **管理クラス**: `BgmManager`, `SeManager` による複数音源の同時再生・ループ制御。

### ツール (Tools)
- **デバッグGUI**: [ImGui](https://github.com/ocornut/imgui) を統合し、実行時にリアルタイムでパラメータ調整が可能（ドッキング機能対応）。

## 動作環境 (Requirements)

- **OS**: Windows 10 / 11 (x64)
- **DirectX**: DirectX 12 (Feature Level 12.0以上)
- **IDE**: Visual Studio 2026 (v145セット)
- **言語**: C++20

## プロジェクト構成 (Project Structure)

本ソリューション (`chaso.sln`) は以下の2プロジェクトで構成されています。

| プロジェクト | 種別 | 役割 |
| :--- | :--- | :--- |
| **ChasoEngine** | 静的ライブラリ (.lib) | 描画・入力・ウィンドウ等のエンジンコア |
| **ChasoApp** | 実行ファイル (.exe) | ゲームロジック・シーン管理 |

## ディレクトリ構成 (Folder Structure)

### Engine (`project/Engine/`)

| ディレクトリ | 役割 |
| :--- | :--- |
| **[Dx12/](project/Engine/Dx12/)** | デバイス、スワップチェーン、パイプライン、汎用 ComputeShader 等のコアラッパー |
| **[Graphics/](project/Engine/Graphics/)** | モデル、スプライト、ライト、エフェクト等の描画オブジェクトの実装 |
| **[Render/](project/Engine/Render/)** | モジュール化された描画クラス（Model/Sprite/Light等）とコンテキスト管理 |
| **[Audio/](project/Engine/Audio/)** | サウンド再生（Sound/BGM/SE）の制御と管理 |
| **[Particle/](project/Engine/Particle/)** | パーティクル生成・更新・計算ロジック |
| **[Camera/](project/Engine/Camera/)** | カメラの管理と変換行列の計算 |
| **[Window/](project/Engine/Window/)** | Win32ウィンドウ管理、メッセージループ処理 |
| **[Input/](project/Engine/Input/)** | キーボード、マウス、コントローラー別の入力管理 |
| **[Common/](project/Engine/Common/)** | 数学ライブラリ、ログ出力、エンジン共通の設定と構造体 |
| **[ImGuiManager/](project/Engine/ImGuiManager/)** | ImGuiの初期化・描画フローとの統合 |

### Application (`project/Application/`)

| ディレクトリ | 役割 |
| :--- | :--- |
| **[Framework/](project/Application/Framework/)** | アプリケーション基盤 (`App`) とコンフィグ管理 |
| **[Game/](project/Application/Game/)** | ゲームロジック（Player・Scene・MapChip・Goal・Coin 等） |

### その他

| ディレクトリ | 役割 |
| :--- | :--- |
| **[Externals/](project/Externals/)** | 外部ライブラリ (Assimp / DirectXTex / ImGui / curl / nlohmann) |
| **[Resources/](project/Resources/)** | テクスチャ、モデル、シェーダ等のリソースファイル（`Shader/Compute/` に CS シェーダ） |

## ビルド方法 (Build)

### Visual Studio を使用する場合

1. `project/chaso.sln` を Visual Studio 2026 で開きます。
2. 構成を `Debug`, `Release`, または `Development` に設定します。
3. プラットフォームを `x64` に設定します。
4. `Ctrl + Shift + B` でソリューションをビルドします。
   - `ChasoEngine` が先にビルドされ、`ChasoApp` がそれをリンクします。

### VS Code / CLI を使用する場合

本プロジェクトにはCLIビルド＆実行環境が用意されています。VS Codeの場合は `Ctrl + Shift + B` からタスク（`🚀 Run Debug` など）を選択するだけで、ビルドからアプリの起動までが一括で行われます。
手動で実行する場合は、プロジェクトルートから以下のPowerShellスクリプトを使用します。

```powershell
# Debug構成でビルド＆実行
powershell -ExecutionPolicy Bypass -File "scripts/build/build_and_run.ps1" -Configuration Debug
```

詳細は `scripts/使い方.md` を参照してください。

## 使用ライブラリ (Libraries)

- [DirectX 12](https://github.com/microsoft/DirectX-Graphics-Samples)
- [Assimp](https://github.com/assimp/assimp)
- [DirectXTex](https://github.com/microsoft/DirectXTex)
- [ImGui](https://github.com/ocornut/imgui)
- Media Foundation (Sound playback)

---
> [!IMPORTANT]
> 本エンジンは教育目的であり、実用的なパフォーマンスとメンテナンス性を重視して設計されています。
