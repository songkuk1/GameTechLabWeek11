#include "EnginePCH.h"
#include "MovieSceneBinding.h"

FMovieSceneBinding::FMovieSceneBinding(const FString& InObjectName)
	: ObjectName(InObjectName)
{
}

void FMovieSceneBinding::AddTrack(UMovieScenePropertyTrack* Track)
{
	Tracks.Add(Track);
}

void FMovieSceneBinding::RemoveTrack(UMovieScenePropertyTrack* Track)
{
	for (int32 i = 0; i < Tracks.Num(); i++)
	{
		if (Tracks[i] == Track)
		{
			Tracks.RemoveAtSwap(i);
			return;
		}
	}

	return;
}

const FString& FMovieSceneBinding::GetObjectName() const
{
	return ObjectName;
}

const TArray<UMovieScenePropertyTrack*>& FMovieSceneBinding::GetTracks() const
{
	return Tracks;
}
