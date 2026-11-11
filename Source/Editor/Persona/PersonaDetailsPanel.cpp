#include "EnginePCH.h"
#include "PersonaDetailsPanel.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Component/SkeletalMeshComponent.h"
#include "Render/SkeletalMesh.h"
#include "Render/Material.h"
#include "Sound/SoundSystem.h"
#include "Core/PathUtils.h"
#include "Math/Transform.h"
#include <algorithm>
#include <cmath>
#include <imgui_internal.h>

namespace
{
	FTransform DecomposeMatrix(const FMatrix& M)
	{
		FTransform Out;
		Out.Location = FVector(M[3][0], M[3][1], M[3][2]);

		const FVector X(M[0][0], M[0][1], M[0][2]);
		const FVector Y(M[1][0], M[1][1], M[1][2]);
		const FVector Z(M[2][0], M[2][1], M[2][2]);

		Out.Scale = FVector(X.Size(), Y.Size(), Z.Size());
		if (Out.Scale.X < 0.0001f) Out.Scale.X = 1.0f;
		if (Out.Scale.Y < 0.0001f) Out.Scale.Y = 1.0f;
		if (Out.Scale.Z < 0.0001f) Out.Scale.Z = 1.0f;

		FMatrix Rotation = FMatrix::Identity;
		Rotation.SetAxis(0, X / Out.Scale.X);
		Rotation.SetAxis(1, Y / Out.Scale.Y);
		Rotation.SetAxis(2, Z / Out.Scale.Z);
		Out.Rotation = MatrixToRotator(Rotation);

		return Out;
	}

	bool DrawAxisControl(const FString& _label, float& _value, float _speed, float _minValue, float _maxValue, float _resetValue, FVector4 _color)
	{
		bool isValueChanged = false;

		float lineHeight = GImGui->Font->LegacySize + GImGui->Style.FramePadding.y * 2.0f;
		ImVec4 color(_color.X, _color.Y, _color.Z, _color.W);
		ImVec2 buttonSize = { lineHeight + 2.0f , lineHeight };

		ImGui::PushStyleColor(ImGuiCol_Button, color);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ color.x + 0.1f, color.y + 0.1f, color.z + 0.1f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ color.x - 0.1f, color.y - 0.1f, color.z - 0.1f, 1.0f });

		if (ImGui::Button(_label.c_str(), buttonSize))
		{
			_value = _resetValue;
			isValueChanged = true;
		}
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		std::string dragID = "##" + _label;
		isValueChanged |= ImGui::DragFloat(dragID.c_str(), &_value, _speed, _minValue, _maxValue, "%.2f");

		return isValueChanged;
	}

	bool DrawVector3Controller(const FString& _label, float* _values, float _resetValue, float _columnWidth = 60.0f, float _speed = 0.05f)
	{
		ImGuiIO& io = ImGui::GetIO();
		auto boldFont = !io.Fonts->Fonts.empty() ? io.Fonts->Fonts[0] : nullptr;

		ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;
		bool isVectorChanged = false;

		ImGui::PushID(_label.c_str());
		if (ImGui::BeginTable(_label.c_str(), 2, flags))
		{
			ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, _columnWidth);
			ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthStretch);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::Text("%s", _label.c_str());

			ImGui::TableSetColumnIndex(1);
			ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 2, 0 });
			if (boldFont) ImGui::PushFont(boldFont);

			isVectorChanged |= DrawAxisControl("X", _values[0], _speed, 0.0f, 0.0f, _resetValue, { 0.8f, 0.1f, 0.1f, 1.0f });

			ImGui::PopItemWidth();

			ImGui::SameLine();
			isVectorChanged |= DrawAxisControl("Y", _values[1], _speed, 0.0f, 0.0f, _resetValue, { 0.1f, 0.8f, 0.1f, 1.0f });

			ImGui::PopItemWidth();

			ImGui::SameLine();
			isVectorChanged |= DrawAxisControl("Z", _values[2], _speed, 0.0f, 0.0f, _resetValue, { 0.1f, 0.1f, 0.8f, 1.0f });

			ImGui::PopItemWidth();

			if (boldFont) ImGui::PopFont();
			ImGui::PopStyleVar();
			ImGui::EndTable();
		}

		ImGui::PopID();

		return isVectorChanged;
	}

	bool DrawRotatorAsXYZ(const FString& _label, FRotator& _rotator, float _columnWidth = 60.0f, float _speed = 0.5f)
	{
		float Euler[3] = { _rotator.Roll, _rotator.Pitch, _rotator.Yaw };
		const bool isChanged = DrawVector3Controller(_label, Euler, 0.0f, _columnWidth, _speed);
		if (isChanged)
		{
			_rotator.Roll = Euler[0];
			_rotator.Pitch = Euler[1];
			_rotator.Yaw = Euler[2];
		}
		return isChanged;
	}
}

