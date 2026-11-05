#pragma once

#include <mutex>
#include <xaudio2.h>

#include "VoiceCallback.h"

struct FSoundData
{
	TArray<uint8> FormatBytes;
	TArray<uint8> Samples;

	const WAVEFORMATEX* GetFormat() const
	{
		return reinterpret_cast<const WAVEFORMATEX*>(FormatBytes.GetData());
	}
};

class FSoundSystem final
{
	friend class FVoiceCallback;

public:
	static FSoundSystem& Get()
	{
		static FSoundSystem Instance;
		return Instance;
	}

	FSoundSystem(const FSoundSystem&) = delete;
	FSoundSystem& operator=(const FSoundSystem&) = delete;
	FSoundSystem(FSoundSystem&&) = delete;
	FSoundSystem& operator=(FSoundSystem&&) = delete;

	bool Initialize();
	void Shutdown();
	void PlaySound(const FString& Path, float Volume = 1.0f);
	void PlayMusic(const FString& Path, float Volume = 1.0f);
	void StopMusic();

private:
	FSoundSystem() = default;
	~FSoundSystem() = default;

	const FSoundData* FindOrLoad(const FString& Path);
	void ReclaimFinishedVoices();

	FVoiceCallback VoiceCallback;

	bool bCOMInitialized = false;

	IXAudio2* XAudio = nullptr;
	IXAudio2MasteringVoice* MasterVoice = nullptr;

	TArray<IXAudio2SourceVoice*> ActiveVoices;
	TArray<IXAudio2SourceVoice*> FinishedVoices;

	IXAudio2SourceVoice* MusicVoice = nullptr;
	FString CurrentMusicPath;

	TMap<FString, TUniquePtr<FSoundData>> SoundCache;
	std::mutex FinishedMutex;
};
