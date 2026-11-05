#include "EnginePCH.h"
#include "MovieSceneFadeSection.h"
#include "Serialization/SequenceSerialization.h"
#include "Serialization/MathSerialization.h"

UMovieSceneFadeSection::UMovieSceneFadeSection()
{
	FadeCurve.SetDefault(0.f);
}

void UMovieSceneFadeSection::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);

	Record << SA_VALUE("FadeCurve", FadeCurve);
	Record << SA_VALUE("FadeColor", FadeColor);
}

bool UMovieSceneFadeSection::EvaluateFadeCurve(const FFrameTime& InTime, float& OutFadeCurve)
{
	return FadeCurve.Evaluate(InTime, OutFadeCurve);
}
