#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "FloatRange.h"
#include "MovieSceneSection.generated.h"

UCLASS()
class UMovieSceneSection : public UObject
{
	GENERATED_BODY()

public:
	virtual ~UMovieSceneSection() = default;

	virtual void Serialize(FStructuredArchive::FRecord Record) override;

	void SetRange(const FFloatRange& InRange);

	const FFloatRange& GetRange();

protected:
	FFloatRange SectionRange;
};
