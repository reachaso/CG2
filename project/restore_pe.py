import sys
import os

path = 'c:/Users/rea/source/repos/2025/Chaso/Chaso/project/Application/Editor/EditorManager.cpp'
with open(path, 'r', encoding='utf-8') as f:
    lines = f.readlines()

new_lines = []
for i, line in enumerate(lines):
    if line.strip() == 'void EditorManager::DrawUI(D3D12_GPU_DESCRIPTOR_HANDLE viewportSrv, Dx12Core* core, float deltaTime, Scene* currentScene) {':
        new_lines.append('void EditorManager::DrawUI(D3D12_GPU_DESCRIPTOR_HANDLE viewportSrv, Dx12Core* core, PipelineManager* pm, float deltaTime, Scene* currentScene) {\n')
    else:
        new_lines.append(line)

insert_idx = -1
for i, line in enumerate(new_lines):
    if line.strip() == '// === Screenshot Pop-out ===':
        insert_idx = i
        break

ui_code = '''
  // ============================
  // Particle Editor パネル
  // ============================
  if (showParticleEditor_) {
    if (ImGui::Begin("Particle Editor Parameters", &showParticleEditor_)) {
      if (!peParticle_) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Particle Editor Preview");
        ImGui::Separator();
        ImGui::TextWrapped("Initialize the preview particle system to start editing.");
        if (ImGui::Button("Initialize Preview Particle", ImVec2(-1, 40))) {
          peParticle_ = std::make_unique<GPUParticle>();
          if (core) {
            // リソース初期化
            peRenderTexture_.Initialize(core, 512, 512, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

            // カメラ初期化
            peCamera_.Initialize(nullptr, {0.0f, 5.0f, -15.0f}, {0.0f, 0.0f, 0.0f}, 0.45f, 1.0f, 0.1f, 100.0f);

            SceneContext ctx{};
            ctx.core = core;
            ctx.pipelineManager = pm;
            peParticle_->Initialize(ctx);
            peParticle_->SetPreviewMode(true);
            peInitialized_ = true;
          }
        }
      } else {
        // --- パラメータ編集 UI ---
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), "Emitter Parameters");
        ImGui::Separator();

        // エミッタ形状
        const char* shapeNames[] = {"Point", "Sphere", "Box", "Cone"};
        int shapeInt = static_cast<int>(peParticle_->emitterShape_);
        if (ImGui::Combo("Emitter Shape", &shapeInt, shapeNames, IM_ARRAYSIZE(shapeNames))) {
          peParticle_->emitterShape_ = static_cast<EmitterShape>(shapeInt);
        }

        // 形状別パラメータ
        if (peParticle_->emitterShape_ == EmitterShape::Sphere || peParticle_->emitterShape_ == EmitterShape::Cone) {
          ImGui::DragFloat("Shape Radius", &peParticle_->shapeRadius_, 0.1f, 0.0f, 50.0f);
        }
        if (peParticle_->emitterShape_ == EmitterShape::Cone) {
          float angleDeg = peParticle_->coneAngle_ * 180.0f / 3.14159265f;
          if (ImGui::DragFloat("Cone Angle (deg)", &angleDeg, 1.0f, 0.0f, 90.0f)) {
            peParticle_->coneAngle_ = angleDeg * 3.14159265f / 180.0f;
          }
        }
        if (peParticle_->emitterShape_ == EmitterShape::Box) {
          ImGui::DragFloat3("Box Size", &peParticle_->shapeBoxSize_.x, 0.1f, 0.0f, 50.0f);
        }

        ImGui::Separator();
        ImGui::Text("Emission");
        int emit = static_cast<int>(peParticle_->GetEmitCount());
        if (ImGui::SliderInt("Emit Count", &emit, 0, 100)) {
          peParticle_->SetEmitCount(static_cast<uint32_t>(emit));
        }
        int maxP = static_cast<int>(peParticle_->GetMaxParticles());
        if (ImGui::SliderInt("Max Particles", &maxP, 256, 16384)) {
          peParticle_->SetMaxParticles(static_cast<uint32_t>(maxP));
        }

        ImGui::Separator();
        ImGui::Text("Lifetime & Scale");
        ImGui::DragFloat("Min Lifetime", &peParticle_->minLifeTime_, 0.1f, 0.1f, 30.0f);
        ImGui::DragFloat("Max Lifetime", &peParticle_->maxLifeTime_, 0.1f, 0.1f, 30.0f);
        ImGui::DragFloat("Min Scale", &peParticle_->minScale_, 0.01f, 0.01f, 5.0f);
        ImGui::DragFloat("Max Scale", &peParticle_->maxScale_, 0.01f, 0.01f, 5.0f);

        ImGui::Separator();
        ImGui::Text("Velocity & Gravity");
        ImGui::DragFloat3("Base Velocity", &peParticle_->baseVelocity_.x, 0.001f);
        ImGui::DragFloat("Velocity Variance", &peParticle_->velocityVariance_, 0.001f, 0.0f, 1.0f);
        ImGui::DragFloat("Gravity", &peParticle_->gravity_, 0.01f, 0.0f, 20.0f);

        ImGui::Separator();
        ImGui::Text("Position");
        ImGui::DragFloat3("Emitter Position", &peParticle_->emitterPosition_.x, 0.1f);

        ImGui::Separator();
        ImGui::Text("Color");
        ImGui::ColorEdit4("Start Color", &peParticle_->startColor_.x);
        ImGui::ColorEdit4("End Color", &peParticle_->endColor_.x);

        // ParticleType と BlendMode
        ImGui::Separator();
        ImGui::Text("Rendering");
        const char* typeNames[] = {"Default", "Explosion", "Rain"};
        int currentTypeInt = static_cast<int>(peParticle_->GetParticleType());
        if (ImGui::Combo("Particle Type", &currentTypeInt, typeNames, IM_ARRAYSIZE(typeNames))) {
          peParticle_->SetParticleType(static_cast<ParticleType>(currentTypeInt));
        }
        const char* blendNames[] = {"None", "Normal", "Add", "Subtract", "Multiply", "Screen", "Premultiplied"};
        int blendInt = static_cast<int>(peParticle_->GetBlendMode());
        if (ImGui::Combo("Blend Mode", &blendInt, blendNames, IM_ARRAYSIZE(blendNames))) {
          peParticle_->SetBlendMode(static_cast<BlendMode>(blendInt));
        }

        // テクスチャ変更
        ImGui::Separator();
        ImGui::Text("Texture");
        ImGui::InputText("Texture Path", peTexPath_, sizeof(peTexPath_));
        ImGui::SameLine();
        if (ImGui::Button("Apply##Tex")) {
          peParticle_->SetTexture(std::string(peTexPath_));
        }

        // JSON Save / Load
        ImGui::Separator();
        ImGui::Text("File I/O");
        ImGui::InputText("JSON Path", peJsonPath_, sizeof(peJsonPath_));
        if (ImGui::Button("Save", ImVec2(100, 30))) {
          peParticle_->SaveToJson(std::string(peJsonPath_));
        }
        ImGui::SameLine();
        if (ImGui::Button("Load", ImVec2(100, 30))) {
          peParticle_->LoadFromJson(std::string(peJsonPath_));
          // テクスチャパスの同期
          #ifdef _MSC_VER
          strncpy_s(peTexPath_, sizeof(peTexPath_), peParticle_->texturePath_.c_str(), _TRUNCATE);
          #endif
        }
      }
    }
    ImGui::End();

    // プレビュー用ウィンドウ
    if (peInitialized_) {
      ImGui::SetNextWindowSizeConstraints(ImVec2(256, 256), ImVec2(2048, 2048));
      if (ImGui::Begin("Particle Editor Preview", &showParticleEditor_)) {
        ImVec2 contentSize = ImGui::GetContentRegionAvail();
        if (contentSize.x > 0 && contentSize.y > 0) {
          float size = std::min(contentSize.x, contentSize.y);
          // 中央に配置
          ImGui::SetCursorPos(ImVec2((contentSize.x - size) * 0.5f + ImGui::GetCursorPosX(), 
                                     (contentSize.y - size) * 0.5f + ImGui::GetCursorPosY()));
          ImGui::Image((ImTextureID)peRenderTexture_.GetSRVGPU().ptr, ImVec2(size, size));
        }
      }
      ImGui::End();
    }
  }
'''

