#include "EnginePCH.h"
#include "Sequencer.h"
#include "Sequence/MovieSceneCameraCutSection.h"
#include "Sequence/MovieSceneCameraCutTrack.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Editor/HitoriEd/EditorEngine.h"
#include "imgui_neo_sequencer.h"
#include "imgui_internal.h"
#include "Serialization/JsonArchive.h"
#include "Camera/CameraActor.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <filesystem>

namespace
{
	void ReleasePanelSequence(ULevelSequence* Sequence)
	{
		if (!Sequence)
		{
			return;
		}
		UMovieScene* Scene = Sequence->GetMovieScene();
		if (Scene)
		{
			std::set<UMovieScenePropertyTrack*> Tracks;
			for (const FMovieSceneBinding& Binding : Scene->GetBindings())
			{
				for (UMovieScenePropertyTrack* Track : Binding.GetTracks())
				{
					Tracks.insert(Track);
				}
			}
			for (UMovieScenePropertyTrack* Track : Tracks)
			{
				delete Track;
			}
			delete Scene;
		}
		delete Sequence;
	}
}

void FLevelSequencePanel::OpenSequenceFile(const FString& Path)
{
	SetOpen(true);
	bFocusRequested = true;
	std::error_code Error;
	const std::filesystem::path AbsolutePath = std::filesystem::absolute(Path, Error);
	if (Error || Path.empty())
	{
		bFileError = true;
		FileStatus = "Could not open the sequence path.";
		return;
	}
	const FString FilePath = AbsolutePath.lexically_normal().generic_string();
	if (FilePath.size() >= sizeof(SequencePath))
	{
		bFileError = true;
		FileStatus = "The sequence path is too long.";
		return;
	}
	if (SampleSequence && FilePath == OpenedSequencePath)
	{
		return;
	}
	strncpy_s(SequencePath, sizeof(SequencePath), FilePath.c_str(), _TRUNCATE);
	LoadSequenceFile();
}

void FLevelSequencePanel::CloseSequenceEditor()
{
	if (SequencePlayer)
	{
		SequencePlayer->Stop();
		SequencePlayer->RestoreState();
	}
	bFollowPlayerTime = false;
	DragChannel = -1;
	DragKeyIndex = -1;
	bDraggingFadeKey = false;
	bWasOpen = false;
	SetOpen(false);
}

void FLevelSequencePanel::SaveSequenceFile()
{
	if (!bSequenceDirty)
	{
		return;
	}
	bFileError = true;
	try
	{
		if (SampleSequence && !OpenedSequencePath.empty() && FJsonArchive::SaveSequence(SampleSequence, OpenedSequencePath))
		{
			bFileError = false;
			bSequenceDirty = false;
			FileStatus = "Saved: " + OpenedSequencePath;
		}
		else
		{
			FileStatus = "Save failed. Check the path and destination folder.";
		}
	}
	catch (const std::exception& Error)
	{
		FileStatus = "Save failed: " + FString(Error.what());
	}
}

void FLevelSequencePanel::LoadSequenceFile()
{
	bFileError = true;
	UEditorEngine* Editor = Cast<UEditorEngine>(GEngine);
	UWorld* World = Editor ? Editor->GetEditorWorldContext().World() : nullptr;
	ULevel* Level = World ? World->GetCurrentLevel() : nullptr;
	if (!Level || SequencePath[0] == '\0')
	{
		FileStatus = "Load failed. A path and editor level are required.";
		return;
	}
	ULevelSequence* LoadedSequence = NewObject<ULevelSequence>(Editor);
	try
	{
		if (!FJsonArchive::LoadSequence(LoadedSequence, SequencePath))
		{
			ReleasePanelSequence(LoadedSequence);
			FileStatus = "Load failed. The current sequence was kept.";
			return;
		}
		UMovieScene* LoadedScene = LoadedSequence->GetMovieScene();
		if (!LoadedScene || !std::isfinite(LoadedScene->GetDuration()) || LoadedScene->GetDuration() <= 0.0f || LoadedScene->GetFrameCount() <= 0)
		{
			ReleasePanelSequence(LoadedSequence);
			FileStatus = "Load failed. Invalid duration or frame count.";
			return;
		}
	}
	catch (const std::exception& Error)
	{
		ReleasePanelSequence(LoadedSequence);
		FileStatus = "Load failed: " + FString(Error.what());
		return;
	}
	if (SequencePlayer)
	{
		SequencePlayer->Stop();
		SequencePlayer->RestoreState();
		delete SequencePlayer;
		SequencePlayer = nullptr;
	}
	Section = nullptr;
	Track = nullptr;
	ReleasePanelSequence(SampleSequence);
	SampleSequence = LoadedSequence;
	SelectedCameraCut = nullptr;
	SelectedFadeSection = nullptr;
	bFadeKeySelected = false;
	bDraggingFadeKey = false;
	CameraCutError.clear();
	OpenedSequencePath = SequencePath;
	bSequenceDirty = false;
	MovieScene = SampleSequence->GetMovieScene();
	SequencePlayer = NewObject<ULevelSceneSequencePlayer>(Editor);
	SequencePlayer->Initialize(SampleSequence, EMovieSceneCompletionMode::RestoreState, Level);
	SequencePlayer->Stop();
	PreviewFrame = 0;
	bFollowPlayerTime = false;
	SelectedActorName.clear();
	SelectedChannel = 0;
	SelectedKeyChannel = -1;
	SelectedKeyFrame = 0;
	KeyFrame = 0;
	KeyValue = 0.0f;
	KeyInterpolation = 1;
	DragChannel = -1;
	DragKeyIndex = -1;
	ActorTrackOpen.clear();
	TransformTrackOpen.clear();
	for (auto& Frames : TimelineFrames)
	{
		Frames.clear();
	}
	for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
	{
		SelectBinding(Binding.GetObjectName());
		if (Section)
		{
			break;
		}
	}
	bFileError = false;
	FileStatus = "Loaded: " + FString(SequencePath);
}

