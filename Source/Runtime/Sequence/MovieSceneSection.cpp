#include "EnginePCH.h"
#include "MovieSceneSection.h"
#include "Serialization/SequenceSerialization.h"

void UMovieSceneSection::SetRange(const FFloatRange& InRange)
{
	SectionRange = InRange;
}

const FFloatRange& UMovieSceneSection::GetRange()
{
	return SectionRange;
}

void UMovieSceneSection::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);

	Record << SA_VALUE("SectionRange", SectionRange);
}
