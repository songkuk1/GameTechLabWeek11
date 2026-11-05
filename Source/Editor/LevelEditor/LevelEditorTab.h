#pragma once
#include "Editor/FEditorTab.h"

#include <functional>
class FViewportsPanel;


// 항상 첫 번째에 있는 레벨 편집 탭. 패널 포인터와 연결은 엔진이 갖고, 이 탭은 레벨 뷰포트를 렌더 목록으로 넘긴다
class FLevelEditorTab : public FEditorTab
{
public:
	FLevelEditorTab();
	virtual ~FLevelEditorTab() = default;;

	FLevelEditorTab(const FLevelEditorTab&) = delete;
	FLevelEditorTab& operator=(const FLevelEditorTab&) = delete;

	// 레벨 뷰포트 슬롯을 전부 넘긴다. 보이는지는 엔진이 다른 탭과 같은 조건으로 거른다
	void CollectViews(TArray<FEditorTabView>& OutViews) override;

	void SetViewportsPanel(FViewportsPanel* InViewportsPanel) { ViewportsPanel = InViewportsPanel; }
	void SetGizmo(const FGizmo* InGizmo) { Gizmo = InGizmo; }
	// PIE 중이고 일시정지가 아닌 슬롯은 Grid·Gizmo 같은 에디터 표시를 그리지 않는다
	void SetIsPlayingViewQuery(std::function<bool(int32)> InQuery) { IsPlayingViewQuery = std::move(InQuery); }

private:
	//뷰포트를 담는 패널. FLevelEditorTab이 소유한다.
	FViewportsPanel* ViewportsPanel = nullptr;
	const FGizmo* Gizmo = nullptr; // 엔진 소유
	std::function<bool(int32)> IsPlayingViewQuery;
};
