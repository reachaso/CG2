#include "Sound.h"
#include "../../externals/imgui/imgui.h"
#include "../../externals/imgui/imgui_impl_dx12.h"
#include "../../externals/imgui/imgui_impl_win32.h"
#include <cassert>
#include <fstream>

SoundData SoundLoadWave(const char *filename) {

  // HRESULT result;

  // ==========================
  // ファイルオープン
  // ==========================

  std::ifstream file;

  file.open(filename, std::ios_base::binary);

  assert(file.is_open());

  // ==========================
  // .wavデータの読み込み
  // ==========================

  RiffHeader riff;
  file.read((char *)&riff, sizeof(riff));

  if (strncmp(riff.chunk.id, "RIFF", 4) != 0) {
    assert(0);
  }

  if (strncmp(riff.type, "WAVE", 4) != 0) {
    assert(0);
  }

  FormatChunk format = {};

  file.read((char *)&format, sizeof(ChunkHeader));
  if (strncmp(format.chunk.id, "fmt ", 4) != 0) {
    assert(0);
  }

  assert(format.chunk.size <= sizeof(format.fmt));
  file.read((char *)&format.fmt, format.chunk.size);

  ChunkHeader data;
  file.read((char *)&data, sizeof(data));

  if (strncmp(data.id, "JUNK", 4) == 0) {
    // JUNK チャンクが存在する場合は読み飛ばす
    file.seekg(data.size, std::ios_base::cur);
    file.read((char *)&data, sizeof(data));
  }

  if (strncmp(data.id, "data", 4) != 0) {
    assert(0);
  }

  char *pBuffer = new char[data.size];
  file.read(pBuffer, data.size);

  file.close();

  // ==========================
  // 読み込んだ音声データをreturn
  // ==========================

  SoundData soundData = {};

  soundData.wfex = format.fmt;
  soundData.pBuffer = reinterpret_cast<BYTE *>(pBuffer);
  soundData.bufferSize = data.size;

  return soundData;
}

void SoundUnload(SoundData *soundData) {

  delete[] soundData->pBuffer;

  soundData->pBuffer = 0;
  soundData->bufferSize = 0;
  soundData->wfex = {};
}

IXAudio2SourceVoice *SoundPlayWave(IXAudio2 *xaudio2,
                                   const SoundData &soundData, bool loop) {
  HRESULT result;

  IXAudio2SourceVoice *pSourceVoice = nullptr;
  result = xaudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
  assert(SUCCEEDED(result));

  XAUDIO2_BUFFER buf{};
  buf.pAudioData = soundData.pBuffer;
  buf.AudioBytes = soundData.bufferSize;
  buf.Flags = XAUDIO2_END_OF_STREAM;
  buf.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

  result = pSourceVoice->SubmitSourceBuffer(&buf);
  assert(SUCCEEDED(result));
  result = pSourceVoice->Start();
  assert(SUCCEEDED(result));

  return pSourceVoice;
}

void SoundStopWave(IXAudio2SourceVoice *pSourceVoice) {
  if (pSourceVoice) {
    pSourceVoice->Stop();
    pSourceVoice->DestroyVoice();
  }
}

Sound::Sound() {
  // XAudio2の初期化
  HRESULT hr = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
  assert(SUCCEEDED(hr));
}

Sound::~Sound() {
  // 再生中なら停止
  if (voice) {
    SoundStopWave(voice);
    voice = nullptr;
  }
  // サウンドデータの解放
  SoundUnload(&soundData);
  // マスターボイスの解放
  if (masteringVoice) {
    masteringVoice->DestroyVoice();
    masteringVoice = nullptr;
  }
}

void Sound::Initialize(const char *filename) {

  if (voice) {
    SoundStopWave(voice);
    voice = nullptr;
  }
  SoundUnload(&soundData);
  if (!masteringVoice) {
    HRESULT hr = xAudio2->CreateMasteringVoice(&masteringVoice);
    assert(SUCCEEDED(hr));
  }
  soundData = SoundLoadWave(filename);
}

void Sound::SoundImGui(const char *soundname) {
  std::string Label = std::string(soundname);

  if (ImGui::CollapsingHeader(Label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Checkbox((std::string("ループ再生##") + Label).c_str(), &isLoop);

    if (ImGui::Button((std::string("再生##") + Label).c_str())) {
      voice = SoundPlayWave(xAudio2.Get(), soundData, isLoop);
    }

    ImGui::SameLine();

    if (ImGui::Button((std::string("停止##") + Label).c_str())) {
      SoundStopWave(voice);
      voice = nullptr;
    }
  }
}
