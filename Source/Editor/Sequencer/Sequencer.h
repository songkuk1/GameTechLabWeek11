#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "Editor/HitoriEd/EditorDragDrop.h"
#include "Sequence/LevelSequence.h"
#include "Sequence/LevelSceneSequencePlayer.h"
#include "Sequence/MovieScene.h"
#include "Sequence/MovieScene3DTransformSection.h"
#include "Sequence/MovieScene3DTransformTrack.h"
#include <vector>
#include <map>

class UMovieSceneCameraCutSection;
class UMovieSceneFadeSection;
class UMovieSceneFadeTrack;

class FLevelSequencePanel : public IEditorPanel
{
public:
	bool Init() override
	{
		return true;
	}

	void Tick(float DeltaTime) override;

	void OnRender() override;
	void OpenSequenceFile(const FString& Path);
	void CloseSequenceEditor();
	bool GetFadePreview(float& OutAmount, FLinearColor& OutColor) const;

	const char* GetPanelName() const override
	{
		return "Level Sequencer";
	}

private:
	AActor* FindActorForUI(uint32 ObjectUUID) const;
	AActor* FindActorForUI(const FString& ObjectName) const;
	void SelectBinding(const FString& ObjectName);
	void AddActor(uint32 ObjectUUID);
	void AddTransformTrack();
	bool DrawTrackContextMenu();
	void SaveSequenceFile();
	void LoadSequenceFile();
	void DrawCameraCutTimeline();
	void DrawCameraCutEditor();
	void AddCameraCutSection(const FString& CameraName);
	void SetCameraCutRange(UMovieSceneCameraCutSection* Cut, int32 Start, int32 End);
	UMovieSceneFadeTrack* FindFadeTrack() const;
	void AddFadeTrack();
	void SelectFadeSection(UMovieSceneFadeSection* FadeSection);
	void DrawFadeTimeline();
	void DrawFadeEditor();

private:
	FString SelectedActorName;
	UMovieScene3DTransformSection* Section = nullptr;
	UMovieScene3DTransformTrack* Track = nullptr;
	UMovieScene* MovieScene = nullptr;
	ULevelSequence* SampleSequence = nullptr;
	ULevelSceneSequencePlayer* SequencePlayer = nullptr;
	int32_t PreviewFrame = 0;
	bool bFollowPlayerTime = true;
	int SelectedChannel = 0;
	int KeyFrame = 0;
	float KeyValue = 0.0f;
	int KeyInterpolation = 1;
	int SelectedKeyChannel = -1;
	int32 SelectedKeyFrame = 0;
	std::vector<int32_t> TimelineFrames[9];
	int DragChannel = -1;
	int DragKeyIndex = -1;
	int32 DragStartFrame = 0;
	float DragStartMouseX = 0.0f;
	float DragPixelsPerFrame = 1.0f;
	std::map<FString, bool> ActorTrackOpen;
	std::map<FString, bool> TransformTrackOpen;
	char SequencePath[512] = {};
	FString OpenedSequencePath;
	FString FileStatus;
	bool bFileError = false;
	bool bSequenceDirty = false;
	bool bFocusRequested = false;
	bool bWasOpen = true;
	UMovieSceneCameraCutSection* SelectedCameraCut = nullptr;
	FString CameraCutError;
	bool bCameraCutDropTargetActive = false;
	UMovieSceneFadeSection* SelectedFadeSection = nullptr;
	bool bFadeKeySelected = false;
	int32 FadeKeyFrame = 0;
	float FadeKeyValue = 0.f;
	bool bDraggingFadeKey = false;
	int32 FadeDragTargetFrame = 0;
	float FadeDragMouseX = 0.f;
	float FadeDragFrameWidth = 1.f;
};
