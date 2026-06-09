---
description: 指定したクラスや関数の単体テストコードを自動生成する
---

# test
指定されたクラスや関数のための単体テスト（Unit Test）のコードを自動生成します。

## ルール (AI向けインストラクション)
1. ユーザーが指定したソースコード（例えば `Math.cpp` や `Utility.cpp`）を読み込むこと。
2. GoogleTest (gtest) などの標準的なC++テストフレームワークを用いたテストコードを作成すること。
3. 境界値テスト（エッジケース）や、例外が発生するパターンのテストも含めること。
4. 生成したテストコードとともに、どこに配置してどう実行すべきかの案内も添えること。

## 出力フォーマット
```cpp
#include <gtest/gtest.h>
#include "対象のヘッダファイル.h"

TEST(TargetClassNameTest, NormalCase) {
    // 正常系のテスト
}

TEST(TargetClassNameTest, EdgeCase) {
    // 異常系・境界値のテスト
}
```
