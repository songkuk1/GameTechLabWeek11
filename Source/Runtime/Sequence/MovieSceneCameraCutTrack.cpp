#include "EnginePCH.h"
#include "MovieSceneCameraCutTrack.h"
#include "MovieSceneCameraCutSection.h"

UMovieSceneSection* UMovieSceneCameraCutTrack::CreateNewSection()
{
	return NewObject<UMovieSceneCameraCutSection>(this, NAME_None);
}