AActor* FLevelSequencePanel::FindActorForUI(uint32 ObjectUUID) const
{
	UEditorEngine* Editor = Cast<UEditorEngine>(GEngine);
	UWorld* World = Editor ? Editor->GetEditorWorldContext().World() : nullptr;
	ULevel* Level = World ? World->GetCurrentLevel() : nullptr;
	if (!Level)
	{
		return nullptr;
	}
	for (AActor* Actor : Level->GetActors())
	{
		if (Actor && Actor->GetUUID() == ObjectUUID)
		{
			return Actor;
		}
	}
	return nullptr;
}

AActor* FLevelSequencePanel::FindActorForUI(const FString& ObjectName) const
{
	UEditorEngine* Editor = Cast<UEditorEngine>(GEngine);
	UWorld* World = Editor ? Editor->GetEditorWorldContext().World() : nullptr;
	ULevel* Level = World ? World->GetCurrentLevel() : nullptr;
	if (!Level)
	{
		return nullptr;
	}
	for (AActor* Actor : Level->GetActors())
	{
		if (Actor && Actor->GetName() == ObjectName)
		{
			return Actor;
		}
	}
	return nullptr;
}

void FLevelSequencePanel::SelectBinding(const FString& ObjectName)
{
	SelectedCameraCut = nullptr;
	SelectedFadeSection = nullptr;
	bFadeKeySelected = false;
	bDraggingFadeKey = false;
	SelectedActorName = ObjectName;
	SelectedKeyChannel = -1;
	SelectedChannel = 0;
	DragChannel = -1;
	DragKeyIndex = -1;
	Track = nullptr;
	Section = nullptr;
	FMovieSceneBinding* Binding = MovieScene ? MovieScene->FindBinding(ObjectName) : nullptr;
	if (!Binding)
	{
		return;
	}
	for (UMovieScenePropertyTrack* Candidate : Binding->GetTracks())
	{
		UMovieScene3DTransformTrack* TransformTrack = Cast<UMovieScene3DTransformTrack>(Candidate);
		if (!TransformTrack)
		{
			continue;
		}
		for (UMovieSceneSection* CandidateSection : TransformTrack->GetAllSections())
		{
			if (UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>(CandidateSection))
			{
				Track = TransformTrack;
				Section = TransformSection;
				return;
			}
		}
	}
}

void FLevelSequencePanel::AddActor(uint32 ObjectUUID)
{
	AActor* Actor = FindActorForUI(ObjectUUID);
	if (!Actor)
	{
		return;
	}
	const FString ObjectName = Actor->GetName();
	if (MovieScene && MovieScene->FindBinding(ObjectName))
	{
		SelectBinding(ObjectName);
		return;
	}
	if (!MovieScene)
	{
		return;
	}
	MovieScene->AddBinding(FMovieSceneBinding(ObjectName));
	bSequenceDirty = true;
	SelectBinding(ObjectName);
}

void FLevelSequencePanel::AddTransformTrack()
{
	FMovieSceneBinding* Binding = MovieScene ? MovieScene->FindBinding(SelectedActorName) : nullptr;
	AActor* Actor = FindActorForUI(SelectedActorName);
	if (!Binding || !Actor)
	{
		return;
	}
	for (UMovieScenePropertyTrack* Existing : Binding->GetTracks())
	{
		if (Cast<UMovieScene3DTransformTrack>(Existing))
		{
			return;
		}
	}
	UMovieScene3DTransformTrack* NewTrack = NewObject<UMovieScene3DTransformTrack>(MovieScene);
	UMovieScene3DTransformSection* NewSection = Cast<UMovieScene3DTransformSection>(NewTrack->AddSection());
	if (!NewSection)
	{
		return;
	}
	const int32 LastFrame = MovieScene->GetFrameCount();
	NewSection->SetRange({ 0.f, static_cast<float>(LastFrame + 1) });
	const FTransform Transform = Actor->GetActorTransform();
	const float Locations[] = { Transform.Location.X, Transform.Location.Y, Transform.Location.Z };
	const float Rotations[] = { Transform.Rotation.Pitch, Transform.Rotation.Yaw, Transform.Rotation.Roll };
	const float Scales[] = { Transform.Scale.X, Transform.Scale.Y, Transform.Scale.Z };
	for (int Index = 0; Index < 3; ++Index)
	{
		NewSection->GetTranslationChannel()[Index].SetDefault(Locations[Index]);
		NewSection->GetRotationChannel()[Index].SetDefault(Rotations[Index]);
		NewSection->GetScaleChannel()[Index].SetDefault(Scales[Index]);
	}
	Binding->AddTrack(NewTrack);
	bSequenceDirty = true;
	SelectBinding(SelectedActorName);
}

void FLevelSequencePanel::AddCameraCutSection(const FString& CameraName)
{
	UMovieSceneCameraCutTrack* CameraTrack = MovieScene ? MovieScene->GetCameraCutTrack() : nullptr;
	if (!CameraTrack || MovieScene->GetFrameCount() <= 0)
	{
		return;
	}
	const int32 Start = std::clamp(PreviewFrame, 0, MovieScene->GetFrameCount() - 1);
	int32 End = MovieScene->GetFrameCount();
	for (UMovieSceneSection* Existing : CameraTrack->GetAllSections())
	{
		UMovieSceneCameraCutSection* Cut = Cast<UMovieSceneCameraCutSection>(Existing);
		if (!Cut)
		{
			continue;
		}
		const FFloatRange Range = Cut->GetRange();
		if (Range.Min == Start)
		{
			bSequenceDirty |= Cut->GetCameraName() != CameraName;
			Cut->SetCameraName(CameraName);
			SelectedCameraCut = Cut;
			SelectedFadeSection = nullptr;
			bFadeKeySelected = false;
			bDraggingFadeKey = false;
			CameraCutError.clear();
			return;
		}
		if (Range.Min > Start)
		{
			End = (std::min)(End, static_cast<int32>(Range.Min));
		}
	}
	UMovieSceneCameraCutSection* NewCut = Cast<UMovieSceneCameraCutSection>(CameraTrack->AddSection());
	if (!NewCut)
	{
		return;
	}
	for (UMovieSceneSection* Existing : CameraTrack->GetAllSections())
	{
		if (Existing == NewCut)
		{
			continue;
		}
		const FFloatRange Range = Existing->GetRange();
		if (Range.Min < Start && Range.Max > Start)
		{
			Existing->SetRange({ Range.Min, static_cast<float>(Start) });
			End = (std::min)(End, static_cast<int32>(Range.Max));
		}
	}
	NewCut->SetRange({ static_cast<float>(Start), static_cast<float>(End) });
	NewCut->SetCameraName(CameraName);
	SelectedCameraCut = NewCut;
	SelectedFadeSection = nullptr;
	bFadeKeySelected = false;
	bDraggingFadeKey = false;
	CameraCutError.clear();
	bSequenceDirty = true;
}

