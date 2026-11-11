#include "EnginePCH.h"
#include "SkeletalMeshPreviewTab.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Engine/Engine.h"
#include "Editor/HitoriEd/EditorEngine.h"
#include "Component/SkinnedMeshComponent.h"
#include "GameFramework/Actor/SkeletalMeshActor.h"
#include "Core/PathUtils.h"
#include "UObject/UObjectIterator.h"
#include <commdlg.h>

namespace
{
	FEditorTabSettings MakeSkeletalMeshPreviewTabSettings()
	{
		FEditorTabSettings Settings;
		Settings.Title = "Skeletal Mesh Preview";
		Settings.DockSpaceName = "SkeletalMeshPreviewDock";
		Settings.bClosable = true;
		Settings.bIsolatePanels = true;
		return Settings;
	}
}

FSkeletalMeshPreviewTab::FSkeletalMeshPreviewTab(USkeletalMesh* InSkeletalMesh)
	: FSkeletalPreviewTabBase(MakeSkeletalMeshPreviewTabSettings(), InSkeletalMesh)
{
	if (const USkeleton* Skeleton = SkeletalMesh ? SkeletalMesh->GetSkeleton() : nullptr)
	{
		PoseSavePath = Skeleton->GetPosePath();
	}
}

void FSkeletalMeshPreviewTab::OnBoneTransformModified()
{
	bPoseDirty = true;
	bPoseSaveFailed = false;

	if (ActivePreviewAnim && MeshActor && MeshActor->GetSkeletalMeshComponent())
	{
		if (UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(MeshActor->GetSkeletalMeshComponent()->GetAnimInstance()))
		{
			SingleNode->Pause();
		}
	}
}

void FSkeletalMeshPreviewTab::OnBoneGizmoDragStarted()
{
	if (ActivePreviewAnim && MeshActor && MeshActor->GetSkeletalMeshComponent())
	{
		if (UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(MeshActor->GetSkeletalMeshComponent()->GetAnimInstance()))
		{
			SingleNode->Pause();
		}
	}
}

