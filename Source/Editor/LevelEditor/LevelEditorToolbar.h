#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS

#include "Editor/EditorUI/EditorPanel.h"
#include "Editor/EditorUI/EditorCommand.h"
#include "Editor/HitoriEd/PlayInEditorDataTypes.h"

class UClass;

class FLevelEditorToolbar
{
public:
	~FLevelEditorToolbar() = default;

	void SetCommands(const TArray<FEditorCommand>& InCommands) { Commands = InCommands; }
	// Play 그룹 끝에 PIE 실행 위치 선택 메뉴를 붙인다. 엔진이 상태를 소유한다.
	void SetPlayModeBinding(std::function<EPlayModeType()> InGetPlayMode,
		std::function<void(EPlayModeType)> InSetPlayMode, std::function<bool()> InCanChangePlayMode)
	{
		GetPlayMode = std::move(InGetPlayMode);
		SetPlayMode = std::move(InSetPlayMode);
		CanChangePlayMode = std::move(InCanChangePlayMode);
	}
	// Add 메뉴에서 고른 액터 클래스를 엔진에 넘긴다. CanSpawn이 false면 메뉴를 비활성화한다.
	void SetActorSpawnBinding(std::function<void(UClass*)> InSpawnActor, std::function<bool()> InCanSpawnActor)
	{
		SpawnActor = std::move(InSpawnActor);
		CanSpawnActor = std::move(InCanSpawnActor);
	}

	void Draw();

	static constexpr float Height = 40.0f;

private:
	void DrawFileGroup();
	void DrawPlayGroup();
	void DrawPlayModeMenu();
	void DrawSpawnGroup();

	bool bIsOpen = true;
	std::function<void(UClass*)> SpawnActor;
	std::function<bool()> CanSpawnActor;
	TArray<FEditorCommand> Commands;
	std::function<EPlayModeType()> GetPlayMode;
	std::function<void(EPlayModeType)> SetPlayMode;
	std::function<bool()> CanChangePlayMode;
};
