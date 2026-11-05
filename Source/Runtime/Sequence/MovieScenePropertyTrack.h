#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Containers/Array.h"

class UMovieSceneSection;

class UMovieScenePropertyTrack : public UObject
{
	DECLARE_CLASS(UMovieScenePropertyTrack, UObject)

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
