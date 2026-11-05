#include "EnginePCH.h"
#include "LevelSceneSequencePlayer.h"
#include "LevelSequence.h"
#include "Engine/Level.h"

void ULevelSceneSequencePlayer::Initialize(ULevelSequence* InSequence, EMovieSceneCompletionMode CompletionMode, ULevel* InLevel)
{
	World = InLevel->GetWorld();

	Super::Initialize(InSequence, CompletionMode);
}

UWorld* ULevelSceneSequencePlayer::GetPlaybackWorld() const
{
	return World;
}
