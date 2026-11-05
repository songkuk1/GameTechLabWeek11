#pragma once

#include "MovieSceneSection.h"
#include "MovieSceneFloatChannel.h"
#include "Math/Transform.h"

class UMovieScene3DTransformSection : public UMovieSceneSection
{
	DECLARE_CLASS(UMovieScene3DTransformSection, UMovieSceneSection)

public:
	UMovieScene3DTransformSection();

	virtual void Serialize(FStructuredArchive::FRecord Record) override;

	bool EvaluateTransform(const FFrameTime& InTime, FTransform& OutTransform);

	FMovieSceneFloatChannel* GetTranslationChannel();

	FMovieSceneFloatChannel* GetScaleChannel();

	FMovieSceneFloatChannel* GetRotationChannel();

private:
	FMovieSceneFloatChannel Translation[3];
	FMovieSceneFloatChannel Scale[3];
	FMovieSceneFloatChannel Rotation[3];
};
