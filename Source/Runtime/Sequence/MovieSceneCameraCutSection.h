#pragma once

#include "MovieSceneSection.h"
#include "MovieSceneCameraCutSection.generated.h"

UCLASS()
class UMovieSceneCameraCutSection : public UMovieSceneSection
{
	GENERATED_BODY()

public:
	virtual void Serialize(FStructuredArchive::FRecord Record) override;

	void SetCameraName(const FString& InCameraName);
	const FString& GetCameraName() const;

private:
	FString CameraName;
};
