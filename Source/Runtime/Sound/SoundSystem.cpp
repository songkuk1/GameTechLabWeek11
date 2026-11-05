#include "EnginePCH.h"
#include "SoundSystem.h"

#include <mutex>

namespace
{
	constexpr int32 MaxVoices = 32;

	TUniquePtr<FSoundData> LoadWav(const FString& Path);
}

bool FSoundSystem::Initialize()
{
	HRESULT Result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(Result))
	{
		Shutdown();
		return false;
	}
	bCOMInitialized = true;

	Result = XAudio2Create(&XAudio);
	if (FAILED(Result))
	{
		Shutdown();
		return false;
	}

	Result = XAudio->CreateMasteringVoice(&MasterVoice);
	if (FAILED(Result))
	{
		Shutdown();
		return false;
	}

	return true;
}

void FSoundSystem::Shutdown()
{
	StopMusic();
	for (IXAudio2SourceVoice* Voice : ActiveVoices)
	{
		Voice->DestroyVoice();
	}
	ActiveVoices.Reset();
	FinishedVoices.Reset();
	if (MasterVoice)
	{
		MasterVoice->DestroyVoice();
		MasterVoice = nullptr;
	}
	if (XAudio)
	{
		XAudio->Release();
		XAudio = nullptr;
	}
	SoundCache.Reset();
	if (bCOMInitialized)
	{
		CoUninitialize();
		bCOMInitialized = false;
	}
}

void FSoundSystem::PlaySound(const FString& Path, float Volume)
{
	if (!XAudio)
	{
		return;
	}
	ReclaimFinishedVoices();

	if (ActiveVoices.Num() >= MaxVoices)
	{
		return;
	}

	const FSoundData* Data = FindOrLoad(Path);
	if (!Data)
	{
		return;
	}

	IXAudio2SourceVoice* Voice = nullptr;
	if (FAILED(XAudio->CreateSourceVoice(&Voice, Data->GetFormat(), 0, XAUDIO2_DEFAULT_FREQ_RATIO, &VoiceCallback)))
	{
		return;
	}

	const XAUDIO2_BUFFER Buffer = {
		.Flags = XAUDIO2_END_OF_STREAM,
		.AudioBytes = static_cast<UINT32>(Data->Samples.Num()),
		.pAudioData = Data->Samples.GetData(),
		.pContext = Voice
	};

	Voice->SetVolume(Volume);
	HRESULT Result = Voice->SubmitSourceBuffer(&Buffer);
	if (FAILED(Result))
	{
		Voice->DestroyVoice();
		return;
	}
	Voice->Start();
	ActiveVoices.Add(Voice);
}

void FSoundSystem::PlayMusic(const FString& Path, float Volume)
{
	if (!XAudio)
	{
		return;
	}
	StopMusic();

	const FSoundData* Data = FindOrLoad(Path);
	if (!Data)
	{
		return;
	}

	if (FAILED(XAudio->CreateSourceVoice(&MusicVoice, Data->GetFormat())))
	{
		MusicVoice = nullptr;
		return;
	}

	const XAUDIO2_BUFFER Buffer = {
		.Flags = XAUDIO2_END_OF_STREAM,
		.AudioBytes = static_cast<UINT32>(Data->Samples.Num()),
		.pAudioData = Data->Samples.GetData(),
		.LoopCount = XAUDIO2_LOOP_INFINITE
	};

	MusicVoice->SetVolume(Volume);
	if (FAILED(MusicVoice->SubmitSourceBuffer(&Buffer)))
	{
		MusicVoice->DestroyVoice();
		MusicVoice = nullptr;
		return;
	}
	MusicVoice->Start();
	CurrentMusicPath = Path;
}

void FSoundSystem::StopMusic()
{
	if (!MusicVoice)
	{
		return;
	}
	MusicVoice->Stop();
	MusicVoice->DestroyVoice();
	MusicVoice = nullptr;
	CurrentMusicPath.clear();
}

const FSoundData* FSoundSystem::FindOrLoad(const FString& Path)
{
	if (auto* Found = SoundCache.Find(Path))
	{
		return Found->get();
	}

	SoundCache[Path] = LoadWav(Path);
	return SoundCache[Path].get();
}

void FSoundSystem::ReclaimFinishedVoices()
{
	TArray<IXAudio2SourceVoice*> Finished;
	{
		std::scoped_lock Lock(FinishedMutex);
		std::swap(Finished, FinishedVoices);
	}
	for (IXAudio2SourceVoice* Voice : Finished)
	{
		Voice->DestroyVoice();
		for (int32 i = 0; i < ActiveVoices.Num(); ++i)
		{
			if (ActiveVoices[i] == Voice)
			{
				ActiveVoices.RemoveAt(i);
				break;
			}
		}
	}
}

