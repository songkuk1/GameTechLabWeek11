#pragma once

#include "Serialization/StructuredArchiveSlots.h"
#include "Sequence/FloatRange.h"
#include "Sequence/MovieSceneFloatChannel.h"
#include "Sequence/MovieSceneBinding.h"
#include "Sequence/MovieScenePropertyTrack.h"

inline void operator<<(FStructuredArchiveSlot Slot, FFloatRange& V)
{
	FStructuredArchiveRecord Record = Slot.EnterRecord();
	Record << SA_VALUE("Min", V.Min);
	Record << SA_VALUE("Max", V.Max);
}

inline void operator<<(FStructuredArchiveSlot Slot, FMovieSceneFloatValue& V)
{
	FStructuredArchiveRecord Record = Slot.EnterRecord();

	FArchive& Archive = Record.GetUnderlyingArchive();

	if (Archive.IsSaving())
	{
		int8 InterpModeEnumToByte = static_cast<int8>(V.InterpMode);
		Record << SA_VALUE("InterpMode", InterpModeEnumToByte);
	}
	else
	{
		int8 InterpModeEnumToByte;
		Record << SA_VALUE("InterpMode", InterpModeEnumToByte);
		V.InterpMode = static_cast<ECurveInterpMode>(InterpModeEnumToByte);
	}

	Record << SA_VALUE("Value", V.Value);
}

inline void operator<<(FStructuredArchiveSlot Slot, FMovieSceneFloatChannel& V)
{
	FStructuredArchiveRecord Record = Slot.EnterRecord();

	FArchive& Archive = Record.GetUnderlyingArchive();

	int32 TimeNum = V.Times.Num();
	FStructuredArchive::FArray TimeArray = Record.EnterArray("Times", TimeNum);
	if (Archive.IsLoading())
	{
		V.Times.SetNum(TimeNum);
	}
	for (int32 i = 0; i < TimeNum; i++)
	{
		TimeArray.EnterElement() << V.Times[i];
	}

	int32 ValueNum = V.Values.Num();
	FStructuredArchive::FArray ValueArray = Record.EnterArray("Values", ValueNum);
	if (Archive.IsLoading())
	{
		V.Values.SetNum(ValueNum);
	}
	for (int32 i = 0; i < ValueNum; i++)
	{
		ValueArray.EnterElement() << V.Values[i];
	}

	Record << SA_VALUE("HasDefaultValue", V.bHasDefaultValue);
	Record << SA_VALUE("DefaultValue", V.DefaultValue);
}

inline void operator<<(FStructuredArchiveSlot Slot, FMovieSceneBinding& V)
{
	FStructuredArchiveRecord Record = Slot.EnterRecord();

	FArchive& Archive = Record.GetUnderlyingArchive();

	Record << SA_VALUE("ObjectName", V.ObjectName);

	int32 TrackNum = V.Tracks.Num();
	FStructuredArchive::FArray TrackArray = Record.EnterArray("Tracks", TrackNum);
	if (Archive.IsLoading())
	{
		V.Tracks.SetNum(TrackNum);

		for (int32 i = 0; i < TrackNum; i++)
		{
			FStructuredArchiveRecord Record = TrackArray.EnterElement().EnterRecord();

			FString ClssName;
			Record << SA_VALUE("ClassName", ClssName);

			UClass* Clss = FindClass(ClssName);
			if (!Clss || !Clss->IsChildOf(UMovieScenePropertyTrack::StaticClass()))
			{
				HTR_LOG(Warning, "Unknown track type");
				continue;
			}

			V.Tracks[i] = NewObject<UMovieScenePropertyTrack>(nullptr, Clss);
			V.Tracks[i]->Serialize(Record);
		}
	}
	else
	{
		for (int32 i = 0; i < TrackNum; i++)
		{
			auto* Track = V.Tracks[i];

			FStructuredArchiveRecord Record = TrackArray.EnterElement().EnterRecord();

			Record << SA_VALUE("ClassName", Track->GetClass()->Name);
			V.Tracks[i]->Serialize(Record);
		}
	}
}
