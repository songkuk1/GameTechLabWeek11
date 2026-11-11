#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Containers/Map.h"
#include "Math/Transform.h"
#include "MovieSceneCompletionMode.h"
#include "MovieSceneSequencePlayer.generated.h"

class AActor;
class UMovieSceneSequence;
class UMovieScene;
class UWorld;

UCLASS()
class UMovieSceneSequencePlayer : public UObject
{
	GENERATED_BODY()

public:
	virtual ~UMovieSceneSequencePlayer() = default;

	void Initialize(UMovieSceneSequence* InSequence, EMovieSceneCompletionMode InCompletionMode);

	void Play();

	void Pause();

	void Stop();

	void Update(float DeltaTime);

	void SetPlaybackPosition(float InTime);

	void RestoreState();

	float GetElapsedTime() const;

	virtual UWorld* GetPlaybackWorld() const = 0;

private:
	// TODO: 일단 단순 구현
	AActor* FindActor(const FString& ObjectName);

private:
	UMovieSceneSequence* Sequence;
	UMovieScene* MovieScene;
	EMovieSceneCompletionMode CompletionMode;

	TMap<FString, FTransform> OriginalTransforms;

	bool bStoreOriginalViewTarget;
	FString OriginalViewTarget;

	bool bIsPlaying;
	float ElapsedTime;
};
