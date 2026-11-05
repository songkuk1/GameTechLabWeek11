#pragma once

#include "MovieSceneSection.h"
#include "MovieSceneFloatChannel.h"

class UMovieSceneFadeSection : public UMovieSceneSection
{
	DECLARE_CLASS(UMovieSceneFadeSection, UMovieSceneSection)

public:
	UMovieSceneFadeSection();

	virtual void Serialize(FStructuredArchive::FRecord Record) override;

	bool EvaluateFadeCurve(const FFrameTime& InTime, float& OutFadeCurve);

	void SetFadeCurve(const FMovieSceneFloatChannel& InChannel)
	{
		FadeCurve = InChannel;
	}

	FMovieSceneFloatChannel& GetFadeCurve()
	{
		return FadeCurve;
	}

	void SetFadeColor(const FLinearColor& InFadeColor)
	{
		FadeColor = InFadeColor;
	}

	FLinearColor& GetFadeColor()
	{
		return FadeColor;
	}

private:
	FMovieSceneFloatChannel FadeCurve;
	FLinearColor FadeColor;
};