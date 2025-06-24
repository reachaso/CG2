#include "Sphere.h"
#include <cassert>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Sphere::~Sphere() {
  if (mVB)
    mVB->Release();
  if (mIB)
    mIB->Release();
}

void Sphere::Initialize(ID3D12Device *device,
                        ID3D12GraphicsCommandList *cmdList, float radius,
                        UINT sliceCount, UINT stackCount) {
  // 頂点・インデックス生成
  BuildGeometry(radius, sliceCount, stackCount);

  for (auto &v : mVertices) {
    float len =
        std::sqrt(v.Position.x * v.Position.x + v.Position.y * v.Position.y +
                  v.Position.z * v.Position.z);
    if (len > 0.0f) {
      v.Normal.x = v.Position.x / len;
      v.Normal.y = v.Position.y / len;
      v.Normal.z = v.Position.z / len;
    } else {
      v.Normal = {0.0f, 1.0f, 0.0f};
    }
  }

  // --- 頂点バッファ作成 ---
  {
    const UINT vbBytes = UINT(mVertices.size() * sizeof(VertexData));
    // ヒーププロパティ（UPLOAD）
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    // リソース記述子
    D3D12_RESOURCE_DESC resDesc{};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = vbBytes;
    resDesc.Height = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.Format = DXGI_FORMAT_UNKNOWN;
    resDesc.SampleDesc.Count = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    HRESULT hr = device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mVB));
    assert(SUCCEEDED(hr));

    // マッピングしてデータ転送
    VertexData *mapped = nullptr;
    mVB->Map(0, nullptr, reinterpret_cast<void **>(&mapped));
    memcpy(mapped, mVertices.data(), vbBytes);
    mVB->Unmap(0, nullptr);

    // バッファビュー設定
    mVBView.BufferLocation = mVB->GetGPUVirtualAddress();
    mVBView.StrideInBytes = sizeof(VertexData);
    mVBView.SizeInBytes = vbBytes;
  }

  // --- インデックスバッファ作成 ---
  {
    const UINT ibBytes = UINT(mIndices.size() * sizeof(uint16_t));
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resDesc{};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = ibBytes;
    resDesc.Height = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.Format = DXGI_FORMAT_UNKNOWN;
    resDesc.SampleDesc.Count = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    HRESULT hr = device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mIB));
    assert(SUCCEEDED(hr));

    uint16_t *mapped = nullptr;
    mIB->Map(0, nullptr, reinterpret_cast<void **>(&mapped));
    memcpy(mapped, mIndices.data(), ibBytes);
    mIB->Unmap(0, nullptr);

    mIBView.BufferLocation = mIB->GetGPUVirtualAddress();
    mIBView.Format = DXGI_FORMAT_R16_UINT;
    mIBView.SizeInBytes = ibBytes;

    mIndexCount = UINT(mIndices.size());
  }
}

void Sphere::BuildGeometry(float radius, UINT sliceCount, UINT stackCount) {
  mVertices.clear();
  mIndices.clear();

  // --- 頂点生成 ---
  // 上極点
  mVertices.push_back({Vector4(0, +radius, 0, 1), Vector2(0.0f, 0.0f)});

  // スタック（緯度）の中間円
  for (UINT latIndex = 1; latIndex < stackCount; ++latIndex) {
    float phi = latIndex * (float(M_PI) / float(stackCount));
    float v = float(latIndex) / float(stackCount);

    for (UINT lonIndex = 0; lonIndex <= sliceCount; ++lonIndex) {
      float theta = lonIndex * (2.0f * float(M_PI) / float(sliceCount));
      float u = float(lonIndex) / float(sliceCount);

      float x = radius * sinf(phi) * cosf(theta);
      float y = radius * cosf(phi);
      float z = radius * sinf(phi) * sinf(theta);

      mVertices.push_back({Vector4(x, y, z, 1), Vector2(u, v)});
    }
  }

  // 下極点
  mVertices.push_back({Vector4(0, -radius, 0, 1), Vector2(0.0f, 1.0f)});

  // --- インデックス生成 ---
  // （この部分はそのままでOK）
  for (UINT i = 1; i <= sliceCount; ++i) {
    mIndices.push_back(0);
    mIndices.push_back(i);
    mIndices.push_back(i + 1);
  }

  UINT ringVerts = sliceCount + 1;
  for (UINT i = 0; i < stackCount - 2; ++i) {
    for (UINT j = 0; j < sliceCount; ++j) {
      UINT a = 1 + i * ringVerts + j;
      UINT b = 1 + i * ringVerts + j + 1;
      UINT c = 1 + (i + 1) * ringVerts + j;
      UINT d = 1 + (i + 1) * ringVerts + j + 1;
      mIndices.push_back(a);
      mIndices.push_back(b);
      mIndices.push_back(d);
      mIndices.push_back(a);
      mIndices.push_back(d);
      mIndices.push_back(c);
    }
  }

  UINT southPoleIndex = UINT(mVertices.size() - 1);
  UINT baseIndex = southPoleIndex - ringVerts;
  for (UINT i = 0; i < sliceCount; ++i) {
    mIndices.push_back(southPoleIndex);
    mIndices.push_back(baseIndex + i + 1);
    mIndices.push_back(baseIndex + i);
  }
}

void Sphere::Draw(ID3D12GraphicsCommandList *cmdList) {
  // 頂点／インデックスバッファを設定
  cmdList->IASetVertexBuffers(0, 1, &mVBView);
  cmdList->IASetIndexBuffer(&mIBView);
  // トポロジを確認（PSO側で三角形指定済みなら不要ですが明示的に）
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  // インデックス描画
  cmdList->DrawIndexedInstanced(mIndexCount, 1, 0, 0, 0);
}
