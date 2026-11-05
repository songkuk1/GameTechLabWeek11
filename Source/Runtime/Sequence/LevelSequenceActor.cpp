#include "EnginePCH.h"
#include "LevelSequenceActor.h"
#include "LevelSceneSequencePlayer.h"
#include "Engine/Level.h"

ALevelSequenceActor::ALevelSequenceActor()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ALevelSequenceActor::BeginPlay()
{
	Super::BeginPlay();

	if (!Sequence)
	{
		return;
	}

	Player = NewObject<ULevelSceneSequencePlayer>(this);
	Player->Initialize(Sequence, bKeepState ? EMovieSceneCompletionMode::KeepState : EMovieSceneCompletionMode::RestoreState, GetLevel());

	if (bPlayOnBegin)
	{
		Player->Play();
	}
}

void ALevelSequenceActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!Player)
	{
		return;
	}

	Player->Update(DeltaTime);
}

void ALevelSequenceActor::SetSequence(ULevelSequence* InSequence)
{
	Sequence = InSequence;
}

ULevelSequence* ALevelSequenceActor::GetSequence()
{
	return Sequence;
}
