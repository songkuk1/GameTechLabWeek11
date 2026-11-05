#pragma once

#include "MovieScenePropertyTrack.h"

class UMovieSceneCameraCutTrack : public UMovieScenePropertyTrack
{
	DECLARE_CLASS(UMovieSceneCameraCutTrack, UMovieScenePropertyTrack)

protected:
	virtual UMovieSceneSection* CreateNewSection() override;
};
