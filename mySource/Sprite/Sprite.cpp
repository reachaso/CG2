#include "Sprite.h"
#include "function/function.h"
#include "Math.h"
#include "Affine/Affine.h"
#include "../../externals/imgui/imgui.h"
#include "../../externals/imgui/imgui_impl_dx12.h"
#include "../../externals/imgui/imgui_impl_win32.h"

Sprite::Sprite(ID3D12Device* device) {
	sprite_.transform.scale = { 1.0f, 1.0f, 1.0f };
	sprite_.transform.rotation = { 0.0f, 0.0f, 0.0f };
	sprite_.transform.translation = { 0.0f, 0.0f, 0.0f };
	sprite_.uvTransform = {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

	//頂点・インデックスバッファ作成 → Mapして vertexData_, indexData_ に保持
	//それぞれビューの設定も
	vertexBuffer_ = CreateBufferResource(device, sizeof (VertexData) * 4);
	vertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
	vbView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
	vbView_.SizeInBytes = sizeof (VertexData) * 4;
	vbView_.StrideInBytes = sizeof (VertexData);

	indexBuffer_ = CreateBufferResource(device, sizeof (uint32_t) * 6);
	indexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));
	ibView_.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
	ibView_.SizeInBytes = sizeof (uint32_t) * 6;
	ibView_.Format = DXGI_FORMAT_R32_UINT;

	//行列・マテリアル用の定数バッファも作成 → Mapして material_, wvpMatrix_ に保持
	//初期設定まで済ませる
	matrixBuffer_ = CreateBufferResource(device, (sizeof (Matrix4x4) + 255) & ~255);
	matrixBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&matrixData_));
	*matrixData_ = MakeIdentity4x4();

	materialBuffer_ = CreateBufferResource(device, sizeof (Material));
	materialBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };	//初期カラーは白
	materialData_->enableLighting = false;
	materialData_->uvTransform = MakeIdentity4x4();
}

Sprite::~Sprite() {
	delete vertexData_;
	delete indexData_;
	delete matrixData_;
	delete materialData_;
}

void Sprite::Initialize(Vector3 position, Vector2 size) {
	//ワールド座標の設定
	sprite_.transform.scale = { 1.0f, 1.0f, 1.0f };
	sprite_.transform.rotation = { 0.0f, 0.0f, 0.0f };
	sprite_.transform.translation = position;

	//vertexData_に初期の四角形座標を書く（size_を使って計算）
	float w = size.x;
	float h = size.y;
	vertexData_[0].position = { 0.0f, h, 0.0f, 1.0f };      //左下
	vertexData_[0].texcoord = { 0.0f, 1.0f };
	vertexData_[0].normal = { 0.0f, 0.0f, -1.0f };

	vertexData_[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };   //左上
	vertexData_[1].texcoord = { 0.0f, 0.0f };
	vertexData_[1].normal = { 0.0f, 0.0f, -1.0f };

	vertexData_[2].position = { w, h, 0.0f, 1.0f };         //右下
	vertexData_[2].texcoord = { 1.0f, 1.0f };
	vertexData_[2].normal = { 0.0f, 0.0f, -1.0f };

	vertexData_[3].position = { w, 0.0f, 0.0f, 1.0f };      //右上
	vertexData_[3].texcoord = { 1.0f, 0.0f };
	vertexData_[3].normal = { 0.0f, 0.0f, -1.0f };

	//indexData_に書き込み
	indexData_[0] = 0;
	indexData_[1] = 1;
	indexData_[2] = 2;
	indexData_[3] = 1;
	indexData_[4] = 3;
	indexData_[5] = 2;
}

void Sprite::Update() {
	Matrix4x4 world = MakeAffineMatrix(sprite_.transform.scale, sprite_.transform.rotation, sprite_.transform.translation);
	Matrix4x4 view = MakeIdentity4x4();
	Matrix4x4 proj = MakeOrthographicMatrix(0, 0, (float)kClientWidth, (float)kClientHeight, 0, 100.0f);
	sprite_.wvpMatrix = Multiply(world, Multiply (view, proj));
	*matrixData_ = sprite_.wvpMatrix;

	// UVTransform更新
	materialData_->uvTransform = MakeAffineMatrix(sprite_.uvTransform.scale, sprite_.uvTransform.rotation, sprite_.uvTransform.translation);
}

void Sprite::Draw(ID3D12GraphicsCommandList* cmdList, D3D12_GPU_DESCRIPTOR_HANDLE textureHandle) {
	cmdList->IASetVertexBuffers(0, 1, &vbView_);   //BBVを設定
	cmdList->IASetIndexBuffer(&ibView_);	        //IBVを設定
	cmdList->SetGraphicsRootConstantBufferView(0, materialBuffer_->GetGPUVirtualAddress());
	cmdList->SetGraphicsRootConstantBufferView(1, matrixBuffer_->GetGPUVirtualAddress());
	cmdList->SetGraphicsRootDescriptorTable(2, textureHandle);
	//こいつでインデックスバッファを使った描画
	cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void Sprite::ShowImGuiEditor() {
	ImGui::DragFloat3("spriteScale", &sprite_.transform.scale.x, 0.01f);
	ImGui::DragFloat3("spriteRotate", &sprite_.transform.rotation.x, 0.01f);
	ImGui::DragFloat3("spriteTranslate", &sprite_.transform.translation.x, 1.0f);
	ImGui::DragFloat2("UVScale", &sprite_.uvTransform.scale.x, 0.01f, -10.0f, 10.0f);
	ImGui::DragFloat2("UVRotate", &sprite_.uvTransform.rotation.x, 0.01f, -10.0f, 10.0f);
	ImGui::DragFloat("UVTranslate", &sprite_.uvTransform.translation.z);
}
