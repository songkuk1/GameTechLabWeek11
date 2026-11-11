#pragma once

#include "MovieScenePropertyTrack.h"
#include "MovieSceneFadeTrack.generated.h"

UCLASS()
class UMovieSceneFadeTrack : public UMovieScenePropertyTrack
{
	GENERATED_BODY()

protected:
	UMovieSceneSection* CreateNewSection() override;
};