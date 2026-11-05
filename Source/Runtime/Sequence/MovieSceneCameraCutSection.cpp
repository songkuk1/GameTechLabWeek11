#include "EnginePCH.h"
#include "MovieSceneCameraCutSection.h"

void UMovieSceneCameraCutSection::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);

	Record << SA_VALUE("CameraName", CameraName);
}

void UMovieSceneCameraCutSection::SetCameraName(const FString& InCameraName)
{
	CameraName = InCameraName;
}

const FString& UMovieSceneCameraCutSection::GetCameraName() const
{
	return CameraName;
}
