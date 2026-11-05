#include "EnginePCH.h"
#include "MovieScene3DTransformTrack.h"
#include "MovieScene3DTransformSection.h"

UMovieSceneSection* UMovieScene3DTransformTrack::CreateNewSection()
{
	return NewObject<UMovieScene3DTransformSection>(this, NAME_None);
}
