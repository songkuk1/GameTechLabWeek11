#pragma once
#include "EnginePCH.h"
#include "Editor/EditorUI/EditorPanel.h"

class FViewport;
struct FViewportInput;
class FEditorViewportClient;
class FGizmo;
class UWorld;
class UObject;

// 엔진이 탭 대신 렌더할 뷰 하나. 포인터는 모두 비소유이고 이번 프레임 렌더 동안만 유효하므로 보관하지 않는다
struct FEditorTabView
{
	FViewport* Viewport = nullptr;
	// 있으면 Grid 같은 에디터 표시를 함께 그린다
	FEditorViewportClient* OverlayClient = nullptr;
	// 탭이 따로 쓰는 Gizmo. 대상이 이 뷰의 월드에 있을 때만 그린다
	const FGizmo* Gizmo = nullptr;
};

struct FEditorTabSettings
{
	FString Title;
	// imgui.ini에 저장되는 도킹 배치의 키. 비우면 Title을 쓴다
	FString DockSpaceName;

	//닫을 수 있는 탭인지(레벨 탭의 경우 닫을 수 없음)
	bool bClosable = true;

	// 다른 탭의 패널과 서로 도킹되지 않게 막는다
	bool bIsolatePanels = true;
};


class FEditorTab
{
public:
	explicit FEditorTab(FEditorTabSettings InSettings);
	virtual ~FEditorTab() = default;
	FEditorTab(const FEditorTab&) = delete;
	FEditorTab& operator=(const FEditorTab&) = delete;

	// 필요한 Panel을 추가한 뒤 raw pointer를 반환한다(소유권 = Tab)
	template <typename T, typename... TArgs>
	T* AddPanel(TArgs&&... Args)
	{
		TUniquePtr<T> NewPanel = MakeUnique<T>(std::forward<TArgs>(Args)...);
		T* Ret = NewPanel.get();
		Ret->Init();
		Panels.Add(std::move(NewPanel));

		return Ret;
	}

	virtual void Tick(float DeltaTime);
	void RenderPanels();

	const char* GetTitle() const { return Settings.Title.c_str(); }
	bool CanClose() const { return Settings.bClosable; }
	const TArray<TUniquePtr<IEditorPanel>>& GetPanels() const { return Panels; }
	//탭의 DockSpace 이름으로 ImGui ID를 계산해서 DockSpaceId에 넣어 두는 함수
	void ResolveDockSpaceId();
	//선택된 탭은 DockSpace를 그리고, 나머지는 도킹 관계만 유지한다
	void SubmitDockSpace(bool bVisible, ImGuiDockNodeFlags Flags);


	// 탭 바의 닫기 버튼이 내린다
	bool* GetOpenFlag() { return CanClose() ? &bOpen : nullptr; }
	bool IsOpen() const { return bOpen; }
	//열라는 요청이 들어왔는지 확인하고, 요청이 있으면 false로 바꾸고 true를 반환한다.
	bool ConsumeFocusRequest() { const bool bRequested = bFocusRequested; bFocusRequested = false; return bRequested; }
	void RequestFocus() { bFocusRequested = true; }
	// 같은 에셋을 두 번 열면 새 탭 대신 기존 탭을 띄우는 데 쓴다
	virtual const UObject* GetEditingAsset() const { return nullptr; }
	virtual UWorld* GetWorld() const { return nullptr; }

	//엔진이 넘겨준 입력을 탭 안으로 전달
	virtual void RouteInput(const FViewportInput& BaseInput) {};
	//엔진에게 "이 탭에서 그릴 화면"을 알려 줌
	virtual void CollectViews(TArray<FEditorTabView>& OutViews) {};

protected:
	// imgui.ini에 이 탭의 배치가 없을 때 한 번 부른다. 기본은 모든 패널을 한 노드에 겹친다
	// DockBuilder로 DockSpaceRoot를 나누고 패널 창 이름으로 배치한다
	void BuildDefaultLayout(ImGuiID DockSpaceRoot);
	//이 탭이 가지고있는 패널들
	TArray<TUniquePtr<IEditorPanel>> Panels;
private:

	//탭 설정
	FEditorTabSettings Settings;

	ImGuiWindowClass WindowClass;
	ImGuiID DockSpaceId = 0;
	bool bOpen = true;
	bool bFocusRequested = false;

};