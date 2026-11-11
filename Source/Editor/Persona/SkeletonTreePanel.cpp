#include "EnginePCH.h"
#include "SkeletonTreePanel.h"
#include "Render/SkeletalMesh.h"
#include "Animation/Skeleton.h"

namespace
{
	void DrawBoneIcon(ImDrawList* DL, ImVec2 Center, float Size, ImU32 Color)
	{
		const float Half = Size * 0.5f;
		const ImVec2 P1(Center.x - Half * 0.7f, Center.y + Half * 0.7f);
		const ImVec2 P2(Center.x + Half * 0.7f, Center.y - Half * 0.7f);
		DL->AddLine(P1, P2, Color, 2.0f);
		const float R = 2.0f;
		const float Offset = 1.8f;
		DL->AddCircleFilled(ImVec2(P1.x - Offset, P1.y - Offset), R, Color);
		DL->AddCircleFilled(ImVec2(P1.x + Offset, P1.y + Offset), R, Color);
		DL->AddCircleFilled(ImVec2(P2.x - Offset, P2.y - Offset), R, Color);
		DL->AddCircleFilled(ImVec2(P2.x + Offset, P2.y + Offset), R, Color);
	}

	void DrawDotIcon(ImDrawList* DL, ImVec2 Center, float Radius, ImU32 Color)
	{
		DL->AddCircleFilled(Center, Radius, Color);
	}
}

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
	{
		ImGui::TextDisabled("No bones");
		ImGui::End();
		return;
	}

	ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0.11f, 0.11f, 0.11f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
	ImGuiTableFlags TableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;

	if (ImGui::BeginTable("##BoneTreeTable", 1, TableFlags))
	{
		for (const int32 RootBone : RootBones)
			DrawBone(RootBone);
		ImGui::EndTable();
	}
	ImGui::PopStyleColor(2);

	// 빈 곳을 누르면 선택을 푼다
	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
		SelectBone(-1);

	ImGui::End();
}

void FSkeletonTreePanel::DrawBone(int32 BoneIndex)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();

	if (BoneIndex == SelectedBone)
	{
		ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImColor(0, 122, 204, 255));
	}

	const bool bLeaf = Children[BoneIndex].Num() == 0;

	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
		ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_DrawLinesToNodes;
	if (bLeaf)
		Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	if (BoneIndex == SelectedBone)
		Flags |= ImGuiTreeNodeFlags_Selected;

	bool bIsSocketOrLeaf = bLeaf;
	const std::string& BoneName = BoneNames[BoneIndex];
	if (BoneName.find("_end") != std::string::npos ||
		BoneName.find("Socket") != std::string::npos ||
		BoneName.find("Cor_") != std::string::npos)
	{
		bIsSocketOrLeaf = true;
	}

	const bool bOpen = ImGui::TreeNodeEx(
		reinterpret_cast<void*>(static_cast<intptr_t>(BoneIndex)),
		Flags,
		"  %s", BoneNames[BoneIndex].c_str()
	);

	ImDrawList* DL = ImGui::GetWindowDrawList();
	ImVec2 ItemMin = ImGui::GetItemRectMin();
	float RowHeight = ImGui::GetFrameHeight();
	const float IconOffsetX = 4.0f;
	ImVec2 IconCenter = ImVec2(
		ItemMin.x + ImGui::GetTreeNodeToLabelSpacing() + IconOffsetX,
		ItemMin.y + RowHeight * 0.35f
	);

	ImU32 IconColor = (BoneIndex == SelectedBone)
		? IM_COL32(255, 255, 255, 255)
		: IM_COL32(180, 180, 180, 220);

	if (bIsSocketOrLeaf)
	{
		DrawDotIcon(DL, IconCenter, 3.0f, IconColor);
	}
	else
	{
		DrawBoneIcon(DL, IconCenter, 11.0f, IconColor);
	}

	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		SelectBone(BoneIndex);

	if (BoneIndex == SelectedBone && bScrollTreeToSelection)
	{
		ImGui::SetScrollHereY(0.5f);
		bScrollTreeToSelection = false;
	}

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
