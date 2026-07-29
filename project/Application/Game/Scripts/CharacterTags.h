#pragma once

/// @file CharacterTags.h
/// @brief スクリプト間で状態を受け渡すための Entity タグキー定義
/// @details 各スクリプトは .cpp 内にローカルなクラスとして定義されているため、
///          スクリプト同士でクラスを直接参照できない。
///          Entity::SetTag / GetTagInt を使って int 値で状態を公開し、
///          そのキー名をここに集約して typo による不一致を防ぐ。

/// @brief 農民が移動中（Walk または Run）か。1 = 移動中
inline constexpr const char* kTagFarmerMoving = "farmerMoving";

/// @brief 農民が攻撃モーション中か。1 = 攻撃中
inline constexpr const char* kTagFarmerAttacking = "farmerAttacking";

/// @brief 農民が鍬を装備済みか。1 = 装備中
inline constexpr const char* kTagFarmerHasHoe = "farmerHasHoe";

/// @brief 農民の手に炎が出ているか。1 = 点火中
/// @details 右クリック / パッドAボタンの長押しで FarmerController が立てる。
///          HoeItem はこれを見て鍬の描画を消す（炎と鍬が重なるのを防ぐ）。
inline constexpr const char* kTagFarmerFire = "farmerFire";
