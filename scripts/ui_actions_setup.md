---
description: VS CodeにProject Actionsを導入し、ビルド＆実行用のUIボタンを自動構築する
---
# Project Actions (UIボタン) 自動構築ワークフロー

> **概要**: CLI用のビルド環境（`build_and_run.ps1`など）が整っていることを前提に、VS Code拡張機能「Project Actions」を利用して、エディタ画面下に1クリックで実行できるボタン群（Debug/Release/Clean等）を自動構築するワークフロー。前提環境がない場合はそのセットアップから誘導する。

## Phase 1: 前提環境（ビルドスクリプト）の確認と自動セットアップ
1. プロジェクト内に `scripts/build/build_and_run.ps1` などのビルド用PowerShellスクリプトが存在するか確認する（`list_dir`等を使用）。
2. **【存在しない場合（他環境での自律的セットアップ）】**
   別のプロジェクト等で過去のワークフローファイルが存在しない場合でも、AI自身が以下の手順で自動的に環境を構築すること。
   - ユーザーに「⚠️ UIボタンを設置するための『ビルドスクリプト群』がまだプロジェクトに存在しないようです。先にCLIビルド環境を自動でセットアップしますか？」と確認する。
   - **YESの場合の自律構築手順**:
     1. ディレクトリをスキャンして `.sln` や `CMakeLists.txt` の位置を特定し、そのファイルから**存在するすべてのビルド構成（Debug, Release, Development, Shippingなど）を抽出**する。
     2. `scripts/build/` フォルダを作成し、CLIから一発でビルド・実行ができる `build_and_run.ps1` を生成する。
        *(※スクリプトには `[ValidateSet("Debug", "Release", "Development"...)] $Configuration` のように、抽出した全ての構成を引数処理に含めること)*
     3. 併せて `clean_build.ps1`（キャッシュ削除＆再ビルド）や `format_code.ps1` などの便利スクリプトも生成する。
     4. **文字化け防止の絶対ルール**: PowerShellスクリプトは必ず「BOM付き UTF-8」で保存し、MSBuildのログ出力等には `/flp:Encoding=UTF-8` を付与して文字化けやパースエラーを防ぐこと。
   - 上記のスクリプト群の生成が完了してから Phase 2 へ進む。

## Phase 2: Project Actions 拡張機能の導入確認
1. 前提環境が整っていることが確認できたら、ターミナルで `code --list-extensions` を実行し、`sanjula.project-actions` がインストールされているか確認する。
   *(※VS Code CLIコマンドが使えない環境の場合は、ユーザーに直接ヒアリングして確認する)*
2. **【インストールされていない場合】**
   ユーザーに拡張機能のインストールを促し、完了するまで待機する。
   > 「💡 VS Codeの画面下部に実行ボタンを追加するために、拡張機能 **Project Actions** (作者: Sanjula Ganepola) が必要です。拡張機能タブからインストールをお願いします！インストールが完了したら教えてください。」

## Phase 3: .project-actions.json の生成とUI構築
1. 拡張機能の存在が確認できたら（またはユーザーからインストール完了の報告を受けたら）、プロジェクトルートに `.project-actions.json` を新規作成、または上書き更新する。
2. ファイルには以下の要素をJSON形式で記述する。**特に「Run」ボタンについては、Phase 1で抽出したすべてのビルド構成（Debug, Release, Development 等）ごとに別々のボタンを動的に用意**すること。
   - `▶ Run (各構成名)`: コマンド `powershell -ExecutionPolicy Bypass -File scripts/build/build_and_run.ps1 -Configuration <構成名>` (構成ごとに色を変えること。例: Debug=緑, Dev=黄, Release=赤)
   - `🗑️ Clean & Build`: コマンド `powershell -ExecutionPolicy Bypass -File scripts/build/clean_build.ps1 -Configuration Debug` (紫系アイコン)
   - `🪣 Format Code`: コマンド `powershell -ExecutionPolicy Bypass -File scripts/build/format_code.ps1` (水色系アイコン)
3. **【🚨重要: 文字コード保護】**
   生成されるボタンから呼ばれる `.ps1` スクリプト群について、PowerShell実行時の文字化け（パースエラー）を防ぐため、必ず「BOM付きUTF-8」で保存されていることを保証すること。AIがファイルを上書きした際は文字コードに注意する。

## Phase 4: 完了報告
1. ボタンの生成が完了したことをユーザーに報告し、生成されたボタンの一覧を箇条書きで提示する。
2. 「画面左下のステータスバー等にボタンが表示されているか確認してください。もし表示されない場合は VS Code のウィンドウを再読み込み (Developer: Reload Window) してみてください」と案内してワークフローを完了する。