void FLevelSequencePanel::SetCameraCutRange(UMovieSceneCameraCutSection* Cut, int32 Start, int32 End)
{
	UMovieSceneCameraCutTrack* CameraTrack = MovieScene ? MovieScene->GetCameraCutTrack() : nullptr;
	if (!CameraTrack || !Cut)
	{
		return;
	}
	const FFloatRange OldRange = Cut->GetRange();
	UMovieSceneSection* Previous = nullptr;
	UMovieSceneSection* Next = nullptr;
	for (UMovieSceneSection* Other : CameraTrack->GetAllSections())
	{
		if (Other == Cut)
		{
			continue;
		}
		const FFloatRange Range = Other->GetRange();
		if (Range.Max <= OldRange.Min && (!Previous || Range.Max > Previous->GetRange().Max))
		{
			Previous = Other;
		}
		if (Range.Min >= OldRange.Max && (!Next || Range.Min < Next->GetRange().Min))
		{
			Next = Other;
		}
	}
	if (Start != OldRange.Min && End == OldRange.Max)
	{
		const bool bSharedBoundary = Previous && Previous->GetRange().Max == OldRange.Min;
		const int32 Minimum = Previous ? static_cast<int32>(bSharedBoundary ? Previous->GetRange().Min + 1 : Previous->GetRange().Max) : 0;
		Start = std::clamp(Start, Minimum, End - 1);
		if (bSharedBoundary)
		{
			Previous->SetRange({ Previous->GetRange().Min, static_cast<float>(Start) });
		}
	}
	else if (End != OldRange.Max && Start == OldRange.Min)
	{
		const bool bSharedBoundary = Next && Next->GetRange().Min == OldRange.Max;
		const int32 Maximum = Next ? static_cast<int32>(bSharedBoundary ? Next->GetRange().Max - 1 : Next->GetRange().Min) : MovieScene->GetFrameCount();
		End = std::clamp(End, Start + 1, Maximum);
		if (bSharedBoundary)
		{
			Next->SetRange({ static_cast<float>(End), Next->GetRange().Max });
		}
	}
	else
	{
		const int32 Minimum = Previous ? static_cast<int32>(Previous->GetRange().Max) : 0;
		const int32 Maximum = Next ? static_cast<int32>(Next->GetRange().Min) : MovieScene->GetFrameCount();
		const int32 Length = End - Start;
		if (Length <= 0 || Length > Maximum - Minimum)
		{
			return;
		}
		Start = std::clamp(Start, Minimum, Maximum - Length);
		End = Start + Length;
	}
	if (Start != OldRange.Min || End != OldRange.Max)
	{
		Cut->SetRange({ static_cast<float>(Start), static_cast<float>(End) });
		bSequenceDirty = true;
		CameraCutError.clear();
		if (SequencePlayer)
		{
			SequencePlayer->Stop();
			bFollowPlayerTime = false;
		}
	}
}

void FLevelSequencePanel::DrawCameraCutTimeline()
{
	bCameraCutDropTargetActive = false;
	UMovieSceneCameraCutTrack* CameraTrack = MovieScene ? MovieScene->GetCameraCutTrack() : nullptr;
	if (!CameraTrack)
	{
		return;
	}
	if (ImGui::BeginNeoTimelineEx("Camera Cuts"))
	{
		for (UMovieSceneSection* Existing : CameraTrack->GetAllSections())
		{
			UMovieSceneCameraCutSection* Cut = Cast<UMovieSceneCameraCutSection>(Existing);
			if (!Cut)
			{
				continue;
			}
			const FFloatRange Range = Cut->GetRange();
			int32_t Start = static_cast<int32_t>(Range.Min);
			int32_t End = static_cast<int32_t>(Range.Max);
			ImGui::PushID(Cut);
			const bool bCutClicked = ImGui::NeoSection("##CameraCut", &Start, &End, Cut->GetCameraName().c_str(), Cut == SelectedCameraCut, 0, MovieScene->GetFrameCount());
			const bool bCutHovered = ImGui::GetItemID() == ImGui::GetID("##CameraCut") && ImGui::IsItemHovered();
			if (DragChannel < 0 && (bCutClicked || (bCutHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))))
			{
				SelectedCameraCut = Cut;
				SelectedFadeSection = nullptr;
				bFadeKeySelected = false;
				bDraggingFadeKey = false;
				SelectedKeyChannel = -1;
			}
			if (DragChannel < 0 && (Start != Range.Min || End != Range.Max))
			{
				SetCameraCutRange(Cut, Start, End);
			}
			if (bCutHovered)
			{
				const FFloatRange CurrentRange = Cut->GetRange();
				ImGui::SetTooltip("%s | %d - %d", Cut->GetCameraName().c_str(), static_cast<int32>(CurrentRange.Min), static_cast<int32>(CurrentRange.Max));
			}
			ImGui::PopID();
		}
		FString DroppedCameraName;
		if (DragChannel < 0 && ImGui::NeoBeginTimelineDropTarget())
		{
			bCameraCutDropTargetActive = true;
			if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(EditorDragDrop::Actor))
			{
				if (Payload->DataSize == sizeof(uint32))
				{
					if (ACameraActor* Camera = Cast<ACameraActor>(FindActorForUI(*static_cast<const uint32*>(Payload->Data))))
					{
						DroppedCameraName = Camera->GetName();
					}
				}
			}
			ImGui::EndDragDropTarget();
		}
		ImGui::EndNeoTimeLine();
		if (!DroppedCameraName.empty())
		{
			AddCameraCutSection(DroppedCameraName);
		}
	}
}

