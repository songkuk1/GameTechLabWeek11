#include "EnginePCH.h"
#include "GameFramework/Actor/SkeletalMeshActor.h"
#include "AnimSequencePreviewTab.h"
#include "AnimTimelinePanel.h"
#include "SkeletonTreePanel.h"
#include "PersonaDetailsPanel.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/Skeleton.h"
#include "Core/PathUtils.h"
#include "UObject/UObjectIterator.h"
#include "Editor/Viewports/PreviewViewportPanel.h"
#include "Asset/ModelBake.h"

namespace
{
	FEditorTabSettings MakeAnimSequencePreviewTabSettings(UAnimSequence* AnimSeq)
	{
		FEditorTabSettings Settings;
		const FString AnimName = AnimSeq ? PathUtils::ToUtf8(std::filesystem::path(AnimSeq->GetPath()).stem()) : "Animation";
		Settings.Title = "Anim: " + AnimName + "###AnimSequencePreview";
		Settings.DockSpaceName = "AnimationPreviewDock";
		Settings.bClosable = true;
		Settings.bIsolatePanels = true;
		return Settings;
	}
}

FAnimSequencePreviewTab::FAnimSequencePreviewTab(UAnimSequence* InAnimSequence)
	: FSkeletalPreviewTabBase(MakeAnimSequencePreviewTabSettings(InAnimSequence),
		(InAnimSequence && InAnimSequence->GetSkeleton()) ? InAnimSequence->GetSkeleton()->GetPreviewMesh() : nullptr),
	AnimSequence(InAnimSequence)
{
	if (DetailsPanel)
	{
		DetailsPanel->SetTargetSequence(AnimSequence);
	}

	// 하단 타임라인 패널 추가
	TimelinePanel = AddPanel<FAnimTimelinePanel>(SingleNodeInstance, AnimSequence, "Timeline##AnimationPreviewDock");
	if (TimelinePanel && DetailsPanel)
	{
		TimelinePanel->SetNotifySelectedCallback([this](int32 Index)
		{
			if (DetailsPanel)
			{
				DetailsPanel->SetSelectedNotify(Index);
			}
		});
	}

	if (!AnimSequence)
		return;

	// 애니메이션을 스켈레톤에 리타겟 (메시가 있으면 메시 스켈레톤에 맞춤)
	if (SkeletalMesh && SkeletalMesh->GetSkeleton())
	{
		AnimSequence->RetargetToSkeleton(SkeletalMesh->GetSkeleton());
	}

	// SingleNodeInstance 생성 및 바인딩 (메시가 없더라도 애니메이션 타임라인과 스켈레톤 트리를 제어할 수 있도록 생성)
	SingleNodeInstance = NewObject<UAnimSingleNodeInstance>();
	if (MeshActor && MeshActor->GetSkeletalMeshComponent())
	{
		MeshActor->GetSkeletalMeshComponent()->SetAnimInstance(SingleNodeInstance);
	}
	SingleNodeInstance->SetAnimation(AnimSequence);
	SingleNodeInstance->Play();

	// 타임라인 패널에 인스턴스 바인딩 전달
	if (TimelinePanel)
	{
		TimelinePanel->SetTarget(SingleNodeInstance, AnimSequence);
	}
}

void FAnimSequencePreviewTab::SetAnimSequence(UAnimSequence* InAnimSequence)
{
	if (!InAnimSequence) return;
	AnimSequence = InAnimSequence;

	const FString AnimName = PathUtils::ToUtf8(std::filesystem::path(AnimSequence->GetPath()).stem());
	SetTitle("Anim: " + AnimName + "###AnimSequencePreview");

	if (AnimSequence->GetSkeleton())
	{
		USkeletalMesh* TargetMesh = AnimSequence->GetSkeleton()->GetPreviewMesh();
		if (TargetMesh && TargetMesh != SkeletalMesh)
		{
			SetPreviewMesh(TargetMesh);
		}
		else if (SkeletalMesh)
		{
			AnimSequence->RetargetToSkeleton(SkeletalMesh->GetSkeleton());
		}
	}

	if (!SingleNodeInstance)
	{
		SingleNodeInstance = NewObject<UAnimSingleNodeInstance>();
		if (MeshActor && MeshActor->GetSkeletalMeshComponent())
		{
			MeshActor->GetSkeletalMeshComponent()->SetAnimInstance(SingleNodeInstance);
		}
	}
	SingleNodeInstance->SetAnimation(AnimSequence);
	SingleNodeInstance->SetPosition(0.0f);
	SingleNodeInstance->Play();

	if (TimelinePanel)
	{
		TimelinePanel->SetTarget(SingleNodeInstance, AnimSequence);
	}
	if (DetailsPanel)
	{
		DetailsPanel->SetTargetSequence(AnimSequence);
		DetailsPanel->SetSelectedNotify(-1);
	}

	FrameMesh();
}

