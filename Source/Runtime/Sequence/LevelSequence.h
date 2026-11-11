#pragma once

#include "MovieSceneSequence.h"
#include "LevelSequence.generated.h"

UCLASS()
class ULevelSequence : public UMovieSceneSequence
{
	GENERATED_BODY()

public:
	void Initialize(UMovieScene* InScene);

	virtual void Serialize(FStructuredArchive::FRecord Record) override;


	virtual UMovieScene* GetMovieScene() const override;

private:
	UMovieScene* MovieScene;
};