void STDMETHODCALLTYPE FVoiceCallback::OnBufferEnd(void* Context)
{
	std::lock_guard Lock(FSoundSystem::Get().FinishedMutex);
	FSoundSystem::Get().FinishedVoices.Add(static_cast<IXAudio2SourceVoice*>(Context));
}

namespace
{
	uint32 ReadU32(const uint8* Ptr)
	{
		uint32 Value = 0;
		std::memcpy(&Value, Ptr, sizeof(Value));
		return Value;
	}

	void CopyBytes(TArray<uint8>& Dest, const uint8* Src, uint32 Size)
	{
		Dest.SetNum(static_cast<int32>(Size));
		if (Size > 0)
		{
			std::memcpy(Dest.GetData(), Src, Size);
		}
	}

	bool IsSupportedFormat(const WAVEFORMATEX* Format)
	{
		return Format->wFormatTag == WAVE_FORMAT_PCM ||
			Format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
			Format->wFormatTag == WAVE_FORMAT_EXTENSIBLE;
	}

	TUniquePtr<FSoundData> LoadWav(const FString& Path)
	{
		std::ifstream File(Path, std::ios::binary | std::ios::ate);
		if (!File)
		{
			HTR_LOG(Warning, "[Sound] cannot open {}", Path);
			return nullptr;
		}

		const size_t FileSize = File.tellg();
		std::vector<uint8> Bytes(FileSize);
		File.seekg(0);
		File.read(reinterpret_cast<char*>(Bytes.data()), static_cast<std::streamsize>(FileSize));
		if (!File)
		{
			HTR_LOG(Warning, "[Sound] read failed {}", Path);
			return nullptr;
		}

		constexpr size_t RiffHeaderSize = 12;  // "RIFF" + Size + "WAVE"
		constexpr size_t ChunkHeaderSize = 8;  // Id + Size

		if (FileSize < RiffHeaderSize
			|| std::memcmp(Bytes.data(), "RIFF", 4) != 0
			|| std::memcmp(Bytes.data() + 8, "WAVE", 4) != 0)
		{
			HTR_LOG(Warning, "[Sound] not a RIFF/WAVE file {}", Path);
			return nullptr;
		}

		TUniquePtr<FSoundData> Data = MakeUnique<FSoundData>();
		bool bFoundFormat = false;
		bool bFoundData = false;

		size_t Offset = RiffHeaderSize;
		while (Offset + ChunkHeaderSize <= FileSize)
		{
			const uint8* ChunkId = Bytes.data() + Offset;
			const uint32 ChunkSize = ReadU32(ChunkId + 4);
			const uint8* ChunkData = ChunkId + ChunkHeaderSize;

			if (ChunkSize > FileSize - Offset - ChunkHeaderSize)
			{
				HTR_LOG(Warning, "[Sound] truncated chunk in {}", Path);
				return nullptr;
			}

			if (std::memcmp(ChunkId, "fmt ", 4) == 0)
			{
				CopyBytes(Data->FormatBytes, ChunkData, ChunkSize);
				if (Data->FormatBytes.Num() < static_cast<int32>(sizeof(WAVEFORMATEX)))
				{
					const int32 OldNum = Data->FormatBytes.Num();
					Data->FormatBytes.SetNum(static_cast<int32>(sizeof(WAVEFORMATEX)));
					std::memset(Data->FormatBytes.GetData() + OldNum, 0, sizeof(WAVEFORMATEX) - OldNum);
				}
				bFoundFormat = true;
			}
			else if (std::memcmp(ChunkId, "data", 4) == 0)
			{
				CopyBytes(Data->Samples, ChunkData, ChunkSize);
				bFoundData = true;
			}

			Offset += ChunkHeaderSize + ChunkSize + (ChunkSize & 1);
		}

		if (!bFoundFormat || !bFoundData)
		{
			HTR_LOG(Warning, "[Sound] missing fmt or data chunk in {}", Path);
			return nullptr;
		}

		if (!IsSupportedFormat(Data->GetFormat()))
		{
			HTR_LOG(Warning, "[Sound] unsupported format (tag {}) in {}", Data->GetFormat()->wFormatTag, Path);
			return nullptr;
		}

		return Data;
	}
}