bool FLevelSequencePanel::DrawTrackContextMenu()
{
	bool bChanged = false;
	if (ImGui::BeginPopupContextWindow("SequenceTrackMenu", ImGuiPopupFlags_MouseButtonRight))
	{
		ImGui::BeginDisabled(DragChannel >= 0);
		FMovieSceneBinding* Binding = MovieScene->FindBinding(SelectedActorName);
		UMovieScene3DTransformTrack* TransformTrack = nullptr;
		if (Binding)
		{
			for (UMovieScenePropertyTrack* Existing : Binding->GetTracks())
			{
				if (UMovieScene3DTransformTrack* Candidate = Cast<UMovieScene3DTransformTrack>(Existing))
				{
					TransformTrack = Candidate;
					break;
				}
			}
		}
		if (ImGui::BeginMenu("Add Track"))
		{
			if (ImGui::MenuItem("Fade", nullptr, false, FindFadeTrack() == nullptr))
			{
				AddFadeTrack();
				bChanged = true;
			}
			if (ImGui::MenuItem("Transform", nullptr, false, Binding && FindActorForUI(SelectedActorName) && !TransformTrack))
			{
				AddTransformTrack();
				bChanged = true;
			}
			if (ImGui::MenuItem("Camera Cut", nullptr, false, MovieScene->GetCameraCutTrack() == nullptr))
			{
				MovieScene->SetCameraCutTrack(NewObject<UMovieSceneCameraCutTrack>(MovieScene));
				SelectedCameraCut = nullptr;
				bSequenceDirty = true;
				bChanged = true;
			}
			ImGui::EndMenu();
		}
		ImGui::Separator();
		UMovieSceneCameraCutTrack* CameraTrack = MovieScene->GetCameraCutTrack();
		if (ImGui::MenuItem("Delete Cut", nullptr, false, CameraTrack && SelectedCameraCut))
		{
			CameraTrack->RemoveSection(SelectedCameraCut);
			bool bStillPresent = false;
			for (UMovieSceneSection* Remaining : CameraTrack->GetAllSections())
			{
				bStillPresent |= Remaining == SelectedCameraCut;
			}
			if (!bStillPresent)
			{
				SelectedCameraCut = nullptr;
				CameraCutError.clear();
				bSequenceDirty = true;
				bChanged = true;
			}
			else
			{
				CameraCutError = "RemoveSection did not remove the selected cut.";
			}
		}
		if (ImGui::MenuItem("Remove Transform Track", nullptr, false, TransformTrack != nullptr))
		{
			if (SequencePlayer)
			{
				SequencePlayer->Stop();
				SequencePlayer->RestoreState();
			}
			Binding->RemoveTrack(TransformTrack);
			SelectBinding(SelectedActorName);
			bFollowPlayerTime = false;
			bSequenceDirty = true;
			bChanged = true;
		}
		if (ImGui::MenuItem("Remove Camera Cut Track", nullptr, false, CameraTrack != nullptr))
		{
			if (SequencePlayer)
			{
				SequencePlayer->Stop();
			}
			MovieScene->SetCameraCutTrack(nullptr);
			SelectedCameraCut = nullptr;
			CameraCutError.clear();
			bFollowPlayerTime = false;
			bSequenceDirty = true;
			bChanged = true;
		}
		ImGui::EndDisabled();
		ImGui::EndPopup();
	}
	return bChanged;
}

