#include "EnginePCH.h"
#include "MovieScene3DTransformSection.h"
#include "Serialization/SequenceSerialization.h"
#include <format>

UMovieScene3DTransformSection::UMovieScene3DTransformSection()
{
	Translation[0].SetDefault(0.f);
	Translation[1].SetDefault(0.f);
	Translation[2].SetDefault(0.f);

	Scale[0].SetDefault(1.f);
	Scale[1].SetDefault(1.f);
	Scale[2].SetDefault(1.f);

	Rotation[0].SetDefault(0.f);
	Rotation[1].SetDefault(0.f);
	Rotation[2].SetDefault(0.f);
}

bool UMovieScene3DTransformSection::EvaluateTransform(const FFrameTime& InTime, FTransform& OutTransform)
{
	struct FTransformColumnElement
	{
		float* Translate;
		float* Scale;
		float* Rotation;
	};

	FTransformColumnElement Elements[3] = {
		{ &OutTransform.Location.X, &OutTransform.Scale.X, &OutTransform.Rotation.Pitch },
		{ &OutTransform.Location.Y, &OutTransform.Scale.Y, &OutTransform.Rotation.Yaw },
		{ &OutTransform.Location.Z, &OutTransform.Scale.Z, &OutTransform.Rotation.Roll },
	};

	for (int32 i = 0; i < 3; i++)
	{
		const FTransformColumnElement& Element = Elements[i];

		FMovieSceneFloatChannel& ChannelT = Translation[i];
		FMovieSceneFloatChannel& ChannelS = Scale[i];
		FMovieSceneFloatChannel& ChannelR = Rotation[i];

		ChannelT.Evaluate(InTime, *Element.Translate);
		ChannelS.Evaluate(InTime, *Element.Scale);
		ChannelR.Evaluate(InTime, *Element.Rotation);
	}

	return true;
}

FMovieSceneFloatChannel* UMovieScene3DTransformSection::GetTranslationChannel()
{
	return Translation;
}

FMovieSceneFloatChannel* UMovieScene3DTransformSection::GetScaleChannel()
{
	return Scale;
}

FMovieSceneFloatChannel* UMovieScene3DTransformSection::GetRotationChannel()
{
	return Rotation;
}

void UMovieScene3DTransformSection::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);

	for (int32 i = 0; i < 3; i++)
	{
		FString TranslationStr = std::format("Translation{}", i);
		FString ScaleStr = std::format("Scale{}", i);
		FString RotationStr = std::format("Rotation{}", i);

		Record << SA_VALUE(TranslationStr.c_str(), Translation[i]);
		Record << SA_VALUE(ScaleStr.c_str(), Scale[i]);
		Record << SA_VALUE(RotationStr.c_str(), Rotation[i]);
	}
}