FPersonaDetailsPanel::FPersonaDetailsPanel(FString InPanelName)
	: PanelName(std::move(InPanelName))
{
}

void FPersonaDetailsPanel::OnRender()
{
	if (!ImGui::Begin(PanelName.c_str()))
	{
		ImGui::End();
		return;
	}

	// 1. 선택된 노티파이가 있으면 노티파이 세부 속성을 최상단에 표시
	if (AnimSequence && SelectedNotifyIndex >= 0 && SelectedNotifyIndex < AnimSequence->GetNotifies().Num())
	{
		DrawNotifyDetails();
		ImGui::Separator();
	}

	// 2. 선택된 본 정보가 있으면 표시
	if (SelectedBoneIndex >= 0)
	{
		DrawBoneDetails();
		ImGui::Separator();
	}

	// 3. 에셋 기본 정보 (애니메이션 or 스켈레탈 메시)
	DrawAssetDetails();

	ImGui::End();
}

void FPersonaDetailsPanel::DrawNotifyDetails()
{
	if (!AnimSequence || SelectedNotifyIndex < 0 || SelectedNotifyIndex >= AnimSequence->GetNotifies().Num())
		return;

	FAnimNotifyEvent& Notify = AnimSequence->GetMutableNotifies()[SelectedNotifyIndex];
	const float Duration = AnimSequence->GetPlayLength();
	const float FrameRate = AnimSequence->GetFrameRate() > 0.0f ? AnimSequence->GetFrameRate() : 30.0f;

	if (ImGui::CollapsingHeader("Selected Notify Details", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Notify Index: %d", SelectedNotifyIndex);

		// 트리거 시간 편집
		float TriggerTime = Notify.TriggerTime;
		if (ImGui::DragFloat("Trigger Time", &TriggerTime, 0.01f, 0.0f, Duration, "%.3f s"))
		{
			Notify.TriggerTime = std::clamp(TriggerTime, 0.0f, Duration);
		}

		// 프레임 번호 편집
		int32 Frame = static_cast<int32>(std::round(Notify.TriggerTime * FrameRate));
		const int32 MaxFrame = AnimSequence->GetTotalFrames() > 0 ? AnimSequence->GetTotalFrames() - 1 : 0;
		if (ImGui::DragInt("Frame", &Frame, 1.0f, 0, MaxFrame))
		{
			Notify.TriggerTime = std::clamp(static_cast<float>(Frame) / FrameRate, 0.0f, Duration);
		}

		// 지속 시간(Duration) 편집
		float NotifyDuration = Notify.Duration;
		if (ImGui::DragFloat("Duration", &NotifyDuration, 0.01f, 0.0f, Duration - Notify.TriggerTime, "%.3f s"))
		{
			Notify.Duration = std::max(0.0f, NotifyDuration);
		}

		ImGui::Separator();

		// 커스텀 이름 기반 노티파이 (HitCheck 등)
		ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Notify Type: Custom Event");
		char NameBuf[128] = {};
		const FString CurrentName = Notify.NotifyName.ToString();
		strncpy_s(NameBuf, CurrentName.c_str(), sizeof(NameBuf) - 1);
		if (ImGui::InputText("Event Name", NameBuf, sizeof(NameBuf)))
		{
			Notify.NotifyName = FName(NameBuf);
		}

		ImGui::Spacing();
		if (ImGui::Button("Deselect"))
		{
			SelectedNotifyIndex = -1;
		}
		ImGui::SameLine();
		if (ImGui::Button("Delete Notify"))
		{
			AnimSequence->RemoveNotify(SelectedNotifyIndex);
			SelectedNotifyIndex = -1;
		}
	}
}

void FPersonaDetailsPanel::DrawAssetDetails()
{
	if (AnimSequence)
	{
		if (ImGui::CollapsingHeader("Animation Asset Details", ImGuiTreeNodeFlags_DefaultOpen))
		{
			const FString AnimName = PathUtils::ToUtf8(PathUtils::FromUtf8(AnimSequence->GetPath()).filename());
			ImGui::Text("Asset: %s", AnimName.c_str());
			ImGui::TextDisabled("Path: %s", AnimSequence->GetPath().c_str());
			ImGui::Separator();

			ImGui::Text("Length: %.3f s", AnimSequence->GetPlayLength());
			ImGui::Text("Frame Rate: %.1f fps", AnimSequence->GetFrameRate());
			ImGui::Text("Total Frames: %d", AnimSequence->GetTotalFrames());
			const int32 TrackCount = AnimSequence->GetDataModel() ? AnimSequence->GetDataModel()->GetBoneAnimationTracks().Num() : 0;
			ImGui::Text("Tracks Count: %d", TrackCount);
			ImGui::Text("Notifies Count: %d", AnimSequence->GetNotifies().Num());

			bool bLock = AnimSequence->IsForceRootLock();
			if (ImGui::Checkbox("Force Root Lock", &bLock))
			{
				AnimSequence->SetForceRootLock(bLock);
			}
			ImGui::SetItemTooltip("Locks root bone motion to origin for In-Place animation preview");

			if (AnimSequence->GetSkeleton())
			{
				const FString SkelName = PathUtils::ToUtf8(PathUtils::FromUtf8(AnimSequence->GetSkeleton()->GetPath()).filename());
				ImGui::Text("Skeleton: %s", SkelName.c_str());
			}
		}
	}
	else if (SkeletalMesh)
	{
		if (ImGui::CollapsingHeader("Skeletal Mesh Details", ImGuiTreeNodeFlags_DefaultOpen))
		{
			const FString MeshName = PathUtils::ToUtf8(PathUtils::FromUtf8(SkeletalMesh->GetPath()).filename());
			ImGui::Text("Asset: %s", MeshName.c_str());
			ImGui::TextDisabled("Path: %s", SkeletalMesh->GetPath().c_str());
			ImGui::Separator();

			const FModelMeshData& MeshData = SkeletalMesh->GetMeshData();
			ImGui::Text("Vertices: %d", MeshData.Vertices.Num());
			ImGui::Text("Triangles: %d", MeshData.Indices.Num() / 3);
			ImGui::Text("Bones: %d", SkeletalMesh->GetSkeleton() ? SkeletalMesh->GetSkeleton()->GetBones().Num() : 0);
			ImGui::Text("Material Slots: %d", SkeletalMesh->GetMaterials().Num());

			if (ImGui::TreeNode("Materials"))
			{
				const auto& Materials = SkeletalMesh->GetMaterials();
				for (int32 i = 0; i < Materials.Num(); ++i)
				{
					ImGui::BulletText("Slot %d: %s", i, Materials[i] ? PathUtils::ToUtf8(PathUtils::FromUtf8(Materials[i]->GetPath()).filename()).c_str() : "None");
				}
				ImGui::TreePop();
			}
		}
	}
}

void FPersonaDetailsPanel::DrawBoneDetails()
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeleton() || SelectedBoneIndex < 0)
		return;

	const USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
	const auto& Bones = Skeleton->GetBones();
	const auto& Nodes = Skeleton->GetNodes();

	if (SelectedBoneIndex >= Bones.Num())
		return;

	const FModelBone& Bone = Bones[SelectedBoneIndex];
	const int32 NodeIndex = Bone.NodeIndex;
	const FString BoneName = (NodeIndex >= 0 && NodeIndex < Nodes.Num()) ? Nodes[NodeIndex].Name : "(Unnamed)";
	const int32 ParentIndex = Bone.ParentBoneIndex;
	FString ParentName = "None (Root)";
	if (ParentIndex >= 0 && ParentIndex < Bones.Num())
	{
		const int32 ParentNodeIndex = Bones[ParentIndex].NodeIndex;
		ParentName = (ParentNodeIndex >= 0 && ParentNodeIndex < Nodes.Num()) ? Nodes[ParentNodeIndex].Name : "(Unnamed)";
	}

	if (ImGui::CollapsingHeader("Selected Bone", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// 1. Reset Pose 및 Reset Bone 버튼
		if (MeshComponent)
		{
			if (ImGui::Button("Reset Pose"))
			{
				MeshComponent->ResetPose();
				MeshComponent->UpdateBoneMatrices();
				if (OnBoneModified)
				{
					OnBoneModified();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Reset Bone"))
			{
				MeshComponent->ResetBone(SelectedBoneIndex);
				if (OnBoneModified)
				{
					OnBoneModified();
				}
			}
		}

		// 2. 본 기본 정보
		ImGui::SeparatorText("Bone Info");
		ImGui::Text("Bone Name: %s", BoneName.c_str());
		ImGui::Text("Bone Index: %d", SelectedBoneIndex);
		ImGui::Text("Parent Bone: %s (Index: %d)", ParentName.c_str(), ParentIndex);

		// 3. 로컬 트랜스폼 조작 (Location, Rotation, Scale)
		ImGui::SeparatorText("Transform (Local)");
		if (MeshComponent)
		{
			FMatrix LocalMat = MeshComponent->GetBoneLocalMatrix(SelectedBoneIndex);
			FTransform LocalTransform = DecomposeMatrix(LocalMat);

			float LocValues[3] = { LocalTransform.Location.X, LocalTransform.Location.Y, LocalTransform.Location.Z };
			FRotator Rot = LocalTransform.Rotation;
			float ScaleValues[3] = { LocalTransform.Scale.X, LocalTransform.Scale.Y, LocalTransform.Scale.Z };

			bool bModified = false;
			bModified |= DrawVector3Controller("Location", LocValues, 0.0f, 60.0f, 0.05f);
			bModified |= DrawRotatorAsXYZ("Rotation", Rot, 60.0f, 0.5f);
			bModified |= DrawVector3Controller("Scale", ScaleValues, 1.0f, 60.0f, 0.01f);

			if (bModified)
			{
				FTransform NewLocal(
					FVector(LocValues[0], LocValues[1], LocValues[2]),
					FVector(ScaleValues[0], ScaleValues[1], ScaleValues[2]),
					Rot
				);
				MeshComponent->SetBoneLocalMatrix(SelectedBoneIndex, NewLocal.GetLocalMatrix());
				if (OnBoneModified)
				{
					OnBoneModified();
				}
			}

			// 4. 컴포넌트 기준 글로벌 트랜스폼 (읽기 전용)
			ImGui::SeparatorText("Transform (Component)");
			FMatrix GlobalMat = MeshComponent->GetBoneComponentMatrix(SelectedBoneIndex);
			FTransform GlobalTransform = DecomposeMatrix(GlobalMat);
			ImGui::TextDisabled("Location: (%.2f, %.2f, %.2f)", GlobalTransform.Location.X, GlobalTransform.Location.Y, GlobalTransform.Location.Z);
			ImGui::TextDisabled("Rotation: (%.1f, %.1f, %.1f)", GlobalTransform.Rotation.Roll, GlobalTransform.Rotation.Pitch, GlobalTransform.Rotation.Yaw);
		}
	}
}
