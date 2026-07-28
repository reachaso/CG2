#include "Animation.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cassert>
#include <filesystem>

namespace RC {

namespace {
/// @brief 内部共通: 指定インデックスのアニメーションをパースする
Animation ParseAnimation_(const aiScene* scene, int animIndex) {
    Animation animation;
    if (!scene || animIndex < 0 || static_cast<unsigned>(animIndex) >= scene->mNumAnimations) {
        return animation;
    }

    aiAnimation* animationAssimp = scene->mAnimations[animIndex];
    animation.duration = float(animationAssimp->mDuration / animationAssimp->mTicksPerSecond);

    // NodeAnimationを解析する
    for (uint32_t channelIndex = 0; channelIndex < animationAssimp->mNumChannels; ++channelIndex) {
        aiNodeAnim* nodeAnimationAssimp = animationAssimp->mChannels[channelIndex];
        NodeAnimation& nodeAnimation = animation.nodeAnimations[nodeAnimationAssimp->mNodeName.C_Str()];

        // Translate
        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumPositionKeys; ++keyIndex) {
            aiVectorKey& keyAssimp = nodeAnimationAssimp->mPositionKeys[keyIndex];
            KeyframeVector3 keyframe;
            keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond);
            keyframe.value = { keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z };
            nodeAnimation.translate.push_back(keyframe);
        }

        // Rotate
        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumRotationKeys; ++keyIndex) {
            aiQuatKey& keyAssimp = nodeAnimationAssimp->mRotationKeys[keyIndex];
            KeyframeQuaternion keyframe;
            keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond);
            keyframe.value = { keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z, keyAssimp.mValue.w };
            nodeAnimation.rotate.push_back(keyframe);
        }

        // Scale
        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumScalingKeys; ++keyIndex) {
            aiVectorKey& keyAssimp = nodeAnimationAssimp->mScalingKeys[keyIndex];
            KeyframeVector3 keyframe;
            keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond);
            keyframe.value = { keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z };
            nodeAnimation.scale.push_back(keyframe);
        }
    }

    return animation;
}
} // namespace

Animation LoadAnimationFile(const std::string& filePath) {
    return LoadAnimationFile(filePath, 0);
}

Animation LoadAnimationFile(const std::string& filePath, int animIndex) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_MakeLeftHanded);
    return ParseAnimation_(scene, animIndex);
}

int GetAnimationCount(const std::string& filePath) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_MakeLeftHanded);
    if (!scene) return 0;
    return static_cast<int>(scene->mNumAnimations);
}

Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time) {
    assert(!keyframes.empty()); // キーがないものは返す値がわからないのでダメ
    if (keyframes.empty()) {
        return Vector3{0.0f, 0.0f, 0.0f}; // 安全なフォールバック
    }
    if (keyframes.size() == 1 || time <= keyframes[0].time) {
        return keyframes[0].value;
    }

    for (size_t index = 0; index < keyframes.size() - 1; ++index) {
        size_t nextIndex = index + 1;
        if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
            float t = (time - keyframes[index].time) / (keyframes[nextIndex].time - keyframes[index].time);
            return Lerp(keyframes[index].value, keyframes[nextIndex].value, t);
        }
    }

    return (*keyframes.rbegin()).value;
}

Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time) {
    assert(!keyframes.empty()); // キーがないものは返す値がわからないのでダメ
    if (keyframes.empty()) {
        return Quaternion{0.0f, 0.0f, 0.0f, 1.0f}; // 単位クオータニオン（安全なフォールバック）
    }
    if (keyframes.size() == 1 || time <= keyframes[0].time) {
        return keyframes[0].value;
    }

    for (size_t index = 0; index < keyframes.size() - 1; ++index) {
        size_t nextIndex = index + 1;
        if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
            float t = (time - keyframes[index].time) / (keyframes[nextIndex].time - keyframes[index].time);
            return Slerp(keyframes[index].value, keyframes[nextIndex].value, t);
        }
    }

    return (*keyframes.rbegin()).value;
}

} // namespace RC
