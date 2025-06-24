#pragma once
#include "../struct.h" // VertexData, Vector4, Vector2 が定義されている想定
#include <d3d12.h>
#include <vector>

class Sphere {
public:

  Transform sphereTransform = {
      {1.0f, 1.0f, 1.0f}, // スケール
      {0.0f, 0.0f, 0.0f}, // 回転
      {0.0f, 0.0f, 0.0f}  // 位置
  };

  bool isSphere = false;


  Sphere() = default;
  ~Sphere();

  // デバイスとコマンドリストを渡してバッファ生成
  void Initialize(ID3D12Device *device, ID3D12GraphicsCommandList *cmdList,
                  float radius, UINT sliceCount, UINT stackCount);
  // 描画コマンドを積む
  void Draw(ID3D12GraphicsCommandList *cmdList);

private:
  void BuildGeometry(float radius, UINT sliceCount, UINT stackCount);

  std::vector<VertexData> mVertices;
  std::vector<uint16_t> mIndices;

  ID3D12Resource *mVB = nullptr;
  ID3D12Resource *mIB = nullptr;
  D3D12_VERTEX_BUFFER_VIEW mVBView{};
  D3D12_INDEX_BUFFER_VIEW mIBView{};
  UINT mIndexCount = 0;
};