USkeleton* FAnimSequencePreviewTab::GetCurrentSkeleton() const
{
	if (SkeletalMesh && SkeletalMesh->GetSkeleton())
	{
		return SkeletalMesh->GetSkeleton();
	}
	return AnimSequence ? AnimSequence->GetSkeleton() : nullptr;
}

void FAnimSequencePreviewTab::BuildDefaultLayout(ImGuiID DockSpaceRoot)
{
	ImGuiID LeftNode = 0;
	ImGuiID RightNode = 0;
	ImGuiID CenterNode = 0;
	ImGuiID BottomNode = 0;

	// 1. 좌측 20% 분할 (SkeletonTree)
	ImGui::DockBuilderSplitNode(DockSpaceRoot, ImGuiDir_Left, 0.20f, &LeftNode, &CenterNode);
	// 2. 우측 25% 분할 (DetailsPanel)
	ImGui::DockBuilderSplitNode(CenterNode, ImGuiDir_Right, 0.25f, &RightNode, &CenterNode);
	// 3. 중앙 하단 28% 분할 (TimelinePanel)
	ImGui::DockBuilderSplitNode(CenterNode, ImGuiDir_Down, 0.28f, &BottomNode, &CenterNode);

	if (SkeletonTreePanel)
		ImGui::DockBuilderDockWindow(SkeletonTreePanel->GetPanelName(), LeftNode);
	if (DetailsPanel)
		ImGui::DockBuilderDockWindow(DetailsPanel->GetPanelName(), RightNode);
	if (TimelinePanel)
		ImGui::DockBuilderDockWindow(TimelinePanel->GetPanelName(), BottomNode);
	if (ViewportPanel)
		ImGui::DockBuilderDockWindow(ViewportPanel->GetPanelName(), CenterNode);
}