new_lines.insert(insert_idx, ui_code + '\n')

render_preview_code = '''
void EditorManager::Term() {
  if (peParticle_) {
    peParticle_->Finalize();
    peParticle_.reset();
  }
  peRenderTexture_.Release();
  peInitialized_ = false;
}

void EditorManager::RenderParticlePreview(ID3D12GraphicsCommandList* cl, Dx12Core* core, PipelineManager* pm, float deltaTime) {
  if (!showParticleEditor_ || !peParticle_ || !peInitialized_) return;

  // RenderTargetへ状態遷移
  peRenderTexture_.TransitionToRenderTarget(cl);

  D3D12_CPU_DESCRIPTOR_HANDLE rtv = peRenderTexture_.GetRTV();
  cl->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

  // 背景のクリア
  const float clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f }; // 暗いグレー
  cl->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

  // ビューポートとシザー
  D3D12_VIEWPORT viewport{};
  viewport.Width = 512.0f;
  viewport.Height = 512.0f;
  viewport.MaxDepth = 1.0f;
  cl->RSSetViewports(1, &viewport);

  D3D12_RECT scissor{};
  scissor.right = 512;
  scissor.bottom = 512;
  cl->RSSetScissorRects(1, &scissor);

  // カメラ更新とSceneContext構築
  peCamera_.Update();
  SceneContext ctx{};
  ctx.core = core;
  ctx.pipelineManager = pm;
  ctx.camera = &peCamera_;
  ctx.deltaTime = deltaTime;

  // パーティクルの更新と描画
  peParticle_->Update(peCamera_.GetView(), peCamera_.GetProjection(), deltaTime);
  peParticle_->Render(ctx, cl);

  // SRVへ状態遷移（ImGuiで描画するため）
  peRenderTexture_.TransitionToShaderResource(cl);
}
'''

final_lines = []
skip = False
for line in new_lines:
    if line.startswith('void EditorManager::Term()'):
        skip = True
    if skip and line == '}\n':
        skip = False
        continue
    if not skip:
        final_lines.append(line)

final_lines.append('\n' + render_preview_code)

with open(path, 'w', encoding='utf-8') as f:
    f.writelines(final_lines)
