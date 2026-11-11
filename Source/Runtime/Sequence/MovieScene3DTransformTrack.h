#pragma once

#include "MovieScenePropertyTrack.h"
#include "MovieScene3DTransformTrack.generated.h"

UCLASS()
class UMovieScene3DTransformTrack : public UMovieScenePropertyTrack
{
	GENERATED_BODY()

protected:
	UMovieSceneSection* CreateNewSection() override;
};
