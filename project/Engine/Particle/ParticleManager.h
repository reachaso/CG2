#pragma once
#include "Particle.h"
#include "GPUParticle.h"
#include "EffectPreset.h"
#include <unordered_map>
#include <string>
#include <memory>

namespace RC {

/// @class ParticleManager
/// @brief 全てのパーティクルシステムとエフェクトプリセットを一元管理するシングルトンクラス
/// @details 各種パーティクルシステム（実体）の登録、プリセット（演出データ）の管理、およびエフェクトの再生・更新・描画を統括します。
class ParticleManager {
public:
    /// @brief シングルトンインスタンスを取得する
    /// @return ParticleManager の参照
    static ParticleManager& GetInstance() {
        static ParticleManager instance;
        return instance;
    }

    ParticleManager(const ParticleManager&) = delete;
    ParticleManager& operator=(const ParticleManager&) = delete;

    /// @brief 全てのパーティクルシステムを更新する
    /// @param view ビュー行列
    /// @param proj プロジェクション行列
    /// @param deltaTime フレーム間隔 (秒)
    void Update(const RC::Matrix4x4& view, const RC::Matrix4x4& proj, float deltaTime = 0.016f);

    /// @brief 全てのパーティクルシステムを描画する
    /// @param ctx シーンコンテキスト
    /// @param cl グラフィックスコマンドリスト
    void Render(SceneContext& ctx, ID3D12GraphicsCommandList* cl);

    /// @brief 全てのパーティクルシステムのデバッグ用 UI を表示する
    void DrawImGui();

    // ============================================
    // システム（実体）の管理
    // ============================================
    
    /// @brief パーティクルシステムの実体を登録する
    /// @param name 登録名
    /// @param system システムの unique_ptr
    void RegisterSystem(const std::string& name, std::unique_ptr<Particle> system);

    /// @brief 名前でパーティクルシステムの実体を取得する
    /// @param name 登録名
    /// @return システムへのポインタ（見つからない場合は nullptr）
    Particle* GetSystem(const std::string& name);

    /// @brief 登録されている全てのシステムを削除する
    void ClearSystems();

    // ============================================
    // GPU Particle（GPU 系統）の管理
    // ============================================

    /// @brief GPU パーティクルシステムを登録する
    /// @param name 登録名
    /// @param system GPU パーティクルシステムの unique_ptr
    void RegisterGPUSystem(const std::string& name, std::unique_ptr<GPUParticle> system);

    /// @brief 名前で GPU パーティクルシステムを取得する
    /// @param name 登録名
    /// @return GPUParticle へのポインタ（見つからない場合は nullptr）
    GPUParticle* GetGPUSystem(const std::string& name);

    /// @brief 登録されている全ての GPU システムを削除する
    void ClearGPUSystems();

    // ============================================
    // エフェクト（プリセット）の管理
    // ============================================

    /// @brief エフェクトのプリセットデータ（演出の組み合わせ）を登録する
    /// @param preset プリセットデータ
    void RegisterPreset(const EffectPreset& preset);

    /// @brief 登録済みのプリセット名を使用して、指定位置でエフェクトを再生する
    /// @param presetName プリセット名
    /// @param pos 再生座標
    void PlayEffect(const std::string& presetName, const Vector3& pos);

    // ============================================
    // 直接特定のパーティクルを放出するユーティリティ
    // ============================================

    /// @brief 特定のパーティクルを直接指定位置へ放出する
    /// @param particleName パーティクル種類名
    /// @param pos 放出座標
    /// @param count 放出数
    /// @param scale スケール倍率
    /// @param color 乗算カラー
    void Emit(const std::string& particleName, const Vector3& pos, int count, float scale = 1.0f, const Vector4& color = {1.0f, 1.0f, 1.0f, 1.0f});

private:
    ParticleManager() = default;
    ~ParticleManager() = default;

    std::unordered_map<std::string, std::unique_ptr<Particle>> systems_;       ///< 登録済み CPU パーティクルシステムのマップ
    std::unordered_map<std::string, std::unique_ptr<GPUParticle>> gpuSystems_; ///< 登録済み GPU パーティクルシステムのマップ
    std::unordered_map<std::string, EffectPreset> presets_;                    ///< 登録済みエフェクトプリセットのマップ
};

} // namespace RC
