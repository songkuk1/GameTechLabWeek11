#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "Editor/FEditorTab.h"

#include "Editor/LevelEditor/LevelEditorTab.h"
#include "Editor/LevelEditor/LevelEditorToolbar.h"

#include <functional> 

class FEditorUI
{
public:
	bool Init(bool bInUseDockSpace = true, bool bInPassthruCentralNode = false);
	void Tick(float DeltaTime);
	void OnRender();

	template <typename T, typename... TArgs>
	T* AddTab(TArgs&&... Args)
	{
		TUniquePtr<T> NewTab = MakeUnique<T>(std::forward<TArgs>(Args)...);
		T* Ret = NewTab.get();
		Tabs.Add(std::move(NewTab));

		return Ret;
	}
	// 탭을 지운다. 레벨 탭(0번)은 지우지 않는다. 탭이 월드를 가지므로 ImGui 프레임 밖(엔진 Tick·종료)에서 부른다
	void RemoveTab(int32 Index) { if (Index > 0 && Index < Tabs.Num()) Tabs.RemoveAt(Index); }
	FLevelEditorToolbar* GetToolbar() const { return Toolbar.get(); }

	void SetNewSceneCallback(std::function<void()> InCallback) { OnNewScene = InCallback; }
	void SetOpenSceneCallback(std::function<void()> InCallback) { OnOpenScene = InCallback; }
	void SetSaveSceneCallback(std::function<void()> InCallback) { OnSaveScene = InCallback; }
	void SetSaveSceneAsCallback(std::function<void()> InCallback) { OnSaveSceneAs = InCallback; }
	//첫번째 탭은 무조건 레벨탭
	FLevelEditorTab* GetLevelTab() const { return static_cast<FLevelEditorTab*>(Tabs[0].get()); }
	//탭마다 도킹 스페이스가 생겨서 탭 이름을 띄워야한다.
	void DrawTabBar(ImGuiDockNodeFlags DockSpaceFlags);

	const TArray<TUniquePtr<FEditorTab>>& GetTabs() const { return Tabs; }

private:
	bool bUseDockSpace = true;
	bool bPassthruCentralNode = false;

	void DrawMainMenuBar();
	void DrawPanelMenuItems(const FEditorTab& Tab);

	TArray<TUniquePtr<FEditorTab>> Tabs;

	TArray<TUniquePtr<IEditorPanel>> Panels;
	TUniquePtr<FLevelEditorToolbar> Toolbar;


	std::function<void()> OnNewScene;
	std::function<void()> OnOpenScene;
	std::function<void()> OnSaveScene;
	std::function<void()> OnSaveSceneAs;
};
