#pragma once
#include "Editor/EditorUI/EditorPanel.h"

#include "Editor/Gizmo/Gizmo.h"
#include "Editor/LevelEditor/ActorSpawnList.h"

class FViewportsPanel;

// 액터 생성과 카메라·기즈모 편집에 필요한 패널 상태를 보관한다.
// 에디터는 같은 기능을 레벨 툴바와 뷰포트 상단 메뉴로 옮겨 이 패널을 쓰지 않는다. 지금은 Benchmark 앱만 쓴다.
class FEditorControlsPanel : public IEditorPanel
{
public:
	bool Init() override;
	void Tick(float DeltaTime)override;
	void OnRender() override;
	const char* GetPanelName() const override { return "Editor Controls"; }
	inline void SetGizmo(FGizmo* InGizmo) { Gizmo = InGizmo; }
	inline void SetWorld(UWorld* InWorld) { World = InWorld; }
	inline void SetIsPlayingQuery(std::function<bool()> InQuery) { IsPlayingQuery = InQuery; }

	float DeltaTime = 1.0f;
	UWorld* World = nullptr;

	void AddActor(uint32 Index);

	int32 SelectedIndex = 0;

	FGizmo* Gizmo = nullptr;
	int32 GizmoSelectedIndex = 0;
	const char* GizmoItems[3] ={"Location","Rotation","Scale"};

	int32 SpaceSelectedIndex = 0;
	const char* SpaceItems[2] ={"Local","World"};

    void SetViewportsPanel(FViewportsPanel* InViewportsPanel) { ViewportsPanel = InViewportsPanel; }

private:
	static constexpr float SectionGap = 10.0f;
	static constexpr float SubsectionGap = 4.0f;

    void DrawCameraProperties();
    FViewportsPanel* ViewportsPanel = nullptr;
	std::function<bool()> IsPlayingQuery;

};
