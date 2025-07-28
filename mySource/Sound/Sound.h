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
                                   const SoundData &soundData, bool loop);

void SoundStopWave(IXAudio2SourceVoice *pSourceVoice);

class Sound {

public:
  Sound();
  ~Sound();

  void Initialize(const char *filename);

  ComPtr<IXAudio2> xAudio2;
  IXAudio2MasteringVoice *masteringVoice = nullptr;

  void SoundImGui(const char *soundname);

private:
  bool isLoop = false;
  IXAudio2SourceVoice *voice = nullptr;
  SoundData soundData;
};
