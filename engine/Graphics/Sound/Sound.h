#pragma once
#include <cstdint>
#include <windows.h>
#include <wrl/client.h>
#include <xaudio2.h>

using Microsoft::WRL::ComPtr;

#pragma comment(lib, "xaudio2.lib")

struct ChunkHeader {
  char id[4];   // チャンクの識別子
  int32_t size; // チャンクのサイズ
};

struct RiffHeader {
  ChunkHeader chunk; // チャンクヘッダー
  char type[4];      // RIFFのタイプ（例: "WAVE"）
};

struct FormatChunk {
  ChunkHeader chunk; // チャンクヘッダー
  WAVEFORMATEX fmt;
};

struct SoundData {
  WAVEFORMATEX wfex;
  BYTE *pBuffer;           // 音声データのバッファ
  unsigned int bufferSize; // バッファのサイズ
};

SoundData SoundLoadWave(const char *filename);

void SoundUnload(SoundData *soundData);

IXAudio2SourceVoice *SoundPlayWave(IXAudio2 *xaudio2,
                                   const SoundData &soundData, float volume,
                                   bool loop);

void SoundStopWave(IXAudio2SourceVoice *pSourceVoice);

class Sound {

public:
  Sound();
  ~Sound();

  void Initialize(const char *filename);

  void SoundImGui(const char *soundname);

  void SetVolume(float volume); // 音量設定用メソッド追加
  float GetVolume() const;      // 音量取得用メソッド追加

private:
  ComPtr<IXAudio2> xAudio2;
  IXAudio2MasteringVoice *masteringVoice = nullptr;
  IXAudio2SourceVoice *voice = nullptr;
  SoundData soundData;

  bool isLoop = false;
  float volume = 1.0f;
};
