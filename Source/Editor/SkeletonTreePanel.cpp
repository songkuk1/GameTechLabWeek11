#include "EnginePCH.h"
#include "SkeletonTreePanel.h"
#include "Render/SkeletalMesh.h"
#include "Animation/Skeleton.h"

FSkeletonTreePanel::FSkeletonTreePanel(USkeletalMesh* InSkeletalMesh, FString InPanelName)
	: SkeletalMesh(InSkeletalMesh), PanelName(std::move(InPanelName))
{
}


bool FSkeletonTreePanel::Init()
{
	// 다시 불려도 같은 결과가 되도록 비우고 시작한다
	Children.Reset();
	RootBones.Reset();
	BoneNames.Reset();

	// 스켈레톤이 없는 메시(임포트 실패 등)는 빈 트리로 둔다
	USkeleton* Skeleton = SkeletalMesh ? SkeletalMesh->GetSkeleton() : nullptr;
	if (!Skeleton)
		return true;

	const TArray<FModelBone>& Bones = Skeleton->GetBones();
	const TArray<FModelNode>& Nodes = Skeleton->GetNodes();
	Children.SetNum(Bones.Num());
	BoneNames.SetNum(Bones.Num());
	for (int32 BoneIndex = 0; BoneIndex < Bones.Num(); ++BoneIndex)
	{
		// 본 번호와 노드 번호는 다르다. 이름은 본이 가리키는 노드에 있다
		const int32 NodeIndex = Bones[BoneIndex].NodeIndex;
		BoneNames[BoneIndex] = NodeIndex >= 0 && NodeIndex < Nodes.Num() ? Nodes[NodeIndex].Name : FString("(Unnamed)");

		const int32 ParentIndex = Bones[BoneIndex].ParentBoneIndex;
		if (ParentIndex < 0 || ParentIndex >= Bones.Num())
			RootBones.Add(BoneIndex);
		else
			Children[ParentIndex].Add(BoneIndex);
	}
	return true;
}

void FSkeletonTreePanel::OnRender()
{
	if (!ImGui::Begin(PanelName.c_str()))
	{
		ImGui::End();
		return;
	}

	if (RootBones.Num() == 0)
		ImGui::TextDisabled("No bones");
	for (const int32 RootBone : RootBones)
		DrawBone(RootBone);

	// 빈 곳을 누르면 선택을 푼다
	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
		SelectBone(-1);

	ImGui::End();
}

void FSkeletonTreePanel::DrawBone(int32 BoneIndex)
{
	const bool bLeaf = Children[BoneIndex].Num() == 0;

	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
	if (bLeaf)
		Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	if (BoneIndex == SelectedBone)
		Flags |= ImGuiTreeNodeFlags_Selected;

	const bool bOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(BoneIndex)), Flags, "%s", BoneNames[BoneIndex].c_str());
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		SelectBone(BoneIndex);

	if (bOpen && !bLeaf)
	{
		for (const int32 Child : Children[BoneIndex])
			DrawBone(Child);
		ImGui::TreePop();
	}
}

void FSkeletonTreePanel::SelectBone(int32 BoneIndex)
{
	if (SelectedBone == BoneIndex)
		return;
	SelectedBone = BoneIndex;
	if (OnBoneSelected)
		OnBoneSelected(SelectedBone);
}