void FLevelSequencePanel::DrawCameraCutEditor()
{
	UMovieSceneCameraCutTrack* CameraTrack = MovieScene ? MovieScene->GetCameraCutTrack() : nullptr;
	if (!CameraTrack || SelectedFadeSection || !ImGui::CollapsingHeader("Camera Cuts", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}
	ImGui::PushID("CameraCutEditor");
	ImGui::BeginDisabled(DragChannel >= 0);
	auto DrawCameraOptions = [&](bool bAdd)
	{
		UEditorEngine* Editor = Cast<UEditorEngine>(GEngine);
		UWorld* World = Editor ? Editor->GetEditorWorldContext().World() : nullptr;
		ULevel* Level = World ? World->GetCurrentLevel() : nullptr;
		bool bHasCamera = false;
		if (Level)
		{
			for (AActor* Actor : Level->GetActors())
			{
				if (ACameraActor* Camera = Cast<ACameraActor>(Actor))
				{
					bHasCamera = true;
					const FString Name = Camera->GetName();
					if (ImGui::Selectable(Name.c_str(), !bAdd && SelectedCameraCut && SelectedCameraCut->GetCameraName() == Name))
					{
						if (bAdd)
						{
							AddCameraCutSection(Name);
						}
						else
						{
							bSequenceDirty |= SelectedCameraCut->GetCameraName() != Name;
							SelectedCameraCut->SetCameraName(Name);
						}
					}
				}
			}
		}
		if (!bHasCamera)
		{
			ImGui::TextDisabled("No cameras in this level");
		}
	};
	if (ImGui::Button("+ Cut at cursor"))
	{
		ImGui::OpenPopup("AddCameraCut");
	}
	if (ImGui::BeginPopup("AddCameraCut"))
	{
		DrawCameraOptions(true);
		ImGui::EndPopup();
	}
	if (SelectedCameraCut)
	{
		if (ImGui::Button(SelectedCameraCut->GetCameraName().c_str()))
		{
			ImGui::OpenPopup("ChangeCamera");
		}
		if (ImGui::BeginPopup("ChangeCamera"))
		{
			DrawCameraOptions(false);
			ImGui::EndPopup();
		}
		const FFloatRange Range = SelectedCameraCut->GetRange();
		ImGui::Text("Frames: %d - %d", static_cast<int32>(Range.Min), static_cast<int32>(Range.Max));
	}
	if (!CameraCutError.empty())
	{
		ImGui::TextWrapped("%s", CameraCutError.c_str());
	}
	ImGui::EndDisabled();
	ImGui::PopID();
}

void FLevelSequencePanel::Tick(float DeltaTime)
{
	if (!IsOpen())
	{
		if (bWasOpen)
		{
			CloseSequenceEditor();
		}
		return;
	}
	bWasOpen = true;
	if (!SequencePlayer)
	{
		return;
	}
	
	SequencePlayer->Update(DeltaTime);
}

void FLevelSequencePanel::OnRender()
{
	if (bFocusRequested)
	{
		ImGui::SetNextWindowFocus();
		bFocusRequested = false;
	}
	bool bOpen = IsOpen();
	const bool bVisible = ImGui::Begin("Level Sequencer###Neo Sequencer Test", &bOpen);
	if (!bOpen)
	{
		ImGui::End();
		CloseSequenceEditor();
		return;
	}
	if (!bVisible)
	{
		ImGui::End();
		return;
	}
	FString DroppedSequencePath;
	ImGuiWindow* Window = ImGui::GetCurrentWindow();
	if (DragChannel < 0 && ImGui::BeginDragDropTargetCustom(Window->InnerRect, ImGui::GetID("##SequenceFileDrop")))
	{
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(EditorDragDrop::Sequence))
		{
			if (Payload->Data && Payload->DataSize > 1 && Payload->DataSize <= sizeof(SequencePath))
			{
				const char* Path = static_cast<const char*>(Payload->Data);
				if (Path[Payload->DataSize - 1] == '\0')
				{
					DroppedSequencePath = Path;
				}
			}
		}
		ImGui::EndDragDropTarget();
	}
	if (!DroppedSequencePath.empty())
	{
		OpenSequenceFile(DroppedSequencePath);
	}
	if (!FileStatus.empty())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, bFileError ? ImVec4(1.0f, 0.4f, 0.35f, 1.0f) : ImVec4(0.5f, 0.85f, 0.55f, 1.0f));
		ImGui::TextWrapped("%s", FileStatus.c_str());
		ImGui::PopStyleColor();
	}
	if (OpenedSequencePath.empty())
	{
		const ImVec2 Available = ImGui::GetContentRegionAvail();
		const ImVec2 Size((std::max)(1.0f, Available.x), (std::max)(1.0f, Available.y));
		const ImVec2 Position = ImGui::GetCursorScreenPos();
		const char* Label = "Drag Level Sequence Here";
		const ImVec2 TextSize = ImGui::CalcTextSize(Label);
		ImGui::InvisibleButton("##EmptySequenceDrop", Size);
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		DrawList->AddRectFilled(Position, ImVec2(Position.x + Size.x, Position.y + Size.y), ImGui::GetColorU32(ImGuiCol_FrameBg), 4.0f);
		DrawList->AddRect(Position, ImVec2(Position.x + Size.x, Position.y + Size.y), ImGui::GetColorU32(ImGuiCol_Border), 4.0f);
		DrawList->AddText(ImVec2(Position.x + (Size.x - TextSize.x) * 0.5f, Position.y + (Size.y - TextSize.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_TextDisabled), Label);
		ImGui::End();
		return;
	}

	if (SampleSequence && MovieScene)
	{
		const float Duration = MovieScene ? MovieScene->GetDuration() : 0.0f;
		const int32 FrameCount = MovieScene ? MovieScene->GetFrameCount() : 0;
		const bool bValidTiming = std::isfinite(Duration) && Duration > 0.0f && FrameCount > 0;
		if (!bValidTiming)
		{
			ImGui::TextUnformatted("Set a positive sequence duration and frame count.");
			ImGui::End();
			return;
		}
		const double DisplayFPS = static_cast<double>(FrameCount) / Duration;
		if (SequencePlayer)
		{
			ImGui::Text("Playback time: %.3f s", SequencePlayer->GetElapsedTime());
		}
		ImGui::BeginDisabled(SequencePlayer == nullptr);
		if (ImGui::Button("Play") && SequencePlayer)
		{
			SequencePlayer->Play();
			bFollowPlayerTime = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop") && SequencePlayer)
		{
			SequencePlayer->Stop();
		}
		ImGui::SameLine();
		if (ImGui::Button("Restart") && SequencePlayer)
		{
			SequencePlayer->Play();
			PreviewFrame = 0;
			bFollowPlayerTime = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Restore original") && SequencePlayer)
		{
			SequencePlayer->Stop();
			SequencePlayer->RestoreState();
			bFollowPlayerTime = false;
		}
		ImGui::EndDisabled();

		{
			AActor* SelectedActor = FindActorForUI(SelectedActorName);
			const FFloatRange CurrentRange = Section ? Section->GetRange() : FFloatRange{ 0.f, static_cast<float>(FrameCount) };
			auto ToRangeFrame = [](float Frame) -> int
			{
				if (!std::isfinite(Frame))
				{
					return 0;
				}
				return static_cast<int>(std::clamp(std::round(static_cast<double>(Frame)), -16777216.0, 16777215.0));
			};
			int SectionStart = ToRangeFrame(CurrentRange.Min);
			int SectionLastFrame = ToRangeFrame(CurrentRange.Max) - 1;
			if (Section && !SelectedCameraCut && !SelectedFadeSection)
			{
				ImGui::BeginDisabled(DragChannel >= 0);
				ImGui::TextUnformatted("Start frame");
				ImGui::SameLine();
				ImGui::SetNextItemWidth((std::min)(160.0f, (std::max)(60.0f, ImGui::GetContentRegionAvail().x)));
				const bool bStartChanged = ImGui::InputInt("##SectionStartFrame", &SectionStart, 0, 0);
				ImGui::TextUnformatted("End frame");
				ImGui::SameLine();
				ImGui::SetNextItemWidth((std::min)(160.0f, (std::max)(60.0f, ImGui::GetContentRegionAvail().x)));
				const bool bEndChanged = ImGui::InputInt("##SectionEndFrame", &SectionLastFrame, 0, 0);
				ImGui::EndDisabled();
				if (bStartChanged || bEndChanged)
				{
					SectionStart = std::clamp(SectionStart, -16777216, 16777214);
					SectionLastFrame = std::clamp(SectionLastFrame, -16777216, 16777214);
					if (SectionStart > SectionLastFrame)
					{
						if (bStartChanged)
						{
							SectionLastFrame = SectionStart;
						}
						else
						{
							SectionStart = SectionLastFrame;
						}
					}
					const FFloatRange NewRange = { static_cast<float>(SectionStart), static_cast<float>(SectionLastFrame + 1) };
					if (NewRange.Min != CurrentRange.Min || NewRange.Max != CurrentRange.Max)
					{
						Section->SetRange(NewRange);
						bSequenceDirty = true;
					}
					if (SequencePlayer)
					{
						SequencePlayer->Stop();
						bFollowPlayerTime = false;
						SequencePlayer->SetPlaybackPosition(static_cast<float>(PreviewFrame / DisplayFPS));
					}
				}
			}
			struct FChannelRow
			{
				const char* Label;
				FMovieSceneFloatChannel* Data;
			};
			const FChannelRow Rows[] = {
				{ "Position X", Section ? &Section->GetTranslationChannel()[0] : nullptr },
				{ "Position Y", Section ? &Section->GetTranslationChannel()[1] : nullptr },
				{ "Position Z", Section ? &Section->GetTranslationChannel()[2] : nullptr },
				{ "Rotation Pitch", Section ? &Section->GetRotationChannel()[0] : nullptr },
				{ "Rotation Yaw", Section ? &Section->GetRotationChannel()[1] : nullptr },
				{ "Rotation Roll", Section ? &Section->GetRotationChannel()[2] : nullptr },
				{ "Scale X", Section ? &Section->GetScaleChannel()[0] : nullptr },
				{ "Scale Y", Section ? &Section->GetScaleChannel()[1] : nullptr },
				{ "Scale Z", Section ? &Section->GetScaleChannel()[2] : nullptr }
			};
			auto ToFrame = [DisplayFPS](float Seconds) -> int32_t
			{
				const double Frame = static_cast<double>(Seconds) * DisplayFPS;
				if (!std::isfinite(Frame))
				{
					return 0;
				}
				return static_cast<int32_t>(std::round(std::clamp(Frame, static_cast<double>((std::numeric_limits<int32_t>::min)()) + 1.0, static_cast<double>((std::numeric_limits<int32_t>::max)()) - 1.0)));
			};
			int32_t StartFrame = (std::min)(0, SectionStart);
			int32_t EndFrame = (std::max)(FrameCount, SectionLastFrame + 1);
			for (const FChannelRow& Row : Rows)
			{
				if (!Row.Data)
				{
					continue;
				}
				for (int32 Frame : Row.Data->GetTimes())
				{
					StartFrame = (std::min)(StartFrame, Frame);
					EndFrame = (std::max)(EndFrame, Frame < (std::numeric_limits<int32_t>::max)() ? Frame + 1 : Frame);
				}
			}
			if (SequencePlayer && bFollowPlayerTime)
			{
				PreviewFrame = ToFrame(SequencePlayer->GetElapsedTime());
			}
			StartFrame = (std::min)(StartFrame, PreviewFrame);
			EndFrame = (std::max)(EndFrame, PreviewFrame + 1);
			ImGui::Text("Preview: frame %d / %.3f s", PreviewFrame, PreviewFrame / DisplayFPS);
			const int32_t FrameBeforeInput = PreviewFrame;
			FString PendingActorName;
			bool bActorSelected = false;
			UMovieScene3DTransformSection* PendingSection = nullptr;
			UMovieScene3DTransformTrack* PendingTrack = nullptr;
			int PendingChannel = -1;
			int PendingIndex = -1;
			float PendingFrameWidth = 1.0f;
			bool bTrackChanged = false;
			bool bActorDropped = false;
			uint32 DroppedActorUUID = 0;
			if (DragChannel < 0)
			{
				for (int ChannelIndex = 0; ChannelIndex < 9; ++ChannelIndex)
				{
					TimelineFrames[ChannelIndex].clear();
					if (!Rows[ChannelIndex].Data)
					{
						continue;
					}
					for (int32 Frame : Rows[ChannelIndex].Data->GetTimes())
					{
						TimelineFrames[ChannelIndex].push_back(Frame);
					}
				}
			}
			const float TimelineHeight = (std::max)(180.0f, ImGui::GetContentRegionAvail().y * 0.65f);
			if (ImGui::BeginNeoSequencer("SequenceData", &PreviewFrame, &StartFrame, &EndFrame, ImVec2(0, TimelineHeight), ImGuiNeoSequencerFlags_HideZoom))
			{
				DrawCameraCutTimeline();
				DrawFadeTimeline();
				if (DragChannel >= 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
				{
					const double Offset = (ImGui::GetMousePos().x - DragStartMouseX) / DragPixelsPerFrame;
					const double Target = std::round(static_cast<double>(DragStartFrame) + Offset);
					TimelineFrames[DragChannel][DragKeyIndex] = static_cast<int32_t>(std::clamp(Target, static_cast<double>((std::numeric_limits<int32_t>::min)()) + 1.0, static_cast<double>((std::numeric_limits<int32_t>::max)()) - 1.0));
				}
				for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
				{
					const FString& ActorName = Binding.GetObjectName();
					AActor* Actor = FindActorForUI(ActorName);
					bool& bActorOpen = ActorTrackOpen.try_emplace(ActorName, true).first->second;
					bool& bTransformOpen = TransformTrackOpen.try_emplace(ActorName, true).first->second;
					ImGui::PushID(ActorName.c_str());
					const bool bActorSelectedNow = SelectedActorName == ActorName;
					if (bActorSelectedNow)
					{
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.25f, 1.0f));
					}
					const bool bDrawActorTracks = ImGui::BeginNeoGroup(Actor ? Actor->GetName().c_str() : "Missing Actor", &bActorOpen);
					if (bActorSelectedNow)
					{
						ImGui::PopStyleColor();
					}
					if (DragChannel < 0 && (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right)))
					{
						PendingActorName = ActorName;
						bActorSelected = true;
					}
					if (bDrawActorTracks)
					{
						const bool bHasTransform = std::any_of(Binding.GetTracks().begin(), Binding.GetTracks().end(), [](UMovieScenePropertyTrack* Candidate)
						{
							return Cast<UMovieScene3DTransformTrack>(Candidate) != nullptr;
						});
						if (bHasTransform && ImGui::BeginNeoGroup("Transform", &bTransformOpen))
						{
							for (UMovieScenePropertyTrack* ActorTrack : Binding.GetTracks())
							{
								if (!ActorTrack)
								{
									continue;
								}
								for (UMovieSceneSection* ActorSection : ActorTrack->GetAllSections())
								{
									UMovieScene3DTransformSection* DrawSection = Cast<UMovieScene3DTransformSection>(ActorSection);
									if (!DrawSection)
									{
										continue;
									}
									ImGui::PushID(DrawSection);
									for (int ChannelIndex = 0; ChannelIndex < 9; ++ChannelIndex)
									{
										FMovieSceneFloatChannel* ChannelData = ChannelIndex < 3 ? &DrawSection->GetTranslationChannel()[ChannelIndex] : (ChannelIndex < 6 ? &DrawSection->GetRotationChannel()[ChannelIndex - 3] : &DrawSection->GetScaleChannel()[ChannelIndex - 6]);
										ImGui::PushID(ChannelIndex);
										if (ImGui::BeginNeoTimelineEx(Rows[ChannelIndex].Label))
										{
											const FFloatRange DrawRange = DrawSection->GetRange();
											ImGui::NeoDrawTimelineRect(ToRangeFrame(DrawRange.Min), ToRangeFrame(DrawRange.Max) - 1, ImVec4(0.2f, 0.5f, 0.8f, 0.15f));
											const auto& Times = ChannelData->GetTimes();
											const auto& Values = ChannelData->GetValues();
											for (int32 Index = 0; Index < Times.Num(); ++Index)
											{
												int32_t Frame = Times[Index];
												if (DrawSection == Section && DragChannel == ChannelIndex && DragKeyIndex == Index)
												{
													Frame = TimelineFrames[ChannelIndex][Index];
												}
												const bool bSelected = DrawSection == Section && SelectedKeyChannel == ChannelIndex && SelectedKeyFrame == Times[Index];
												if (bSelected)
												{
													ImGui::PushNeoSequencerStyleColor(ImGuiNeoSequencerCol_Keyframe, IM_COL32(255, 190, 60, 255));
												}
												ImGui::PushID(Index);
												ImGui::NeoKeyframe(&Frame);
												if (Index < Values.Num() && ImGui::IsNeoKeyframeHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
												{
													PendingActorName = ActorName;
													PendingSection = DrawSection;
													PendingTrack = Cast<UMovieScene3DTransformTrack>(ActorTrack);
													PendingChannel = ChannelIndex;
													PendingIndex = Index;
													PendingFrameWidth = (std::max)(ImGui::NeoGetFrameWidth(), 0.001f);
												}
												ImGui::PopID();
												if (bSelected)
												{
													ImGui::PopNeoSequencerStyleColor();
												}
											}
											ImGui::EndNeoTimeLine();
										}
										ImGui::PopID();
									}
									ImGui::PopID();
								}
							}
							ImGui::EndNeoGroup();
						}
						ImGui::EndNeoGroup();
					}
					ImGui::PopID();
				}
				if (DragChannel < 0 && !bCameraCutDropTargetActive && ImGui::NeoBeginTrackDropTarget())
				{
					if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(EditorDragDrop::Actor))
					{
						if (Payload->DataSize == sizeof(uint32))
						{
							DroppedActorUUID = *static_cast<const uint32*>(Payload->Data);
							bActorDropped = true;
						}
					}
					ImGui::EndDragDropTarget();
				}
				if (bActorSelected)
				{
					SelectBinding(PendingActorName);
				}
				bTrackChanged = DrawTrackContextMenu();
				ImGui::EndNeoSequencer();
			}
			if (bTrackChanged)
			{
				ImGui::End();
				return;
			}
			if (bActorDropped)
			{
				AddActor(DroppedActorUUID);
				ImGui::End();
				return;
			}
			if (PendingSection)
			{
				SelectBinding(PendingActorName);
				Section = PendingSection;
				Track = PendingTrack;
				for (int Index = 0; Index < 9; ++Index)
				{
					FMovieSceneFloatChannel* ChannelData = Index < 3 ? &Section->GetTranslationChannel()[Index] : (Index < 6 ? &Section->GetRotationChannel()[Index - 3] : &Section->GetScaleChannel()[Index - 6]);
					TimelineFrames[Index].clear();
					for (int32 Frame : ChannelData->GetTimes())
					{
						TimelineFrames[Index].push_back(Frame);
					}
				}
				FMovieSceneFloatChannel* ChannelData = PendingChannel < 3 ? &Section->GetTranslationChannel()[PendingChannel] : (PendingChannel < 6 ? &Section->GetRotationChannel()[PendingChannel - 3] : &Section->GetScaleChannel()[PendingChannel - 6]);
				SelectedKeyChannel = PendingChannel;
				SelectedChannel = PendingChannel;
				SelectedKeyFrame = ChannelData->GetTimes()[PendingIndex];
				KeyFrame = SelectedKeyFrame;
				KeyValue = ChannelData->GetValues()[PendingIndex].Value;
				KeyInterpolation = ChannelData->GetValues()[PendingIndex].InterpMode == ECurveInterpMode::Linear ? 1 : 0;
				DragChannel = PendingChannel;
				DragKeyIndex = PendingIndex;
				DragStartFrame = SelectedKeyFrame;
				DragStartMouseX = ImGui::GetMousePos().x;
				DragPixelsPerFrame = PendingFrameWidth;
				bFollowPlayerTime = false;
				if (SequencePlayer)
				{
					SequencePlayer->Stop();
				}
				ImGui::End();
				return;
			}
			if (bActorSelected)
			{
				SelectBinding(PendingActorName);
				ImGui::End();
				return;
			}
			if (Section && DragChannel >= 0 && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				const int32 NewFrame = TimelineFrames[DragChannel][DragKeyIndex];
				const FFloatRange& Range = Section->GetRange();
				bool bCanMove = NewFrame != DragStartFrame && NewFrame >= Range.Min && NewFrame < Range.Max;
				for (int32 Frame : Rows[DragChannel].Data->GetTimes())
				{
					if (Frame == NewFrame)
					{
						bCanMove = false;
						break;
					}
				}
				if (bCanMove)
				{
					Rows[DragChannel].Data->MoveKey(DragStartFrame, NewFrame);
					bSequenceDirty = true;
					SelectedKeyFrame = NewFrame;
					KeyFrame = NewFrame;
					if (SequencePlayer)
					{
						SequencePlayer->SetPlaybackPosition(static_cast<float>(PreviewFrame / DisplayFPS));
					}
				}
				DragChannel = -1;
				DragKeyIndex = -1;
			}
			if (SequencePlayer && PreviewFrame != FrameBeforeInput)
			{
				bFollowPlayerTime = false;
				SequencePlayer->Stop();
				SequencePlayer->SetPlaybackPosition(static_cast<float>(PreviewFrame / DisplayFPS));
			}

			if (Section && !SelectedCameraCut && !SelectedFadeSection)
			{
				ImGui::Separator();
				ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "Actor: %s", SelectedActor ? SelectedActor->GetName().c_str() : "Missing Actor");
				bool bHasSelectedKey = false;
				if (SelectedKeyChannel >= 0 && SelectedKeyChannel < 9)
				{
					for (int32 Frame : Rows[SelectedKeyChannel].Data->GetTimes())
					{
						if (Frame == SelectedKeyFrame)
						{
							bHasSelectedKey = true;
							break;
						}
					}
				}
				if (bHasSelectedKey)
				{
					ImGui::Text("Selected: %s | Frame %d", Rows[SelectedKeyChannel].Label, SelectedKeyFrame);
				}
				ImGui::SetNextItemWidth(150.0f);
				if (ImGui::BeginCombo("Channel", Rows[SelectedChannel].Label))
				{
					for (int Index = 0; Index < 9; ++Index)
					{
						if (ImGui::Selectable(Rows[Index].Label, SelectedChannel == Index))
						{
							SelectedChannel = Index;
							SelectedKeyChannel = -1;
							bHasSelectedKey = false;
						}
					}
					ImGui::EndCombo();
				}
				KeyFrame = bHasSelectedKey ? SelectedKeyFrame : PreviewFrame;
				ImGui::Text("Target frame: %d%s", KeyFrame, bHasSelectedKey ? " (selected key)" : " (cursor)");
				ImGui::SetNextItemWidth(150.0f);
				const bool bValueChanged = ImGui::InputFloat("Key value", &KeyValue);
				ImGui::SetNextItemWidth(150.0f);
				const bool bInterpolationChanged = ImGui::Combo("Interpolation", &KeyInterpolation, "Constant\0Linear\0");
				const FFloatRange& Range = Section->GetRange();
				const bool bCursorInSection = PreviewFrame >= Range.Min && PreviewFrame < Range.Max;
				ImGui::Text("Add at cursor: frame %d", PreviewFrame);
				ImGui::BeginDisabled(!bCursorInSection || !std::isfinite(KeyValue));
				const bool bAddKey = ImGui::Button("Add key at cursor");
				ImGui::EndDisabled();
				if (std::isfinite(KeyValue) && (bAddKey || (bHasSelectedKey && (bValueChanged || bInterpolationChanged))))
				{
					const int32 TargetFrame = bAddKey ? PreviewFrame : SelectedKeyFrame;
					FMovieSceneFloatValue NewKey;
					NewKey.Value = KeyValue;
					NewKey.InterpMode = KeyInterpolation == 0 ? ECurveInterpMode::Constant : ECurveInterpMode::Linear;
					bool bKeyChanged = true;
					const auto& Times = Rows[SelectedChannel].Data->GetTimes();
					const auto& Values = Rows[SelectedChannel].Data->GetValues();
					for (int32 Index = 0; Index < Times.Num(); ++Index)
					{
						if (Times[Index] == TargetFrame)
						{
							bKeyChanged = Values[Index].Value != NewKey.Value || Values[Index].InterpMode != NewKey.InterpMode;
							break;
						}
					}
					Rows[SelectedChannel].Data->UpdateOrAddKey(TargetFrame, NewKey);
					bSequenceDirty |= bKeyChanged;
					SelectedKeyChannel = SelectedChannel;
					SelectedKeyFrame = TargetFrame;
					bHasSelectedKey = true;
					if (SequencePlayer)
					{
						SequencePlayer->Stop();
						bFollowPlayerTime = false;
						SequencePlayer->SetPlaybackPosition(static_cast<float>(PreviewFrame / DisplayFPS));
					}
				}
				if (ImGui::GetContentRegionAvail().x > 320.0f)
				{
					ImGui::SameLine();
				}
				ImGui::BeginDisabled(!bHasSelectedKey);
				if (ImGui::Button("Delete selected key"))
				{
					Rows[SelectedKeyChannel].Data->RemoveKey(SelectedKeyFrame);
					bSequenceDirty = true;
					SelectedKeyChannel = -1;
					if (SequencePlayer)
					{
						SequencePlayer->Stop();
						bFollowPlayerTime = false;
						SequencePlayer->SetPlaybackPosition(static_cast<float>(PreviewFrame / DisplayFPS));
					}
				}
				ImGui::EndDisabled();
			}
		}
	}
	if (SampleSequence && !OpenedSequencePath.empty())
	{
		DrawCameraCutEditor();
		DrawFadeEditor();
		ImGui::Separator();
		ImGui::BeginDisabled(DragChannel >= 0 || !bSequenceDirty);
		if (ImGui::Button("Save"))
		{
			SaveSequenceFile();
		}
		ImGui::EndDisabled();
	}
	ImGui::End();
}
