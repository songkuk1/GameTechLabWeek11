#pragma once
#include "EnginePCH.h"
#include "Editor/EditorUI/EditorPanel.h"

#include <functional>

class USkeletalMesh;

// 메시의 본 계층을 트리로 보여 주고, 클릭한 본을 선택 콜백으로 알린다
class FSkeletonTreePanel : public IEditorPanel
{
public:
	// InPanelName은 ImGui 창 이름이라 탭마다 달라야 한다. "Skeleton Tree##탭이름"처럼 넘긴다
	FSkeletonTreePanel(USkeletalMesh* InSkeletalMesh, FString InPanelName);

	// 본 부모 인덱스로 자식 목록과 본 이름을 한 번 만든다. AddPanel이 부른다
	bool Init() override;
	void Tick(float DeltaTime) override {}
	void OnRender() override;
	const char* GetPanelName() const override { return PanelName.c_str(); }

	// 선택이 바뀌면 본 인덱스로 부른다. 선택 해제는 -1이다
	void SetBoneSelectedCallback(std::function<void(int32)> InCallback) { OnBoneSelected = std::move(InCallback); }
	int32 GetSelectedBone() const { return SelectedBone; }

private:
	void DrawBone(int32 BoneIndex);
	void SelectBone(int32 BoneIndex);

	USkeletalMesh* SkeletalMesh = nullptr;
	FString PanelName;

	// [본] = 자식 본 인덱스들. 루트는 RootBones에 따로 둔다
	TArray<TArray<int32>> Children;
	TArray<int32> RootBones;
	// [본] = 본 이름. 본 번호와 노드 번호가 달라서 Init에서 노드 이름을 본 순서로 옮겨 둔다
	TArray<FString> BoneNames;
	// 선택 안 함은 -1이라 부호 있는 정수여야 한다
	int32 SelectedBone = -1;

	std::function<void(int32)> OnBoneSelected;
};
