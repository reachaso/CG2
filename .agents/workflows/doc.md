---
description: C++クラスやメソッドにDoxygenコメントを自動生成する
---

# doc
C++のクラスやメソッドにDoxygen形式のドキュメントコメントを自動生成します。

## ルール (AI向けインストラクション)
1. ユーザーが指定したファイル（またはActive Document）のC++コードを読み込むこと。
2. クラス定義や関数宣言に対して、`/// @brief` や `/// @param`, `/// @return` を用いたDoxygen形式のコメントを追記すること。
3. すでにコメントがある場合は、それをフォーマットに沿って整理すること。
4. 処理の中身は変更せず、コメントのみを追記した差分（Diff）を提示すること。

## 出力フォーマット
```cpp
// 修正前のコード例
void Update(float deltaTime);

// 修正後（提案）
/// @brief 毎フレーム呼ばれる更新処理
/// @param deltaTime 前フレームからの経過時間（秒）
void Update(float deltaTime);
```