void FAnimSequencePreviewTab::DrawToolbar()
{
	DrawCommonViewToolbar();

	if (!SingleNodeInstance || !AnimSequence)
		return;

	if (ImGui::Button("Save"))
	{
		if (AnimSequence)
		{
			TArray<FModelAnimationNotify> SavedNotifies;
			for (const FAnimNotifyEvent& Notify : AnimSequence->GetNotifies())
			{
				FModelAnimationNotify ModelNotify;
				ModelNotify.TriggerTime = Notify.TriggerTime;
				ModelNotify.Duration = Notify.Duration;
				ModelNotify.NotifyName = Notify.NotifyName;
				SavedNotifies.Add(std::move(ModelNotify));
			}

			const FString AnimationPath = FModelBake::GetAnimationAssetPath(AnimSequence->GetPath());
			if (!AnimationPath.empty() && FModelBake::UpdateAnimationNotifies(AnimationPath, SavedNotifies))
			{
				HTR_LOG(Info, "Saved animation asset: {}", AnimationPath);
			}
			else
			{
				HTR_LOG(Error, "Failed to save animation asset: {}", AnimationPath);
			}
		}
	}
	ImGui::SetItemTooltip("Save animation notifies to .anim file");
	ImGui::SameLine();

	if (ImGui::Button("Reset Pose"))
	{
		SingleNodeInstance->SetPosition(SingleNodeInstance->GetPosition());
	}
	ImGui::SetItemTooltip("Reset manual bone modifications back to animation keyframe");

	ImGui::SameLine();
	ImGui::Separator();
	ImGui::SameLine();

	const FString CurrentAnimName = AnimSequence
		? PathUtils::ToUtf8(std::filesystem::path(AnimSequence->GetPath()).stem())
		: "Select Animation";

	ImGui::TextUnformatted("Anim:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(180.0f);
	if (ImGui::BeginCombo("##AnimSequenceCombo", CurrentAnimName.c_str()))
	{
		TArray<UAnimSequence*> Compatible = GetCompatibleAnimSequences();
		for (UAnimSequence* Anim : Compatible)
		{
			const FString Name = PathUtils::ToUtf8(std::filesystem::path(Anim->GetPath()).stem());
			const bool bSelected = (Anim == AnimSequence);
			if (ImGui::Selectable(Name.c_str(), bSelected))
			{
				SetAnimSequence(Anim);
			}
			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SetItemTooltip("Switch current animation sequence");

	ImGui::SameLine();
	ImGui::Separator();
	ImGui::SameLine();

	ImGui::TextUnformatted("Mesh:");
	ImGui::SameLine();
	const FString CurrentMeshName = (SkeletalMesh && !SkeletalMesh->GetMeshData().Vertices.IsEmpty())
		? PathUtils::ToUtf8(std::filesystem::path(SkeletalMesh->GetPath()).stem())
		: "Skeleton Only (Pick Mesh)";
	ImGui::SetNextItemWidth(160.0f);
	if (ImGui::BeginCombo("##PreviewMeshCombo", CurrentMeshName.c_str()))
	{
		for (TObjectIterator<USkeletalMesh> It; It; ++It)
		{
			if (It->GetMeshData().Vertices.IsEmpty()) continue;
			if (It->GetSkeleton() && SkeletalMesh && SkeletalMesh->GetSkeleton() && !SkeletalMesh->GetSkeleton()->IsCompatibleWith(It->GetSkeleton()))
				continue;

			const FString MeshName = PathUtils::ToUtf8(std::filesystem::path(It->GetPath()).stem());
			const bool bSelected = (SkeletalMesh == *It);
			if (ImGui::Selectable(MeshName.c_str(), bSelected))
			{
				SetPreviewMesh(*It);
			}
			if (bSelected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
}

void FAnimSequencePreviewTab::SetPreviewMesh(USkeletalMesh* InMesh)
{
	if (!InMesh) return;
	SkeletalMesh = InMesh;
	MeshActor->GetSkeletalMeshComponent()->SetSkeletalMesh(SkeletalMesh);
	if (DetailsPanel)
	{
		DetailsPanel->SetTargetMesh(SkeletalMesh);
		DetailsPanel->SetMeshComponent(MeshActor ? MeshActor->GetSkeletalMeshComponent() : nullptr);
	}
	if (AnimSequence && SkeletalMesh->GetSkeleton())
	{
		// 선택한 캐릭터 메시의 스켈레톤에 맞게 애니메이션 리타겟
		AnimSequence->RetargetToSkeleton(SkeletalMesh->GetSkeleton());
		AnimSequence->GetSkeleton()->SetPreviewMesh(SkeletalMesh);
	}
	if (!SingleNodeInstance)
	{
		SingleNodeInstance = NewObject<UAnimSingleNodeInstance>();
		if (MeshActor && MeshActor->GetSkeletalMeshComponent())
		{
			MeshActor->GetSkeletalMeshComponent()->SetAnimInstance(SingleNodeInstance);
		}
		SingleNodeInstance->SetAnimation(AnimSequence);
		SingleNodeInstance->Play();
		if (TimelinePanel)
		{
			TimelinePanel->SetTarget(SingleNodeInstance, AnimSequence);
		}
	}
	FrameMesh();
}

void FAnimSequencePreviewTab::OnBoneGizmoDragStarted()
{
	if (SingleNodeInstance && SingleNodeInstance->IsPlaying())
	{
		SingleNodeInstance->Pause();
	}
}

void FAnimSequencePreviewTab::OnBoneTransformModified()
{
	if (SingleNodeInstance && SingleNodeInstance->IsPlaying())
	{
		SingleNodeInstance->Pause();
	}
}
