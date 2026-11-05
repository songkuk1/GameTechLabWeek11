#pragma once

#include "MovieScenePropertyTrack.h"

class UMovieScene3DTransformTrack : public UMovieScenePropertyTrack
{
	DECLARE_CLASS(UMovieScene3DTransformTrack, UMovieScenePropertyTrack)

protected:
	UMovieSceneSection* CreateNewSection() override;
};
