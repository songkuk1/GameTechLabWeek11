#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "MovieSceneBinding.h"
#include "MovieScene.generated.h"

class UMovieSceneCameraCutTrack;

UCLASS()
class UMovieScene : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(float InDuration, int32 InFrameCount);

	virtual void Serialize(FStructuredArchive::FRecord Record) override;

	void AddBinding(const FMovieSceneBinding& Binding);

	void RemoveBinding(const FString& ObjectName);

	FMovieSceneBinding* FindBinding(const FString& ObjectName);

	void AddTrack(UMovieScenePropertyTrack* InTrack);

	void SetCameraCutTrack(UMovieSceneCameraCutTrack* InTrack);

	const TArray<FMovieSceneBinding>& GetBindings() const;
	const TArray<UMovieScenePropertyTrack*>& GetTracks() const;
	UMovieSceneCameraCutTrack* GetCameraCutTrack();

	float GetDuration() const;

	int32 GetFrameCount();

private:
	TArray<FMovieSceneBinding> ObjectBindings;
	TArray<UMovieScenePropertyTrack*> Tracks;
	UMovieSceneCameraCutTrack* CameraCutTrack;

	float Duration;
	int32 FrameCount;
};