void FSkeletalMeshPreviewTab::DrawToolbar()
{
	DrawCommonViewToolbar();

	ImGui::BeginDisabled(PoseSavePath.empty() || BoneGizmo.IsUsing());
	const FString FileEntry = PathUtils::ToUtf8(PoseSavePath.filename()) + " ...###PoseFile";
	if (ImGui::Button(FileEntry.c_str()))
	{
		SelectPosePath();
	}
	ImGui::SetItemTooltip("%s", PathUtils::ToUtf8(PoseSavePath).c_str());
	ImGui::EndDisabled();

	const USkeleton* Skeleton = SkeletalMesh ? SkeletalMesh->GetSkeleton() : nullptr;
	const bool bNeedsSave = bPoseDirty || (Skeleton && PoseSavePath != Skeleton->GetPosePath());
	ImGui::SameLine();
	ImGui::BeginDisabled(!bNeedsSave || PoseSavePath.empty() || BoneGizmo.IsUsing());
	if (ImGui::Button("Save"))
	{
		SavePose();
	}
	ImGui::EndDisabled();

	if (bNeedsSave)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("%s", bPoseSaveFailed ? "Pose save failed" : "Unsaved changes");
	}

	ImGui::SameLine();
	ImGui::Separator();
	ImGui::SameLine();

	const FString CurrentAnimName = ActivePreviewAnim
		? PathUtils::ToUtf8(std::filesystem::path(ActivePreviewAnim->GetPath()).stem())
		: "None (Bind Pose)";

	ImGui::TextUnformatted("Animation:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(180.0f);
	if (ImGui::BeginCombo("##MeshPreviewAnimCombo", CurrentAnimName.c_str()))
	{
		const bool bNoneSelected = (ActivePreviewAnim == nullptr);
		if (ImGui::Selectable("None (Bind Pose)", bNoneSelected))
		{
			ActivePreviewAnim = nullptr;
			if (MeshActor && MeshActor->GetSkeletalMeshComponent())
			{
				MeshActor->GetSkeletalMeshComponent()->PlayAnimation(nullptr);
			}
		}
		if (bNoneSelected)
		{
			ImGui::SetItemDefaultFocus();
		}

		TArray<UAnimSequence*> Compatible = GetCompatibleAnimSequences();
		for (UAnimSequence* Anim : Compatible)
		{
			const FString Name = PathUtils::ToUtf8(std::filesystem::path(Anim->GetPath()).stem());
			const bool bSelected = (Anim == ActivePreviewAnim);
			if (ImGui::Selectable(Name.c_str(), bSelected))
			{
				ActivePreviewAnim = Anim;
				if (MeshActor && MeshActor->GetSkeletalMeshComponent())
				{
					MeshActor->GetSkeletalMeshComponent()->PlayAnimation(Anim);
					MeshActor->GetSkeletalMeshComponent()->SetAnimationLooping(true);
				}
			}
			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SetItemTooltip("Preview animation on this mesh");

	if (ActivePreviewAnim)
	{
		ImGui::SameLine();
		if (ImGui::Button("Open Timeline"))
		{
			if (UEditorEngine* Editor = Cast<UEditorEngine>(GEngine))
			{
				Editor->OpenAnimationSequenceViewer(SkeletalMesh, ActivePreviewAnim);
			}
		}
		ImGui::SetItemTooltip("Open this animation in Animation Viewer with full timeline controls");
	}
}

void FSkeletalMeshPreviewTab::SelectPosePath()
{
	std::wstring FilePath(32768, L'\0');
	const auto FileName = PoseSavePath.filename().wstring();
	const auto InitialDirectory = PoseSavePath.parent_path().wstring();
	FileName.copy(FilePath.data(), FilePath.size() - 1);

	OPENFILENAMEW Dialog = {};
	Dialog.lStructSize = sizeof(Dialog);
	Dialog.hwndOwner = GetActiveWindow();
	Dialog.lpstrFile = FilePath.data();
	Dialog.nMaxFile = static_cast<DWORD>(FilePath.size());
	Dialog.lpstrFilter = L"Pose Files (*.pose)\0*.pose\0";
	Dialog.lpstrDefExt = L"pose";
	Dialog.lpstrInitialDir = InitialDirectory.c_str();
	Dialog.lpstrTitle = L"Choose Pose Save Location";
	Dialog.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
	if (GetSaveFileNameW(&Dialog))
	{
		const std::filesystem::path SelectedPath(FilePath.c_str());
		if (_wcsicmp(SelectedPath.extension().c_str(), L".pose") != 0)
		{
			MessageBoxW(Dialog.hwndOwner, L"Choose a file with the .pose extension.", L"Pose Save Location", MB_OK | MB_ICONWARNING);
			return;
		}
		PoseSavePath = SelectedPath;
		bPoseSaveFailed = false;
	}
}

void FSkeletalMeshPreviewTab::SavePose()
{
	if (!SkeletalMesh) return;
	USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
	USkinnedMeshComponent* PreviewComponent = MeshActor->GetSkeletalMeshComponent();
	if (!Skeleton || !Skeleton->SaveDefaultPose(PreviewComponent->GetLocalPose(), PoseSavePath))
	{
		bPoseSaveFailed = true;
		HTR_LOG(Error, "Failed to save skeletal mesh pose: {}", SkeletalMesh->GetPath());
		return;
	}

	bPoseDirty = false;
	bPoseSaveFailed = false;
	PoseSavePath = Skeleton->GetPosePath();
	for (TObjectIterator<USkinnedMeshComponent> Component; Component; ++Component)
	{
		if (*Component != PreviewComponent && Component->GetSkeletalMesh() == SkeletalMesh)
		{
			Component->ResetPose();
			Component->UpdateBoneMatrices();
		}
	}
}