#pragma once

#include "GameFramework/Actor.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.generated.h"

class ULevelSceneSequencePlayer;

UCLASS()
class ALevelSequenceActor : public AActor
{
	GENERATED_BODY()


public:
	ALevelSequenceActor();

	void BeginPlay() override;

	void Tick(float DeltaTime) override;

	void SetSequence(ULevelSequence* InSequence);

	ULevelSequence* GetSequence();

private:
	UPROPERTY(EditAnywhere)
	ULevelSequence* Sequence = nullptr;
	ULevelSceneSequencePlayer* Player = nullptr;
	UPROPERTY(EditAnywhere)
	bool bPlayOnBegin = true;
	UPROPERTY(EditAnywhere)
	bool bKeepState = false;
};
