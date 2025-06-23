#pragma once
#include "../struct.h"
#include <d3d12.h>
#include <vector>

class Sphere {
public:
  Sphere(ID3D12Device *device, float radius = 1.0f, int sliceCount = 16,
         int stackCount = 16);
  ~Sphere();

  // 描画用ビュー取得
  const D3D12_VERTEX_BUFFER_VIEW &GetVertexBufferView() const {
    return vertexBufferView_;
  }
  const D3D12_INDEX_BUFFER_VIEW &GetIndexBufferView() const {
    return indexBufferView_;
  }
  UINT GetIndexCount() const { return static_cast<UINT>(indices_.size()); }

private:
  std::vector<VertexData> vertices_;
  std::vector<uint32_t> indices_;
  ID3D12Resource *vertexBuffer_ = nullptr;
  ID3D12Resource *indexBuffer_ = nullptr;
  D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
  D3D12_INDEX_BUFFER_VIEW indexBufferView_{};

  void CreateBuffers(ID3D12Device *device);
  void GenerateSphere(float radius, int sliceCount, int stackCount);
};
