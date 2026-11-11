#pragma once

#include "MovieScenePropertyTrack.h"
#include "MovieSceneCameraCutTrack.generated.h"

UCLASS()
class UMovieSceneCameraCutTrack : public UMovieScenePropertyTrack
{
	GENERATED_BODY()

protected:
	virtual UMovieSceneSection* CreateNewSection() override;
};
