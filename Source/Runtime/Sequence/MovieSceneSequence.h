#pragma once

#include "Asset/Asset.h"

class UMovieScene;

class UMovieSceneSequence : public UAsset
{
	DECLARE_CLASS(UMovieSceneSequence, UAsset)

public:
	virtual ~UMovieSceneSequence() = default;

	virtual UMovieScene* GetMovieScene() const;
};
