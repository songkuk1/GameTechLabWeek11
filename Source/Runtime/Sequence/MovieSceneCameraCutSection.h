#pragma once

#include "MovieSceneSection.h"

class UMovieSceneCameraCutSection : public UMovieSceneSection
{
	DECLARE_CLASS(UMovieSceneCameraCutSection, UMovieSceneSection)

public:
	virtual void Serialize(FStructuredArchive::FRecord Record) override;

	void SetCameraName(const FString& InCameraName);
	const FString& GetCameraName() const;

private:
	FString CameraName;
};
