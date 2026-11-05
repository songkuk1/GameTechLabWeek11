#include "EnginePCH.h"
#include "MovieScenePropertyTrack.h"
#include "MovieSceneSection.h"

UMovieSceneSection* UMovieScenePropertyTrack::AddSection()
{
	UMovieSceneSection* NewSection = CreateNewSection();
	Sections.Add(NewSection);

	return NewSection;
}

void UMovieScenePropertyTrack::RemoveSection(UMovieSceneSection* Section)
{
	for (int32 i = 0; i < Sections.Num(); i++)
	{
		if (Sections[i] == Section)
		{
			Sections.RemoveAtSwap(i);
		}
		return;
	}

	return;
}

UMovieSceneSection* UMovieScenePropertyTrack::FindSection(int32 Time)
{
	for (UMovieSceneSection* Section : Sections)
	{
		const FFloatRange& Range = Section->GetRange();
		if (Time >= Range.Min && Time < Range.Max)
		{
			return Section;
		}
	}

	return nullptr;
}

const TArray<UMovieSceneSection*> UMovieScenePropertyTrack::GetAllSections()
{
	return Sections;
}

UMovieScenePropertyTrack::~UMovieScenePropertyTrack()
{
}

void UMovieScenePropertyTrack::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);

	FArchive& Archive = Record.GetUnderlyingArchive();

	int32 SectionNum = Sections.Num();
	FStructuredArchive::FArray SectionArray = Record.EnterArray("Sections", SectionNum);

	if (Archive.IsLoading())
	{
		int32 PrevNum = Sections.Num();

		int32 i;
		for (i = 0; i < SectionNum; i++)
		{
			UMovieSceneSection* NewSection = nullptr;
			if (i < Sections.Num())
			{
				NewSection = Sections[i];
			}
			else
			{
				NewSection = AddSection();
			}

			NewSection->Serialize(SectionArray.EnterElement().EnterRecord());
		}

		for (; i < PrevNum; i++)
		{
			delete Sections[i];
		}
		Sections.SetNum(SectionNum);
	}
	else
	{
		for (int32 i = 0; i < SectionNum; i++)
		{
			Sections[i]->Serialize(SectionArray.EnterElement().EnterRecord());
		}
	}
}
