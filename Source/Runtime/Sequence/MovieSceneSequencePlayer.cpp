#include "EnginePCH.h"
#include "MovieSceneSequencePlayer.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "MovieScenePropertyTrack.h"
#include "MovieScene3DTransformSection.h"
#include "MovieSceneCameraCutSection.h"
#include "MovieSceneCameraCutTrack.h"
#include "MovieSceneFadeTrack.h"
#include "MovieSceneFadeSection.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Math/EngineMath.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

void UMovieSceneSequencePlayer::Initialize(UMovieSceneSequence* InSequence, EMovieSceneCompletionMode InCompletionMode)
{
	Sequence = InSequence;
	MovieScene = InSequence->GetMovieScene();
	CompletionMode = InCompletionMode;

	OriginalTransforms.Reset();

	bStoreOriginalViewTarget = false;
	OriginalViewTarget.clear();
}

void UMovieSceneSequencePlayer::Play()
{
	bIsPlaying = true;
	ElapsedTime = 0.f;
}

void UMovieSceneSequencePlayer::Pause()
{
}

void UMovieSceneSequencePlayer::Stop()
{
	bIsPlaying = false;

	if (CompletionMode == EMovieSceneCompletionMode::RestoreState)
	{
		RestoreState();
	}
}

void UMovieSceneSequencePlayer::Update(float DeltaTime)
{
	if (!bIsPlaying || !MovieScene)
	{
		return;
	}

	float NextTime = ElapsedTime + DeltaTime;
	if (NextTime > MovieScene->GetDuration())
	{
		SetPlaybackPosition(MovieScene->GetDuration());
		Stop();
	}
	else
	{
		SetPlaybackPosition(NextTime);
	}
}

void UMovieSceneSequencePlayer::SetPlaybackPosition(float InTime)
{
	if (!MovieScene)
	{
		return;
	}

	FFrameTime CurrentFrameTime = FFrameTime::FromDecimal((InTime / MovieScene->GetDuration()) * MovieScene->GetFrameCount());

	UWorld* World = GetPlaybackWorld();
	APlayerController* PC = World->GetPlayerController();

	for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
	{
		AActor* Actor = FindActor(Binding.GetObjectName());
		if (!Actor)
		{
			continue;
		}

		for (const auto& Track : Binding.GetTracks())
		{
			UMovieSceneSection* Section = Track->FindSection(CurrentFrameTime.FrameNumber);
			if (UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>(Section))
			{
				FTransform Transform = Actor->GetActorTransform();

				if (CompletionMode == EMovieSceneCompletionMode::RestoreState && !OriginalTransforms.Contains(Binding.GetObjectName()))
				{
					OriginalTransforms[Binding.GetObjectName()] = Transform;
				}

				if (TransformSection->EvaluateTransform(CurrentFrameTime, Transform))
				{
					Actor->SetActorTransform(Transform);
				}
			}
		}
	}

	for (UMovieScenePropertyTrack* Track : MovieScene->GetTracks())
	{
		UMovieSceneSection* Section = Track->FindSection(CurrentFrameTime.FrameNumber);
		if (UMovieSceneFadeSection* FadeSection = Cast<UMovieSceneFadeSection>(Section))
		{
			float FadeCurve;
			if (PC && PC->PlayerCameraManager && FadeSection->EvaluateFadeCurve(CurrentFrameTime, FadeCurve))
			{
				PC->PlayerCameraManager->SetManualCameraFade(FadeCurve, FadeSection->GetFadeColor());
			}
		}
	}

	UMovieSceneCameraCutTrack* CameraCutTrack = MovieScene->GetCameraCutTrack();
	if (CameraCutTrack)
	{
		UMovieSceneCameraCutSection* Section = Cast<UMovieSceneCameraCutSection>(CameraCutTrack->FindSection(CurrentFrameTime.FrameNumber));
		if (Section)
		{
			ACameraActor* CameraActor = Cast<ACameraActor>(FindActor(Section->GetCameraName()));
			if (CameraActor && PC)
			{
				AActor* PrevViewTarget = PC->GetViewTarget();
				if (CompletionMode == EMovieSceneCompletionMode::RestoreState && !bStoreOriginalViewTarget && PrevViewTarget)
				{
					OriginalViewTarget = PrevViewTarget->GetName();
					bStoreOriginalViewTarget = true;
				}

				PC->SetViewTarget(CameraActor);
			}
		}
	}

	ElapsedTime = FMath::Clamp(InTime, 0.f, MovieScene->GetDuration());
}

void UMovieSceneSequencePlayer::RestoreState()
{
	UWorld* World = GetPlaybackWorld();
	for (const auto& [ObjectName, Transform] : OriginalTransforms)
	{
		AActor* Actor = FindActor(ObjectName);
		if (!Actor)
		{
			continue;
		}

		Actor->SetActorTransform(Transform);
	}
	OriginalTransforms.Reset();

	if (bStoreOriginalViewTarget)
	{
		AActor* ViewTarget = Cast<AActor>(FindActor(OriginalViewTarget));
		APlayerController* PC = World->GetPlayerController();
		if (PC)
		{
			PC->SetViewTarget(ViewTarget);
		}
	}
	bStoreOriginalViewTarget = false;
}

float UMovieSceneSequencePlayer::GetElapsedTime() const
{
	return ElapsedTime;
}

AActor* UMovieSceneSequencePlayer::FindActor(const FString& ObjectName)
{
	UWorld* World = GetPlaybackWorld();
	if (!World || !World->GetCurrentLevel())
	{
		return nullptr;
	}

	for (AActor* Actor : World->GetCurrentLevel()->GetActors())
	{
		if (!Actor || Actor->GetName() != ObjectName)
		{
			continue;
		}

		return Actor;
	}

	return nullptr;
}
