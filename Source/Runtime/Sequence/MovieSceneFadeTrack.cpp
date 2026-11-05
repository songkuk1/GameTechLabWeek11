#include "EnginePCH.h"
#include "MovieSceneFadeTrack.h"
#include "MovieSceneFadeSection.h"

UMovieSceneSection* UMovieSceneFadeTrack::CreateNewSection()
{
    return NewObject<UMovieSceneFadeSection>(this, NAME_None);
}
