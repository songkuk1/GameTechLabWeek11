#include "EnginePCH.h"
#include "MovieScene.h"
#include "MovieScenePropertyTrack.h"
#include "MovieSceneCameraCutTrack.h"
#include "Serialization/SequenceSerialization.h"

void UMovieScene::SetCameraCutTrack(UMovieSceneCameraCutTrack* InTrack)
{
	CameraCutTrack = InTrack;
}

UMovieSceneCameraCutTrack* UMovieScene::GetCameraCutTrack()
{
	return CameraCutTrack;
}

void UMovieScene::Initialize(float InDuration, int32 InFrameCount)
{
	CameraCutTrack = nullptr;
	Duration = InDuration;
	FrameCount = InFrameCount;
}

void UMovieScene::AddBinding(const FMovieSceneBinding& Binding)
{
	ObjectBindings.Add(Binding);
}

void UMovieScene::RemoveBinding(const FString& ObjectName)
{
	for (int32 i = 0; i < ObjectBindings.Num(); i++)
	{
		FMovieSceneBinding& Binding = ObjectBindings[i];

		if (Binding.GetObjectName() == ObjectName)
		{
			ObjectBindings.RemoveAtSwap(i);
			return;
		}
	}
}

FMovieSceneBinding* UMovieScene::FindBinding(const FString& ObjectName)
{
	// TODO: 맵 조회로 최적화 가능성 있음.
	for (int32 i = 0; i < ObjectBindings.Num(); i++)
	{
		FMovieSceneBinding& Binding = ObjectBindings[i];

		if (Binding.GetObjectName() == ObjectName)
		{
			return &Binding;
		}
	}

	return nullptr;
}

void UMovieScene::AddTrack(UMovieScenePropertyTrack* InTrack)
{
	Tracks.Add(InTrack);
}

const TArray<FMovieSceneBinding>& UMovieScene::GetBindings() const
{
	return ObjectBindings;
}

const TArray<UMovieScenePropertyTrack*>& UMovieScene::GetTracks() const
{
	return Tracks;
}

float UMovieScene::GetDuration() const
{
	return Duration;
}

int32 UMovieScene::GetFrameCount()
{
	return FrameCount;
}

void UMovieScene::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);

	FArchive& Archive = Record.GetUnderlyingArchive();

	int32 BindingNum = ObjectBindings.Num();
	FStructuredArchive::FArray BindingArray = Record.EnterArray("Bindings", BindingNum);

	if (Archive.IsLoading())
	{
		int32 PrevNum = ObjectBindings.Num();

		int32 i;
		for (i = 0; i < BindingNum; i++)
		{
			FMovieSceneBinding* NewBinding = nullptr;
			if (i < ObjectBindings.Num())
			{
				NewBinding = &ObjectBindings[i];
			}
			else
			{
				NewBinding = &ObjectBindings.AddDefaulted_GetRef();
			}

			BindingArray.EnterElement() << *NewBinding;
		}

		ObjectBindings.SetNum(BindingNum);
	}
	else
	{
		for (int32 i = 0; i < BindingNum; i++)
		{
			BindingArray.EnterElement() << ObjectBindings[i];
		}
	}

	int32 TrackNum = Tracks.Num();
	FStructuredArchive::FArray TrackArray = Record.EnterArray("Tracks", TrackNum);

	if (Archive.IsLoading())
	{
		int32 PrevNum = Tracks.Num();

		int32 i;
		for (i = 0; i < TrackNum; i++)
		{
			FStructuredArchiveRecord Record = TrackArray.EnterElement().EnterRecord();

			FString ClssName;
			Record << SA_VALUE("ClassName", ClssName);

			UClass* Clss = FindClass(ClssName);

			UMovieScenePropertyTrack* NewTrack = nullptr;
			if (i < Tracks.Num())
			{
				NewTrack = Tracks[i];
			}
			else
			{
				NewTrack = NewObject<UMovieScenePropertyTrack>(this, Clss);
				Tracks.Add(NewTrack);
			}

			NewTrack->Serialize(Record);
		}

		for (; i < PrevNum; i++)
		{
			delete Tracks[i];
		}
		Tracks.SetNum(TrackNum);
	}
	else
	{
		for (int32 i = 0; i < TrackNum; i++)
		{
			auto* Track = Tracks[i];

			FStructuredArchiveRecord Record = TrackArray.EnterElement().EnterRecord();

			Record << SA_VALUE("ClassName", Track->GetClass()->Name);
			Track->Serialize(Record);
		}
	}

	bool bHasCameraCutTrack = CameraCutTrack;
	Record << SA_VALUE("HasCameraCutTrack", bHasCameraCutTrack);

	if (bHasCameraCutTrack)
	{
		if (!CameraCutTrack)
		{
			CameraCutTrack = NewObject<UMovieSceneCameraCutTrack>(this);
		}

		CameraCutTrack->Serialize(Record.EnterRecord("CameraCutTrack"));
	}
	else if(CameraCutTrack)
	{
		delete CameraCutTrack;
		CameraCutTrack = nullptr;
	}

	Record << SA_VALUE("Duration", Duration);
	Record << SA_VALUE("FrameCount", FrameCount);
}
