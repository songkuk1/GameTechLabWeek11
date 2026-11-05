#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "FloatRange.h"

class UMovieSceneSection : public UObject
{
	DECLARE_CLASS(UMovieSceneSection, UObject)

public:
	virtual ~UMovieSceneSection() = default;

	virtual void Serialize(FStructuredArchive::FRecord Record) override;

	void SetRange(const FFloatRange& InRange);

	const FFloatRange& GetRange();

protected:
	FFloatRange SectionRange;
};
