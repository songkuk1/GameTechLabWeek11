#pragma once

#include "MovieSceneSequence.h"

class ULevelSequence : public UMovieSceneSequence
{
	DECLARE_CLASS(ULevelSequence, UMovieSceneSequence)

public:
	void Initialize(UMovieScene* InScene);

	virtual void Serialize(FStructuredArchive::FRecord Record) override;


	virtual UMovieScene* GetMovieScene() const override;

private:
	UMovieScene* MovieScene;
};
