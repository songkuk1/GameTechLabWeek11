#pragma once

#include <format>
#include "Editor/EditorUI/EditorPanel.h"

#include "Engine/World.h"

struct FTransform;
class USkeletalMesh;

using DetailsSelectionCallback = std::function<void(UActorComponent*)>;
using DetailsDeleteComponentCallback = std::function<void(UActorComponent*)>;
using DetailsOpenPreviewCallback = std::function<void(USkeletalMesh*)>;

class FDetailsPanel : public IEditorPanel
{
public:
	FDetailsPanel() = default;
	~FDetailsPanel();

	bool Init() override;
	void Tick(float DeltaTime) override;
	void OnRender() override;

	void SelectComponent(UActorComponent* Component);
	void DrawCompoenetList(AActor* SelectedActor, UActorComponent*& OutSelectedComponent);

	const char* GetPanelName() const override { return "Details"; }

	void SetTarget(AActor* InTargetActor) 
	{ 
		TargetActor = InTargetActor;
		TargetComponent = InTargetActor ? InTargetActor->GetRootComponent() : nullptr;
	}
	UActorComponent* GetTargetComponent() const { return TargetComponent; }
	void SetTargetComponent(UActorComponent* InTargetComponent) { TargetComponent = InTargetComponent; }

	void SetSelectionCallback(DetailsSelectionCallback InCallback) { Callback = InCallback; }
	void SetDeleteComponentCallback(DetailsDeleteComponentCallback InCallback) { DeleteCallback = InCallback; }
	// 스켈레탈 메시 컴포넌트의 Open Preview 버튼이 부른다
	void SetOpenPreviewCallback(DetailsOpenPreviewCallback InCallback) { OpenPreviewCallback = InCallback; }

	void SetWorld(UWorld* InWorld) { World = InWorld; }

	ImFont* GetCustomFont() { return CustomFont; }

private:
	void DrawSceneComponentNode(USceneComponent* Component, USceneComponent* Root, UActorComponent*& OutSelectedComponent);
	void TryReparent(USceneComponent* DroppedComponent, USceneComponent* DragComponent);

	UWorld* World = nullptr;
	ImFont* CustomFont = nullptr;

	AActor* TargetActor = nullptr;
	UActorComponent* TargetComponent = nullptr;
	DetailsSelectionCallback Callback;
	DetailsDeleteComponentCallback DeleteCallback;
	DetailsOpenPreviewCallback OpenPreviewCallback;

	UActorComponent* PendingDeleteComponent = nullptr;
	UActorComponent* EditingNameComponent = nullptr;
	char ComponentNameEditBuffer[256] = {};
	bool bFocusNameEdit = false;
	bool bIsEditingName = false;

	USceneComponent* PendingDroppedComponent = nullptr;
	USceneComponent* PendingDragComponent = nullptr;

	// 우클릭 "Set as Root"로 고른 컴포넌트. 트리를 그리는 도중에 계층을 바꾸지 않도록 다음 프레임에 반영한다.
	USceneComponent* PendingRootComponent = nullptr;
};

