#pragma once

#include "Asset/Asset.h"
#include "MovieSceneSequence.generated.h"

class UMovieScene;

UCLASS()
class UMovieSceneSequence : public UAsset
{
	GENERATED_BODY()

public:
	virtual ~UMovieSceneSequence() = default;

	virtual UMovieScene* GetMovieScene() const;
};
