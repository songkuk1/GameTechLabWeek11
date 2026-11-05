#pragma once

#include "MovieScenePropertyTrack.h"

class UMovieSceneFadeTrack : public UMovieScenePropertyTrack
{
	DECLARE_CLASS(UMovieSceneFadeTrack, UMovieScenePropertyTrack)

protected:
	UMovieSceneSection* CreateNewSection() override;
};