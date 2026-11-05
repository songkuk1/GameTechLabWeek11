#pragma once

#include "GameFramework/Actor.h"
#include "LevelSequence.h"

class ULevelSceneSequencePlayer;

class ALevelSequenceActor : public AActor
{
	DECLARE_CLASS(ALevelSequenceActor, AActor)

	REFLECT_START(ALevelSequenceActor)
		PROPERTY(Sequence)
		PROPERTY(bPlayOnBegin)
		PROPERTY(bKeepState)
	REFLECT_END()

public:
	ALevelSequenceActor();

	void BeginPlay() override;

	void Tick(float DeltaTime) override;

	void SetSequence(ULevelSequence* InSequence);

	ULevelSequence* GetSequence();

private:
	ULevelSequence* Sequence = nullptr;
	ULevelSceneSequencePlayer* Player = nullptr;
	bool bPlayOnBegin = true;
	bool bKeepState = false;
};
