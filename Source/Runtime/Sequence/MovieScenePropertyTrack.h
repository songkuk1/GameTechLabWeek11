#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Containers/Array.h"
#include "MovieScenePropertyTrack.generated.h"

class UMovieSceneSection;

UCLASS()
class UMovieScenePropertyTrack : public UObject
{
	GENERATED_BODY()

public:
	virtual ~UMovieScenePropertyTrack();

	virtual void Serialize(FStructuredArchive::FRecord Record) override;

	UMovieSceneSection* AddSection();

	void RemoveSection(UMovieSceneSection* Section);

	UMovieSceneSection* FindSection(int32 Time);

	const TArray<UMovieSceneSection*> GetAllSections();

protected:
	virtual UMovieSceneSection* CreateNewSection() = 0;

protected:
	TArray<UMovieSceneSection*> Sections;
};
