#pragma once

#include "Core/EngineString.h"
#include "Containers/Array.h"

class FStructuredArchiveSlot;
class UMovieScenePropertyTrack;

struct FMovieSceneBinding
{
public:
	FMovieSceneBinding() = default;

	FMovieSceneBinding(const FString& InObjectName);

	void AddTrack(UMovieScenePropertyTrack* Track);

	void RemoveTrack(UMovieScenePropertyTrack* Track);

	const FString& GetObjectName() const;

	const TArray<UMovieScenePropertyTrack*>& GetTracks() const;

private:
	friend void operator<<(FStructuredArchiveSlot Slot, FMovieSceneBinding& V);

	FString ObjectName;
	TArray<UMovieScenePropertyTrack*> Tracks;
};
