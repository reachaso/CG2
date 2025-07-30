#pragma once
#include "struct.h"
#include <d3d12.h>
#include <wrl.h>
using namespace Microsoft::WRL;


class Sprite {
private:	//メンバ変数
	//スプライトのデータ
	SpriteData sprite_;

	//GPUリソース
	ComPtr<ID3D12Resource> vertexBuffer_;
	ComPtr<ID3D12Resource> indexBuffer_;
	ComPtr<ID3D12Resource> matrixBuffer_;
	ComPtr<ID3D12Resource> materialBuffer_;

	VertexData* vertexData_ = nullptr;
	uint32_t* indexData_ = nullptr;
	Matrix4x4* matrixData_ = nullptr;
	Material* materialData_ = nullptr;

	D3D12_VERTEX_BUFFER_VIEW vbView_{};
	D3D12_INDEX_BUFFER_VIEW ibView_{};

public:		//メンバ関数
	Sprite(ID3D12Device* device);
	~Sprite();

	void Initialize(Vector3 position, Vector2 size);
	void Update();
	void Draw(ID3D12GraphicsCommandList* cmdList, D3D12_GPU_DESCRIPTOR_HANDLE textureHandle);

	// ImGuiで編集する用
	void ShowImGuiEditor();
};
