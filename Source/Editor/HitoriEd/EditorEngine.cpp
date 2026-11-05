#include "EnginePCH.h"

#include "Editor/HitoriEd/EditorEngine.h"

#include "Core/EngineStatics.h"
#include "Core/EngineTimer.h"
#include "Launch/LaunchEngineLoop.h"
#include "Core/StatOverlay.h"
#include "Input/InputSystem.h"

#include "GameFramework/Actor/PlayerActor.h"
#include "Script/LuaScriptSystem.h"

#include "Render/GeometryGenerator.h"

#include "Engine/World.h"
#include "Engine/Level.h"

#include "Render/Renderer.h"

#include "Asset/AssetManager.h"
#include "Render/RenderResourceManager.h"

#include "Render/RenderCommand.h"
#include "Render/ColorWorkflow.h"
#include "Editor/Outliner/OutlinerPanel.h"
#include "Editor/HitoriEd/EditorFileUtils.h"
#include "Serialization/JsonArchive.h"
#include "Editor/HitoriEd/EditorResourcePaths.h"
#include "UObject/UObjectIterator.h"
#include "Component/SpotLightComponent.h"
#include "Core/ResourcePaths.h"

#include "Core/EngineLog.h"
#include "Core/Stats/LightweightStats.h"
#include "Core/Stats/EditorStats.h"
#include "Collision/HitResult.h"
#include "Component/BillboardComponent.h"
#include "Component/PrimitiveComponent.h"

#include "Engine/LightSceneProxy.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "Component/ShapeComponent.h"
#include "Component/DecalComponent.h"
#include "Component/SkinnedMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Editor/SkeletalMeshPreviewTab.h"

namespace
{
	DECLARE_CYCLE_STAT("Viewport Update", STAT_ViewportUpdate);
	DECLARE_CYCLE_STAT("World Tick", STAT_WorldTick);
	DECLARE_CYCLE_STAT("Editor Tick", STAT_EditorTick);
	DECLARE_CYCLE_STAT("Capture World", STAT_CaptureWorld);
	DECLARE_CYCLE_STAT("Build Render Queue", STAT_BuildRenderQueue);
	DECLARE_CYCLE_STAT("ImGui", STAT_ImGui);

	// 렌더 캡처 목록에 없는 컴포넌트도 Hit ID로 찾아 Owner를 반환한다.
	AActor* FindActorByPrimitiveId(const uint32 PrimitiveId)
	{
		for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
		{
			if (It->GetUUID() == PrimitiveId)
				return It->GetOwner();
		}
		return nullptr;
	}
}

#include "Core/SplashScreen.h"
#include "Core/Delegate.h"

FEngineConfig UEditorEngine::GetConfig() const
{
	FEngineConfig Desc;
	Desc.Title = L"Hitori Engine";
	Desc.Width = 1920;
	Desc.Height = 1080;
	Desc.bBorderless = false;
	Desc.SyncInterval = 0;
	// View는 각자 깊이 버퍼를 쓰고 백버퍼에는 ImGui만 그린다.
	Desc.bCreateDepthBuffer = false;
	Desc.bExitOnEscape = false;
	// 파일이 없으면 검은 배경에 상태 텍스트만 표시된다.
	Desc.SplashImage = ResourcePaths::SplashImage;
	return Desc;
}

// 렌더 자원·월드·에디터와 MultipleViewports 연결을 초기화한다.
// Device·Window·Swapchain·AssetManager는 FEngineLoop가 먼저 만들어 둔다.
bool UEditorEngine::Init()
{
	if (!Super::Init())
		return false;

	FWorldContext& InitialWorldContext = CreateNewWorldContext(EWorldType::Editor);
	InitialWorldContext.SetCurrentWorld(UWorld::CreateWorld(EWorldType::Editor, true));
	EditorWorld = InitialWorldContext.World();

	MainWindow = GetEngineLoop().GetMainWindow();
	MainWindowSC = GetEngineLoop().GetSwapchain();
	Renderer = GetEngineLoop().GetRenderer();
	FRenderDevice* RenderDevice = GetEngineLoop().GetRenderDevice();

	EditorUI = MakeUnique<FEditorUI>();
	EditorUI->Init();

	EditorUI->SetNewSceneCallback([this]() { CreateNewScene(); });
	EditorUI->SetOpenSceneCallback([this]() { OpenScene(); });
	EditorUI->SetSaveSceneCallback([this]() { SaveCurrentScene(); });
	EditorUI->SetSaveSceneAsCallback([this]() { SaveSceneAs(); });

	OutputLogPanel = EditorUI->GetLevelTab()->AddPanel<FOutputLogPanel>();
	FLog::AddSink(OutputLogPanel);
	HTR_LOG(Info, "Editor Initialize...");

	HTR_LOG(Info, "Initialize ImGui...");
	ImGuiRenderer = MakeUnique<FImGuiRenderer>();
	if (!ImGuiRenderer->Init(MainWindow->GetHandle(), RenderDevice->GetDevice(), RenderDevice->GetContext()))
	{
		HTR_LOG(Error, "Failed To Initialize ImGui!");
	}
	HTR_LOG(Info, "Initialize ImGui Success!");

	GridRenderer = MakeUnique<FGridRenderer>();
	GridRenderer->Init(Renderer);

	GizmoRenderer = MakeUnique<FGizmoRenderer>();
	GizmoRenderer->Init(Renderer);

	Gizmo = MakeUnique<FGizmo>();

	// 필요한 Panel들 추가후 raw pointer 반환(소유권 = EditorUI)
	DetailsPanel = EditorUI->GetLevelTab()->AddPanel<FDetailsPanel>();
	ViewportsPanel = EditorUI->GetLevelTab()->AddPanel<FViewportsPanel>();
	ContentDrawerPanel = EditorUI->GetLevelTab()->AddPanel<FContentDrawerPanel>();
	LevelSequencerPanel = EditorUI->GetLevelTab()->AddPanel<FLevelSequencePanel>();
	ContentDrawerPanel->SetOpenLevelSequenceBinding([this](const FString& Path)
	{
		LevelSequencerPanel->OpenSequenceFile(Path);
	});

	EditorUI->GetLevelTab()->SetGizmo(Gizmo.get());
	EditorUI->GetLevelTab()->SetViewportsPanel(ViewportsPanel);
	EditorUI->GetLevelTab()->SetIsPlayingViewQuery([this](int32 ViewIndex) { return IsPlayingView(ViewIndex); });

	SceneRenderer.Init(Renderer);

	// OutLine
	OutlineRenderer = MakeUnique<FOutlineRenderer>();
	OutlineRenderer->Init(Renderer);

	SettingsPanel = EditorUI->GetLevelTab()->AddPanel<FSettingsPanel>();

	//ToolbarPanel->SetPlayCallback([this]() { RequestPlaySession(); });
	//ToolbarPanel->SetStopCallback([this]() { RequestEndPlayMap(); });
	//ToolbarPanel->SetIsPlayingQuery([this]() { return PlayWorld != nullptr; });

	Outline = MakeUnique<FOutline>();

	SystemFont = UAssetManager::GetAssetByPath<UFont>(ResourcePaths::DefaultFont);

	TextRenderer = MakeUnique<FTextRenderer>();
	TextRenderer->Init();

	// 투영 행렬 생성 
	ViewportsPanel->InitializeDefaultViews();
	// 레벨 View 편집 Client는 모두 EditorWorld를 편집한다.
	for (int32 ViewIndex = 0; ViewIndex < FViewportsPanel::ViewCount; ++ViewIndex)
		ViewportsPanel->GetEditorClient(ViewIndex).SetWorld(EditorWorld);
	// 화면 나눔 비율 설정 가져오기
	ViewportsPanel->SetSplitRatio({
		SettingsPanel->GetSettings().MultipleViewportsHorizontal,
		SettingsPanel->GetSettings().MultipleViewportsVertical });
	// SingleView에 사용할 인덱스 설정
	ViewportsPanel->SetSingleViewIndex(
		SettingsPanel->GetSettings().MultipleViewportsSingleViewIndex);
	// 뷰포트 레이아웃 설정
	ViewportsPanel->SetLayoutMode(
		SettingsPanel->GetSettings().bMultipleViewportsSingle
		? ELayoutMode::Single
		: ELayoutMode::QuadSplit);

	/// 삭제 예정
	//SceneManager = EditorUI->AddEditorPanel<FSceneManager>();
	//SceneManager->SetWorld(World);

	OutlinerPanel = EditorUI->GetLevelTab()->AddPanel<FOutlinerPanel>();
	OutlinerPanel->SetWorld(GetActiveWorld());
	// 아웃라이너·뷰포트 피킹이 같은 선택 목록을 쓰고, 바뀌면 한곳에서 기즈모·디테일·외곽선을 맞춘다.
	OutlinerPanel->SetSelection(&Selection);
	Selection.SetChangedCallback([this]() { SyncSelectionToEditors(); });
	// 편집 월드의 액터 파괴와 에디터 참조 정리 연결
	EditorWorld->OnActorDestroyed.AddRaw(this, &UEditorEngine::HandleActorDestroyed);
	DetailsPanel->SetSelectionCallback(
		[this](UActorComponent* Component)
		{
			if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
			{
				AActor* Owner = SceneComponent->GetOwner();
				if (Owner && SceneComponent == Owner->GetRootComponent())
				{
					Gizmo->SetTarget(SceneComponent, true);
				}
				else
				{
					Gizmo->SetTarget(SceneComponent);
				}
			}
			else
			{
				Gizmo->SetTarget(nullptr);
			}
		}
	);

	OutlinerPanel->SetDeleteActorCallback(
		[this](AActor* Actor)
		{
			// 선택된 액터에서 Delete를 고르면 선택 전체를 지운다.
			if (Selection.IsSelected(Actor))
				DeleteSelectedActors();
			else
				DeleteActor(Actor);
		}
	);
	DetailsPanel->SetDeleteComponentCallback(
		[this](UActorComponent* Component)
		{
			DeleteComponent(Component);
		}
	);
	// 버튼은 ImGui가 탭 목록을 도는 중에 눌리므로 바로 탭을 만들지 않고, 다음 Tick의 UpdateEditorTabs에서 연다
	DetailsPanel->SetOpenPreviewCallback(
		[this](USkeletalMesh* SkeletalMesh)
		{
			PendingPreviewMesh = SkeletalMesh;
		}
	);

	LineBatcher = MakeUnique<FLineBatcher>();
	LineBatcher->Init(Renderer, GetActiveWorld());

	DetailsPanel->SetWorld(GetActiveWorld());

	SettingsPanel->SetWorld(GetActiveWorld());
	SettingsPanel->SetViewportsPanel(ViewportsPanel);
	ViewportsPanel->SetGizmo(Gizmo.get());

	const ImVec4 White = { 1, 1, 1, 1 };
	const ImVec4 Green = { 0.40f, 0.80f, 0.30f, 1.0f };

	Save = { "Save",   "Save Current Scene",UAssetManager::Get().LoadTexture(EditorResourcePaths::ToolbarSaveIcon),  White, { EKeyCode::S, true } };
	// 저장한 적 없는 씬이면 SaveScene이 파일 대화상자를 띄운다.
	Save.CanExecute = [this]() { return !IsPlaying(); };
	Save.Execute = [this]() { SaveCurrentScene(); };

	Play = { "Play",   "Play World",        UAssetManager::Get().LoadTexture(EditorResourcePaths::ToolbarPlayIcon),  Green, { EKeyCode::P, false, true } };

	Play.CanExecute = [this]()->bool {return !PlayWorld; };
	Play.Execute = [this]() { RequestPlaySession(); };

	Pause = { "Pause",  "Pause",			UAssetManager::Get().LoadTexture(EditorResourcePaths::ToolbarPauseIcon), White, { EKeyCode::Pause } };

	Pause.CanExecute = [this]()->bool {return !bIsPaused && PlayWorld; };
	Pause.Execute = [this]() { bIsPaused = true; };


	Resume = { "Resume", "Resume",          UAssetManager::Get().LoadTexture(EditorResourcePaths::ToolbarPlayIcon),  White, { EKeyCode::Pause } };

	Resume.CanExecute = [this]()->bool {return bIsPaused && PlayWorld; };
	Resume.Execute = [this]() { bIsPaused = false; };

	StepFrame = { "Step", "Step One Frame", UAssetManager::Get().LoadTexture(EditorResourcePaths::ToolbarStepIcon),  White, {} };

	StepFrame.CanExecute = [this]()->bool {return bIsPaused && PlayWorld; };
	StepFrame.Execute = [this]() { bStepRequested= true; };


	Stop = { "Stop",   "Stop",				UAssetManager::Get().LoadTexture(EditorResourcePaths::ToolbarStopIcon),  White, { EKeyCode::Escape } };

	Stop.CanExecute = [this]()->bool {return PlayWorld; };
	Stop.Execute = [this]() { RequestEndPlayMap(); };

	EditorUI->GetToolbar()->SetCommands({ Play, Pause, Resume, StepFrame, Stop });

	// 복사·붙여넣기는 에디터 월드에서만 한다. PIE 월드에 만든 액터는 세션이 끝나면 사라지기 때문이다.
	Copy = { "Copy", "Copy Selected Actors", nullptr, White, { EKeyCode::C, true } };
	Copy.CanExecute = [this]() { return !IsPlaying() && !Selection.IsEmpty(); };
	Copy.Execute = [this]() { CopySelectedActors(); };

	Paste = { "Paste", "Paste Copied Actors", nullptr, White, { EKeyCode::V, true } };
	Paste.CanExecute = [this]() { return !IsPlaying() && !CopiedActorData.IsEmpty(); };
	Paste.Execute = [this]() { PasteCopiedActors(); };

	// 복사 후 바로 붙여넣는다. 복사해 둔 내용도 이번 선택으로 바뀐다.
	Duplicate = { "Duplicate", "Duplicate Selected Actors", nullptr, White, { EKeyCode::D, true } };
	Duplicate.CanExecute = [this]() { return !IsPlaying() && !Selection.IsEmpty(); };
	Duplicate.Execute = [this]() { CopySelectedActors(); PasteCopiedActors(); };

	FocusSelection = { "Focus", "Focus Selected Actors", nullptr, White, { EKeyCode::F } };
	FocusSelection.CanExecute = [this]() { return !Selection.IsEmpty(); };
	FocusSelection.Execute = [this]() { FocusOnSelection(); };

	CycleGizmoMode = { "Gizmo Mode", "Cycle Gizmo Mode", nullptr, White, { EKeyCode::Space } };
	CycleGizmoMode.Execute = [this]()
	{
		const int NextMode = (static_cast<int>(Gizmo->GetMode()) + 1) % 3;
		Gizmo->SetMode(static_cast<EGizmoMode>(NextMode));
	};

	EditorUI->GetToolbar()->SetActorSpawnBinding(
		[this](UClass* Class) { SpawnActorFromToolbar(Class); },
		[this]() { return !IsPlaying(); });
	EditorUI->GetToolbar()->SetPlayModeBinding(
		[this]() { return PlayMode; },
		[this](EPlayModeType Mode) { PlayMode = Mode; },
		[this]() { return !IsPlaying(); });

	return true;
}

// 프레임 시작·View 상태·월드 갱신 후 View별 오프스크린 렌더와 UI 합성을 진행한다.
// 입력·창 메시지는 FEngineLoop가 먼저 처리하고 Present는 호출 직후에 한다.
void UEditorEngine::Tick(const float DeltaTime)
{
	// PIE 창의 닫기 버튼은 앱이 아니라 PIE 세션만 끝낸다.
	if (PIEWindow && PIEWindow->ConsumeCloseRequest())
	{
		RequestEndPlayMap();
	}
	if (bRequestEndPlayMapQueued) 
	{ 
		bRequestEndPlayMapQueued = false; 
		EndPlayMap(); 
	}
	if (bPlaySessionRequested)	
	{
		bPlaySessionRequested = false;
		if (!PlayWorld)
		{
			StartPlayInEditorSession();
		}
	}

	// 유저의 World 전환 요청을 처리한다.
	TickWorldTravel();

	BeginFrame(DeltaTime);
	// 임시: 선택된 Actor를 복제해서 결과를 로그로 확인 (PIE 연결 후 삭제)
	if (!IsPlaying() && !ImGui::GetIO().WantTextInput && FInputSystem::IsKeyPressed(EKeyCode::F9))
	{
		UWorld* PIEWorld = UWorld::CreateWorld(EWorldType::PIE, false);
		FObjectDuplicationParameters Params(EditorWorld, nullptr);
		Params.DuplicationSeed.Add(EditorWorld, PIEWorld);
		Params.DuplicationSeed.Add(EditorWorld->GetPersistentLevel(), PIEWorld->GetPersistentLevel());
		Params.PortFlags = EPropertyPortFlags::PPF_DuplicateForPIE;
		StaticDuplicateObjectEx(Params);

		HTR_LOG(Info, "[DupTest] actors {} -> {}",
			EditorWorld->GetPersistentLevel()->GetActors().Num(),
			PIEWorld->GetPersistentLevel()->GetActors().Num());
		for (AActor* A : PIEWorld->GetPersistentLevel()->GetActors())
			HTR_LOG(Info, "[DupTest]   {} world={} level={} root owner ok={}",
				A->GetName(), A->GetWorld() == PIEWorld, A->GetLevel() == PIEWorld->GetPersistentLevel(),
				A->GetRootComponent() && A->GetRootComponent()->GetOwner() == A);
	}
	UpdateMultipleViewportState(DeltaTime);
	TickWorldAndEditor(DeltaTime);
	RenderViewports();
	EndFrame();
}

// DeltaTime을 패널에 전달하고 에디터 단축키를 처리한다.
void UEditorEngine::BeginFrame(const float DeltaTime)
{
	FStatOverlay::Tick(DeltaTime);

	if (!ImGui::GetIO().WantTextInput && FInputSystem::IsKeyPressed(EKeyCode::Delete))
		DeleteSelectedActors();

	EditorCommandUI::ProcessShortcuts({ &Save, &Copy, &Paste, &Duplicate, &FocusSelection, &CycleGizmoMode });
}

// 패널의 Layout·Preset 요청과 입력을 패널 상태에 반영한다.
void UEditorEngine::UpdateMultipleViewportState(const float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_ViewportUpdate);

	const FVector2 ViewportSize = ViewportsPanel->GetContentSize();
	const FVector2 LocalMousePosition = ViewportsPanel->GetLocalMousePosition();

	ELayoutMode RequestedLayout{};
	int32 RequestedSingleViewIndex = ViewportsPanel->GetSingleViewIndex();
	if (ViewportsPanel->ConsumeLayoutRequest(RequestedLayout, RequestedSingleViewIndex))
	{
		if (RequestedLayout == ELayoutMode::Single)
			ViewportsPanel->SetSingleViewIndex(RequestedSingleViewIndex);
		ViewportsPanel->SetLayoutMode(RequestedLayout);

		FEditorSettings& Settings = SettingsPanel->GetMutableSettings();
		Settings.bMultipleViewportsSingle = RequestedLayout == ELayoutMode::Single;
		Settings.MultipleViewportsSingleViewIndex = RequestedSingleViewIndex;
	}

	int32 PresetViewIndex = InvalidViewIndex;
	EMultipleViewportsCameraPreset RequestedPreset = EMultipleViewportsCameraPreset::Perspective;
	if (ViewportsPanel->ConsumeCameraPresetRequest(PresetViewIndex, RequestedPreset))
		ViewportsPanel->GetEditorClient(PresetViewIndex).ApplyCameraPreset(RequestedPreset, ViewportsPanel->GetViewRect(PresetViewIndex));
	ViewportsPanel->UpdateLayout(ViewportSize, LocalMousePosition);

	const float HorizontalDrag = ViewportsPanel->ConsumeHorizontalDrag();
	const float VerticalDrag = ViewportsPanel->ConsumeVerticalDrag();
	if (HorizontalDrag != 0.0f)
		ViewportsPanel->ApplySplitterDrag(EDragAxis::Horizontal, HorizontalDrag, ViewportSize);
	if (VerticalDrag != 0.0f)
		ViewportsPanel->ApplySplitterDrag(EDragAxis::Vertical, VerticalDrag, ViewportSize);
	if (HorizontalDrag != 0.0f || VerticalDrag != 0.0f)
	{
		ViewportsPanel->UpdateLayout(ViewportSize, LocalMousePosition);
		const FSplitRatio Ratio = ViewportsPanel->GetSplitRatio();
		SettingsPanel->GetMutableSettings().MultipleViewportsHorizontal = Ratio.Horizontal;
		SettingsPanel->GetMutableSettings().MultipleViewportsVertical = Ratio.Vertical;
	}

	// 우클릭 + 휠로 바뀐 이동 속도는 Settings에 바로 저장되고, 바뀐 View에 잠시 표시한다.
	float& CameraSpeed = SettingsPanel->GetMutableSettings().CameraSpeed;
	const float PreviousCameraSpeed = CameraSpeed;
	ViewportsPanel->UpdateInput(
		DeltaTime,
		LocalMousePosition,
		CameraSpeed,
		SettingsPanel->GetSettings().MouseSensitivity);
	if (CameraSpeed != PreviousCameraSpeed)
		ViewportsPanel->ShowCameraSpeed(ViewportsPanel->GetActiveViewIndex(), CameraSpeed);
	// 겹친 창은 Hover 선택에서 제외하고 우클릭 Capture를 우선한다.
	if (ViewportsPanel->IsHovered() || ViewportsPanel->GetCapturedViewIndex() != InvalidViewIndex)
		ViewportsPanel->SetEditorViewIndex(ViewportsPanel->GetActiveViewIndex());

	// 에셋 탭 뷰포트도 레벨과 같은 카메라 속도·감도를 쓴다. 받을지는 각 탭의 패널이 Hover·Capture로 거른다.
	// 휠은 레벨 View가 가져가지 않았을 때만 남아 있다.
	FViewportInput TabInput;
	TabInput.DeltaTime = DeltaTime;
	TabInput.MoveSpeed = CameraSpeed;
	TabInput.MouseSensitivity = SettingsPanel->GetSettings().MouseSensitivity;
	TabInput.bEnabled = ::GetForegroundWindow() == MainWindow->GetHandle() && !ImGui::GetIO().WantTextInput;
	for (const TUniquePtr<FEditorTab>& Tab : EditorUI->GetTabs())
		Tab->RouteInput(TabInput);
}

// 월드를 한 번 Tick·Capture한 뒤 에디터와 피킹을 갱신한다.
void UEditorEngine::TickWorldAndEditor(const float DeltaTime)
{
	// 월드 상태는 프레임마다 정확히 한 번 갱신하고 캡처한다.

	{
		SCOPE_CYCLE_COUNTER(STAT_WorldTick);
		if (bStepRequested && bIsPaused)
		{
			GetActiveWorld()->Tick(1.0f / 60.0f);
			FLuaScriptSystem::Get().Update(1.0f / 60.0f);
			bStepRequested = false;
		}
		else if (!bIsPaused)
		{
			UWorld* ActiveWorld = GetActiveWorld();
			const float GameDeltaTime = DeltaTime * ActiveWorld->GetGlobalTimeDilation();
			GetActiveWorld()->Tick(GameDeltaTime);
			FLuaScriptSystem::Get().Update(GameDeltaTime);
		}

	}
	{
		SCOPE_CYCLE_COUNTER(STAT_EditorTick);
		UpdateEditorTabs();
		EditorUI->Tick(DeltaTime);
	}
	// 라이트 프록시가 아래 UpdateAllTransforms에서 바뀐 작업 흐름으로 다시 계산되도록 먼저 반영한다.
	ColorWorkflow::bLinear = SettingsPanel->GetSettings().bLinearWorkflow;
	// 월드 캡처는 View마다 그릴 월드가 다르므로 RenderViewports에서 한다.
	// PIE 중에도 PIE가 아닌 View에서는 EditorWorld 편집을 계속한다.
	UpdateGizmoAndPicking();
	//SCOPE_CYCLE_COUNTER(STAT_UpdateAllTransforms);
	EditorWorld->GetScene().UpdateAllTransforms();
	if (PlayWorld && bIsPaused)
		PlayWorld->GetScene().UpdateAllTransforms();
}

// 레벨 탭의 뷰포트 슬롯과 에셋 탭 뷰를 같은 목록으로 모아 같은 조건으로 그린다. 별도 PIE 창은 따로 그린다.
void UEditorEngine::RenderViewports()
{
	// PIE 창 크기를 외부 View에 먼저 알려야 카메라 Override의 직교 폭 보정이 맞는다.
	bPIEWindowRendered = false;
	SceneRenderer.BeginFrame();
	const bool bRenderPIEWindow = IsPIEInWindow() && UpdatePIEWindowTargets();
	const FSceneRenderSettings SceneRenderSettings = MakeSceneRenderSettings();

	// 월드 Tick으로 움직인 게임 카메라를 같은 프레임에 반영한다.
	UpdatePIEViewCamera();

	TArray<FEditorTabView> TabViews;
	for (const TUniquePtr<FEditorTab>& Tab : EditorUI->GetTabs())
		Tab->CollectViews(TabViews);

	// 같은 월드가 이어지면 캡처를 다시 하지 않는다.
	UWorld* CapturedWorld = nullptr;
	const auto PrepareWorld = [&](UWorld& ViewWorld)
	{
		if (&ViewWorld == CapturedWorld)
			return;
		SCOPE_CYCLE_COUNTER(STAT_CaptureWorld);
		RenderCollector.CaptureWorld(ViewWorld, SceneRenderSettings.bDrawSkeletalMeshes);
		CapturedWorld = &ViewWorld;
	};

	TArray<UWorld*> RenderedWorlds;
	const auto AddRenderedWorld = [&RenderedWorlds](UWorld* World)
	{
		if (World && RenderedWorlds.Find(World) == -1)
			RenderedWorlds.Add(World);
	};
	for (const FEditorTabView& TabView : TabViews)
	{
		FViewport& Viewport = *TabView.Viewport;
		FViewportClient* Client = Viewport.GetActiveClient();
		if (!Viewport.IsVisible() || !Client || !Client->GetWorld())
			continue;
		Viewport.EnsureRenderTargets();
		UWorld& ViewWorld = *Client->GetWorld();
		PrepareWorld(ViewWorld);
		RenderFrame(Viewport, ViewWorld, MakeSceneView(Viewport), SceneRenderSettings, TabView.OverlayClient, TabView.Gizmo);
		AddRenderedWorld(&ViewWorld);
	}

	// 별도 PIE 창은 패널 View 뒤에 PlayWorld를 한 번 더 캡처해 그린다.
	if (bRenderPIEWindow && IsViewActive(PIEViewIndex))
	{
		UWorld& ViewWorld = *PIEWindowViewport.GetActiveClient()->GetWorld();
		PrepareWorld(ViewWorld);
		RenderFrame(PIEWindowViewport, ViewWorld, MakeSceneView(PIEWindowViewport), SceneRenderSettings, nullptr);
		CopyPIEWindowToBackbuffer();
		AddRenderedWorld(&ViewWorld);
	}

	// 그림자 맵은 월드마다 이번 프레임에 한 번 갱신했으니 다음 프레임까지 다시 그리지 않는다.
	AddRenderedWorld(EditorWorld);
	AddRenderedWorld(PlayWorld);
	for (UWorld* World : RenderedWorlds)
		World->GetScene().ClearShadowDirty();
}

void UEditorEngine::RenderFrame(const FViewport& Viewport, UWorld& ViewWorld, const FSceneView& View, const FSceneRenderSettings& Settings,
	FEditorViewportClient* OverlayClient, const FGizmo* ViewGizmo)
{
	{
		SCOPE_CYCLE_COUNTER(STAT_BuildRenderQueue);
		RenderCollector.BuildRenderQueue(Viewport, RenderQueue);
	}

	// 1. 씬과 후처리 (Linear 씬 컬러 → 표시용 sRGB)
	RenderFrame(View, Settings, ViewWorld, Viewport.GetRenderTargets(), RenderQueue);

	// 2. 오버레이 Pass: Grid·Gizmo·UUID는 sRGB 색으로 지정되므로 인코딩 뒤 표시용 타깃에 그린다. PIE UI도 여기서 얹는다
	RenderOverlayPass(Viewport, OverlayClient, ViewWorld, View, RenderQueue, ViewGizmo);
}

void UEditorEngine::UpdatePIEViewCamera()
{
	if (!PlayWorld || PIEViewIndex == InvalidViewIndex)
		return;

	APlayerController* PlayerController = PlayWorld->GetPlayerController();
	if (!PlayerController)
	{
		GetViewportForView(PIEViewIndex).EndPlay();
		return;
	}

	// 게임 카메라 자체 입력(WASD·우클릭 회전·휠)은 PIE View를 선택했을 때만 받는다.
	// 별도 창이면 그 창이 OS 포커스를 가졌을 때다.
	const bool bPIEViewFocused = IsPIEInWindow()
		? ::GetForegroundWindow() == PIEWindow->GetHandle()
		: ViewportsPanel->GetActiveViewIndex() == PIEViewIndex && !ImGui::GetIO().WantTextInput;
	PlayerController->SetInputEnabled(bPIEViewFocused);

	const FMinimalViewInfo& PlayerView = PlayerController->GetPlayerView();
	const FVector Forward = PlayerView.Rotation.Quaternion().RotateVector(FVector{ 1.0f, 0.0f, 0.0f }).Normalized();

	const FCameraProjection Projection{
		PlayerView.bOrthographic ? EProjectionMode::Orthographic : EProjectionMode::Perspective,
		PlayerView.FOV,
		PlayerView.OrthoWidth,
		PlayerView.NearClip,
		PlayerView.FarClip };
	// 월드가 바뀌면 HandleOnPIEWorldChanged가 Game Client를 떼므로 여기서 새 PlayWorld로 다시 붙는다.
	FViewport& PIEViewport = GetViewportForView(PIEViewIndex);
	if (!PIEViewport.HasGameClient())
		PIEViewport.BeginPlay(MakeUnique<FGameViewportClient>(*PlayWorld));
	PIEViewport.GetGameClient()->SetCamera(PlayerView.Location, Forward, Projection, PIEViewport.GetOutputRect());
}

// 화면을 표시하고 UI 변경 후 View 설정을 보관한다.
void UEditorEngine::EndFrame()
{
	PresentFrame();
	// 메인 Swapchain은 FEngineLoop가 VSync 설정대로 Present하므로 PIE 창은 기다리지 않고 올린다.
	if (bPIEWindowRendered && PIESwapchain)
		PIESwapchain->SwapBuffers(0);
	// UI 변경 후 설정을 복사해 종료 시 카메라 수명에 의존하지 않는다.
	SettingsPanel->CaptureViewportSettings();
	// 프로파일러 반영
	FStatRegistry::EndFrame();
}

void UEditorEngine::UpdateEditorTabs()
{
	const TArray<TUniquePtr<FEditorTab>>& Tabs = EditorUI->GetTabs();
	for (int32 Index = Tabs.Num() - 1; Index > 0; --Index)
	{
		if (Tabs[Index]->IsOpen())
		{
			continue;
		}
		EditorUI->RemoveTab(Index);
	}

	if (PendingPreviewMesh)
	{
		OpenSkeletalMeshViewer(PendingPreviewMesh);
		PendingPreviewMesh = nullptr;
	}

}

FSceneView UEditorEngine::MakeSceneView(const FViewport& Viewport) const
{
	const FVector2 OutputSize = Viewport.GetOutputSize();
	const FCameraProjection& Projection = Viewport.GetProjection();

	return {
		.ViewProjection = Viewport.PrepareView().ViewProjection,
		.Location = Viewport.GetCameraLocation(),
		.Forward = Viewport.GetCameraForward(),
		.FovDegrees = Projection.FovDegrees > 0.0f ? Projection.FovDegrees : 60.0f,
		.AspectRatio = Viewport.IsVisible() ? OutputSize.X / OutputSize.Y : 16.0f / 9.0f,
		.bOrthographic = Projection.Mode == EProjectionMode::Orthographic,
		.RenderMode = Viewport.GetViewRenderMode()
	};
}

FViewport& UEditorEngine::GetViewportForView(const int32 ViewIndex)
{
	if (ViewIndex == PIEWindowViewIndex)
		return PIEWindowViewport;
	return ViewportsPanel->GetViewport(ViewIndex);
}

const FViewport& UEditorEngine::GetViewportForView(const int32 ViewIndex) const
{
	if (ViewIndex == PIEWindowViewIndex)
		return PIEWindowViewport;
	return ViewportsPanel->GetViewport(ViewIndex);
}

// PIE 창 View는 레이아웃과 무관하게 크기와 카메라가 주어졌을 때만 활성이다.
bool UEditorEngine::IsViewActive(const int32 ViewIndex) const
{
	if (ViewIndex == PIEWindowViewIndex)
		return PIEWindowViewport.HasGameClient() && PIEWindowViewport.IsVisible();
	return ViewportsPanel->IsViewActive(ViewIndex);
}

FSceneRenderSettings UEditorEngine::MakeSceneRenderSettings() const
{
	const FEditorSettings& Settings = SettingsPanel->GetSettings();

	FShadowFilterSettings Filter;
	Filter.Mode = static_cast<EShadowFilterMode>(Settings.ShadowFilterMode);
	Filter.bPoisson = Settings.bShadowPoisson;
	Filter.bGaussian = Settings.bShadowGaussian;
	Filter.bRotate = Settings.bShadowRotate;
	Filter.PCFRadiusTexels = Settings.ShadowPCFRadiusTexels;
	Filter.MaxFilterTexels = Settings.ShadowMaxFilterTexels;

	return {
		.bDrawPrimitives = Settings.bDrawPrimitives,
		.bDrawSkeletalMeshes = Settings.bDrawSkeletalMeshes,
		.bDrawDecals = Settings.bDrawDecals,
		.bDrawShadows = Settings.bDrawShadows,
		.ShadowFilter = Filter,
		.bExponentialHeightFog = Settings.bExponentialHeightFog,
		.bToneMapping = Settings.bToneMapping,
		.ExposureEV = Settings.ExposureEV,
		.bEnableFXAA = Settings.bEnableFXAA,
		.bDepthView = Settings.bDepthView,
		.bVisualizeCascades = Settings.bVisualizeCascades,
		.bVisualizeTileLightCount = Settings.bVisualizeTileLightCount,
		// 에디터 View의 비네트. PIE View는 RenderFrame에서 PCM 값으로 바꾼다
		.ScreenEffects = {
			.Vignette = {
				.bEnabled = Settings.bVignetting,
				.Intensity = Settings.VignetteIntensity,
				.Radius = Settings.VignetteRadius,
				.Softness = Settings.VignetteSoftness
			}
		}
	};
}

void UEditorEngine::OpenSkeletalMeshViewer(USkeletalMesh* SkeletalMesh)
{
	if(!SkeletalMesh)
		return;

	//같은 메시의 미리보기 탭이 존재한다면 해당 탭을 포커스한다.
	for (const TUniquePtr<FEditorTab>& Tab : EditorUI->GetTabs())
	{
		if (Tab->GetEditingAsset() == SkeletalMesh)
		{
			Tab->RequestFocus();
			return;
		}
	}


	EditorUI->AddTab<FSkeletalMeshPreviewTab>(SkeletalMesh)->RequestFocus();
}

FWorldContext& UEditorEngine::GetEditorWorldContext()
{
	for (int32 i = 0; i < WorldList.Num(); ++i)
	{
		if (WorldList[i].WorldType == EWorldType::Editor)
		{
			return WorldList[i];
		}
	}

	return CreateNewWorldContext(EWorldType::Editor);
}

FWorldContext* UEditorEngine::GetPIEWorldContext(int32 WorldPIEInstance)
{
	for (FWorldContext& WorldContext : WorldList)
	{
		if (WorldContext.WorldType == EWorldType::PIE)
		{
			return &WorldContext;
		}
	}

	return nullptr;
}

void UEditorEngine::HandleOnPIEWorldChanged(UWorld* OldWorld, UWorld* NewWorld)
{
	if (PlayWorld != OldWorld)
	{
		// 현재 등록되어 있는 월드랑 다르면 이상함.
		HTR_LOG(Warning, "Undefined Behavior!");
		return;
	}

	ResetSceneSelection();

	// 이전 PIE 월드의 액터 파괴 구독 해제
	OldWorld->OnActorDestroyed.RemoveAll(this);

	if (PIEViewIndex != InvalidViewIndex)
	{
		GetViewportForView(PIEViewIndex).EndPlay();
	}

	PlayWorld->EndPlay();
	// 이전 월드 액터(owner)를 붙잡은 코루틴이 삭제 후에 재개되면 해제된 메모리를 읽는다.
	FLuaScriptSystem::Get().StopAllCoroutines();

	PlayWorld = NewWorld;
	if (PlayWorld)
	{
		// 새 PIE 월드의 액터 파괴와 에디터 참조 정리 연결
		PlayWorld->OnActorDestroyed.AddRaw(this, &UEditorEngine::HandleActorDestroyed);
	}
	OutlinerPanel->SetWorld(NewWorld);
}

// 파괴 예정 액터의 선택·기즈모 드래그·아웃라이너 참조 정리
void UEditorEngine::HandleActorDestroyed(AActor* Actor)
{
	Selection.Remove(Actor);
	GizmoDragStarts.Reset();
	OutlinerPanel->NotifyActorDestroyed(Actor);
}

void UEditorEngine::RenderOverlayPass(const FViewport& Viewport, FEditorViewportClient* OverlayClient, UWorld& ViewWorld, const FSceneView& View, FRenderQueue& RenderQueue, const FGizmo* ViewGizmo)
{
	// 실행 중인 PIE View와 별도 PIE 창은 OverlayClient가 없어 Grid·Gizmo·Outline 같은 에디터 표시를 그리지 않는다.
	const bool bShowEditorOverlay = OverlayClient != nullptr;
	const FSceneRenderTargets& Targets = Viewport.GetRenderTargets();
	const FMatrix& ViewProjection = View.ViewProjection;
	const FVector& ViewCameraLocation = View.Location;
	const FVector& ViewCameraForward = View.Forward;

	// 현재 Viewport의 최종 출력과 깊이 타깃 유지
	FRenderingInfo PassInfo;
	PassInfo.ViewportSetting.Width = Targets.Width;
	PassInfo.ViewportSetting.Height = Targets.Height;
	FRenderingDesc ColorDesc;
	ColorDesc.Texture = Targets.ViewColor.get();
	ColorDesc.LoadOp = ERenderTargetLoadOp::Load;
	PassInfo.ColorRenderTargets.Add(ColorDesc);
	PassInfo.DepthStencil.Texture = Targets.Depth.get();

	PassInfo.DepthStencil.LoadOp = ERenderTargetLoadOp::Load;

	RenderCommand::BeginRenderPass(PassInfo);

	const FEditorSettings& Settings = SettingsPanel->GetSettings();

	LineBatcher->BeginFrame();

	// 라인 배처는 매 프레임 한 번만 비우고 한 번만 그린다.
	// 라이트 디버그 라인은 바운딩 박스 표시 여부와 무관하게 항상 그린다.
	if (bShowEditorOverlay && !Settings.bDepthView)
	{
		if (Settings.bDrawBoundingBox)
		{
			LineBatcher->BuildVertexBuffer(ViewWorld);
		}
		// 미리보기 월드의 라이트는 편집 대상이 아니라 조명용이라 방향 표시 선을 그리지 않는다
		const bool bPreviewWorld = ViewWorld.GetWorldType() == EWorldType::EditorPreview;
		for (TObjectIterator<UDirectionalLightComponent> DirLight; DirLight && !bPreviewWorld; ++DirLight)
		{
			if (DirLight && DirLight->GetOwner() && DirLight->GetOwner()->GetWorld() == &ViewWorld)
			{
				DirLight->DrawDebug(LineBatcher.get());
			}
		}
		// 선택된 액터의 View별 데칼 볼륨 표시
		for (TObjectIterator<UDecalComponent> It; It; ++It)
		{
			UDecalComponent* Decal = *It;
			AActor* Owner = Decal->GetOwner();
			if (!Owner || Owner->GetWorld() != &ViewWorld)
			{
				continue;
			}
			if (Selection.IsSelected(Owner))
			{
				Decal->DrawDebug(LineBatcher.get());
			}
		}
		// Point·Spot 라이트의 범위는 선택된 액터의 라이트만 그린다 (Spot의 IsDebugLine이 켜져 있으면 항상).
		// 반복자는 파생 클래스도 돌기 때문에 Spot도 여기서 함께 그린다.
		for (TObjectIterator<UPointLightComponent> LocalLight; LocalLight; ++LocalLight)
		{
			AActor* Owner = LocalLight->GetOwner();
			if (!Owner || Owner->GetWorld() != &ViewWorld)
				continue;
			if (Selection.IsSelected(Owner) || LocalLight->ShouldAlwaysDrawDebug())
				LocalLight->DrawDebug(LineBatcher.get());
		}
	}

	for (TObjectIterator<UShapeComponent> CollisionComponent; CollisionComponent; ++CollisionComponent)
	{
		AActor* Owner = CollisionComponent->GetOwner();
		if (!Owner || Owner->GetWorld() != &ViewWorld) continue;
		if (ViewWorld.GetWorldType() == EWorldType::PIE && CollisionComponent->GetIsHiddenLine()) continue;

		if (CollisionComponent->ShouldAlwaysDrawDebug())
			CollisionComponent->DrawDebug(LineBatcher.get());
	}

	LineBatcher->OnRender(ViewProjection);

	// 미리보기 월드는 본을 보는 곳이라 스켈레톤을 메시 위에 겹쳐 그린다. 메시 안쪽에 있으므로 깊이 검사를 끈다
	if (bShowEditorOverlay && ViewWorld.GetWorldType() == EWorldType::EditorPreview)
	{
		LineBatcher->BeginFrame();
		LineBatcher->SetDepthTest(false);
		for (TObjectIterator<USkinnedMeshComponent> SkinnedMesh; SkinnedMesh; ++SkinnedMesh)
		{
			AActor* Owner = SkinnedMesh->GetOwner();
			if (Owner && Owner->GetWorld() == &ViewWorld)
				SkinnedMesh->DrawDebugSkeleton(LineBatcher.get());
		}
		LineBatcher->OnRender(ViewProjection);
		LineBatcher->SetDepthTest(true);
	}

	// Grid는 후처리(안개·FXAA)가 끝난 뒤 합성하는 에디터 표시라 안개를 적용하지 않는다.
	// 원근은 픽셀 셰이더 Grid, 직교는 선 Grid를 쓴다 (언리얼 에디터와 같은 구분).
	if (bShowEditorOverlay && Settings.bDrawGrid)
	{
		const EGridPlane GridPlane = OverlayClient->GetGridPlane(Viewport.GetCamera().Transform.Rotation);

		if (!Viewport.IsOrthographic())
		{
			GridRenderer->OnRenderPSGrid(
				ViewProjection, ViewCameraLocation, Settings, PassInfo.ViewportSetting
			);
		}
		else
		{
			GridRenderer->OnRenderBatchGrid(
				ViewProjection,
				ViewCameraLocation,
				ViewCameraForward,
				GridPlane,
				static_cast<float>(Settings.GridSpacing),
				!Viewport.IsOrthographic() ||
				OverlayClient->GetCameraPreset() == EMultipleViewportsCameraPreset::OrthographicView,
				PassInfo.ViewportSetting
			);
		}
	}

	// 선택 대상은 한 월드에만 속하므로 그 월드를 그리는 View에만 표시한다.
	const auto IsInViewWorld = [&ViewWorld](const UActorComponent* Component)
	{
		return Component && Component->GetOwner() && Component->GetOwner()->GetWorld() == &ViewWorld;
	};

	// 스텐실 기반이라 선택 대상의 가시성이 꺼져 있어도 외곽선만 그린다.
	if (bShowEditorOverlay)
	{
		OutlineRenderer->OnRender(*Outline, ViewProjection, PassInfo.ViewportSetting, &ViewWorld);
	}

	// 기즈모는 탭마다 따로다. 레벨 View는 엔진 Gizmo, 에셋 탭은 자기 Gizmo를 넘긴다
	if (bShowEditorOverlay && ViewGizmo && IsInViewWorld(ViewGizmo->GetTarget()))
	{
		RenderCommand::ClearDepthStencil(Targets.Depth.get());

		GizmoRenderer->OnRender(
			*ViewGizmo,
			ViewProjection,
			ViewCameraLocation,
			Viewport.IsOrthographic());
	}

	RenderCommand::ClearDepthStencil(PassInfo.DepthStencil.Texture);

	if (Settings.bShowUUID)
	{
		// 월드 +Z가 아니라 화면 위쪽으로 띄워야 Top·Bottom처럼 Z축을 따라 보는 View에서도 물체를 가리지 않는다.
		const FVector ScreenUp = Viewport.GetCameraUp();

		for (AActor* Actor : ViewWorld.GetPersistentLevel()->GetActors())
		{
			if (!Actor)
				continue;

			FBox ActorBounds;
			if (!Actor->TryGetActorBounds(ActorBounds))
			{
				const FVector ActorLocation = Actor->GetActorLocation();
				ActorBounds.Min = ActorLocation;
				ActorBounds.Max = ActorLocation;
			}

			const FVector Center = (ActorBounds.Min + ActorBounds.Max) * 0.5f;
			const FVector HalfExtent = (ActorBounds.Max - ActorBounds.Min) * 0.5f;
			// 바운드 박스가 화면 위쪽으로 뻗은 길이만큼 올린 뒤 여백을 더한다.
			const float ExtentAlongUp = std::fabs(ScreenUp.X) * HalfExtent.X
				+ std::fabs(ScreenUp.Y) * HalfExtent.Y
				+ std::fabs(ScreenUp.Z) * HalfExtent.Z;
			UUIDLocation = Center + ScreenUp * (ExtentAlongUp + 0.5f);

			FString Text =
				"UUID : " + std::to_string(Actor->GetUUID());

			TextRenderer->BuildTextMesh(
				Text,
				0.5f,
				*SystemFont
			);

			const FMatrix BillboardWorld = Viewport.BuildBillboardMatrix(UUIDLocation, 1.0f, 1.0f);
			TextRenderer->OnRender(Text, BillboardWorld, 0.5f, *SystemFont, ViewProjection);
		}
	}

	// 게임 UI는 Game Client가 그리는 View가 실행 중일 때만 얹는다
	if (GameViewport && Viewport.HasGameClient() && !bIsPaused)
	{
		GameViewport->CollectDrawList(PassInfo.ViewportSetting.Width, PassInfo.ViewportSetting.Height);
		GameViewport->Draw(PassInfo.ViewportSetting.Width, PassInfo.ViewportSetting.Height, *TextRenderer);
	}

	RenderCommand::EndRenderPass();
}

// 입력 View의 Ray와 피킹으로 Gizmo·공유 선택을 갱신한다.
void UEditorEngine::UpdateGizmoAndPicking()
{
	// Delete는 BeginFrame에서 한 번만 처리하고 여기서는 View 입력만 다룬다.
	const int32 ViewIndex = ViewportsPanel->GetActiveViewIndex();
	if (ViewIndex == InvalidViewIndex || !ViewportsPanel->IsHovered())
		return;
	// 실행 중인 PIE View는 편집하지 않는다. 일시정지 중이면 PlayWorld를 피킹한다.
	if (IsPlayingView(ViewIndex))
		return;

	const FVector2 LocalMousePosition = ViewportsPanel->GetLocalMousePosition();
	FRay Ray{};
	if (!ViewportsPanel->TryGetActiveViewRay(LocalMousePosition, Ray))
		return;

	const FViewport& Viewport = ViewportsPanel->GetViewport(ViewIndex);
	// 마우스를 View 기준으로 바꾸는 오프셋은 배치를 아는 패널 Rect에서 얻는다.
	const FRect& Rect = ViewportsPanel->GetViewRect(ViewIndex);
	const FVector2 ViewLocalMouse(
		LocalMousePosition.X - Rect.X,
		LocalMousePosition.Y - Rect.Y);
	const FMatrix ViewProjection = Viewport.PrepareView().ViewProjection;
	bool bMouseDown = FInputSystem::IsMouseDown(EMouseButton::Left);

	// 다른 월드의 선택 대상은 이 View에 Gizmo가 보이지 않으므로 조작하지 않는다.
	UWorld* ViewWorld = GetWorldForView(ViewIndex);
	const USceneComponent* GizmoTarget = Gizmo->GetTarget();
	const bool bGizmoInView = !GizmoTarget ||
		(GizmoTarget->GetOwner() && GizmoTarget->GetOwner()->GetWorld() == ViewWorld);

	if (bGizmoInView)
	{
		Gizmo->Update(
			Ray,
			ViewLocalMouse,
			ViewProjection,
			static_cast<int>(Rect.Width),
			static_cast<int>(Rect.Height),
			bMouseDown,
			Viewport.GetCameraLocation(),
			Viewport.IsOrthographic());
		UpdateSelectionGizmoDrag();
	}

	const bool bGizmoBlocksPick = bGizmoInView && (Gizmo->IsUsing() || Gizmo->GetHoveredAxis() >= 0);
	if (FInputSystem::IsMousePressed(EMouseButton::Left) && !bGizmoBlocksPick)
	{
		AActor* PickedActor = nullptr;
		{
			SCOPE_CYCLE_COUNTER_ALWAYS(EditorStats::STAT_PickingTime);
			// 렌더와 같은 함수로 각 Billboard의 위치·크기에 맞는 View 행렬을 만든다.
			const auto ResolveBillboardTransform = [](const UBillboardComponent& Billboard, const void* Context) -> FMatrix
			{
				const auto& PickViewport = *static_cast<const FViewport*>(Context);
				const FVector Scale = Billboard.GetWorldScale3D();
				return PickViewport.BuildBillboardMatrix(Billboard.GetWorldLocation(), Scale.Y, Scale.Z);
			};
			FHitResult Hit;
			if (ViewWorld->LineTraceSingle(Ray, Hit, ResolveBillboardTransform, &Viewport))
				PickedActor = FindActorByPrimitiveId(Hit.HitComponent->GetUUID());
		}

		// Shift·Ctrl 클릭은 기존 선택에 더하거나 빼고, 빈 곳을 눌러도 선택을 유지한다.
		// 다른 월드(PIE 일시정지 View 등)의 액터와는 섞지 않는다.
		const bool bAdditive = FInputSystem::IsKeyDown(EKeyCode::Shift) || FInputSystem::IsKeyDown(EKeyCode::Control);
		const AActor* Primary = Selection.GetPrimary();
		const bool bSameWorld = !Primary || (PickedActor && PickedActor->GetWorld() == Primary->GetWorld());
		if (bAdditive && bSameWorld)
		{
			Selection.Toggle(PickedActor);
		}
		else if (!bAdditive)
		{
			OutlinerPanel->SelectActor(PickedActor);
		}
	}

}

// 선택 전체에 외곽선을, Primary 액터에 기즈모와 디테일을 맞춘다.
void UEditorEngine::SyncSelectionToEditors()
{
	AActor* Primary = Selection.GetPrimary();
	Gizmo->SetTarget(Primary ? Primary->GetRootComponent() : nullptr, true);
	DetailsPanel->SetTarget(Primary);

	// 액터마다 루트가 프리미티브면 루트를, 아니면 첫 프리미티브를 외곽선 대상으로 쓴다.
	TArray<UPrimitiveComponent*> OutlineTargets;
	for (AActor* Actor : Selection.GetActors())
	{
		UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (Primitive)
				break;
			Primitive = Cast<UPrimitiveComponent>(Component);
		}
		if (Primitive)
			OutlineTargets.Add(Primitive);
	}
	Outline->SetTargets(OutlineTargets);
}

void UEditorEngine::UpdateSelectionGizmoDrag()
{
	AActor* Primary = Selection.GetPrimary();
	USceneComponent* PrimaryRoot = Primary ? Primary->GetRootComponent() : nullptr;

	// Details에서 하위 컴포넌트를 골라 기즈모로 움직일 때는 그 컴포넌트만 움직인다.
	const bool bGroupDrag = Gizmo->IsUsing() && PrimaryRoot && Gizmo->GetTarget() == PrimaryRoot
		&& Selection.GetActors().Num() > 1;
	if (!bGroupDrag)
	{
		GizmoDragStarts.Reset();
		return;
	}

	// 드래그를 시작한 프레임에 Primary와 나머지 액터의 시작 Transform을 기록한다.
	if (GizmoDragStarts.IsEmpty())
	{
		GizmoDragStarts.Add({ PrimaryRoot, PrimaryRoot->GetWorldLocation(),
			PrimaryRoot->GetWorldRotation().Quaternion(), PrimaryRoot->GetWorldScale3D() });
		for (AActor* Actor : Selection.GetActors())
		{
			USceneComponent* Root = Actor->GetRootComponent();
			if (Root && Root != PrimaryRoot)
				GizmoDragStarts.Add({ Root, Root->GetWorldLocation(), Root->GetWorldRotation().Quaternion(), Root->GetWorldScale3D() });
		}
	}

	const FGizmoDragStart& Pivot = GizmoDragStarts[0];
	const EGizmoMode Mode = Gizmo->GetMode();
	if (Mode == EGizmoMode::Location)
	{
		const FVector Delta = PrimaryRoot->GetWorldLocation() - Pivot.Location;
		for (int32 Index = 1; Index < GizmoDragStarts.Num(); ++Index)
			GizmoDragStarts[Index].Component->SetWorldLocation(GizmoDragStarts[Index].Location + Delta, false);
	}
	else if (Mode == EGizmoMode::Rotation)
	{
		// Primary를 중심으로 같은 각도만큼 공전·자전한다.
		const FQuat DeltaRotation = PrimaryRoot->GetWorldRotation().Quaternion() * Pivot.Rotation.Inverse();
		for (int32 Index = 1; Index < GizmoDragStarts.Num(); ++Index)
		{
			const FGizmoDragStart& Start = GizmoDragStarts[Index];
			Start.Component->SetWorldLocation(Pivot.Location + DeltaRotation.RotateVector(Start.Location - Pivot.Location), false);
			Start.Component->SetWorldRotation((DeltaRotation * Start.Rotation).ToFRotator(), false);
		}
	}
	else
	{
		// 크기는 Primary가 바뀐 배율을 축마다 각 액터에 곱한다. 0으로 나누지 않도록 시작 값이 0인 축은 그대로 둔다.
		const FVector Current = PrimaryRoot->GetWorldScale3D();
		const auto Ratio = [](const float Now, const float Start) { return std::fabs(Start) > 1.0e-6f ? Now / Start : 1.0f; };
		const FVector ScaleRatio(Ratio(Current.X, Pivot.Scale.X), Ratio(Current.Y, Pivot.Scale.Y), Ratio(Current.Z, Pivot.Scale.Z));
		for (int32 Index = 1; Index < GizmoDragStarts.Num(); ++Index)
		{
			const FVector& StartScale = GizmoDragStarts[Index].Scale;
			GizmoDragStarts[Index].Component->SetWorldScale3D(
				FVector(StartScale.X * ScaleRatio.X, StartScale.Y * ScaleRatio.Y, StartScale.Z * ScaleRatio.Z));
		}
	}
}

// 선택된 액터들의 경계를 모두 감싸는 구를 마지막으로 편집한 View에 맞춘다.
void UEditorEngine::FocusOnSelection()
{
	FBox Bounds;
	bool bHasBounds = false;
	for (AActor* Actor : Selection.GetActors())
	{
		// 메시가 없는 액터(라이트 등)는 위치만 작은 상자로 넣는다.
		FBox ActorBounds;
		if (!Actor->TryGetActorBounds(ActorBounds))
		{
			const FVector Location = Actor->GetActorLocation();
			ActorBounds.Min = Location;
			ActorBounds.Max = Location;
		}

		if (!bHasBounds)
		{
			Bounds = ActorBounds;
			bHasBounds = true;
			continue;
		}
		Bounds.Min = FVector(std::min(Bounds.Min.X, ActorBounds.Min.X), std::min(Bounds.Min.Y, ActorBounds.Min.Y), std::min(Bounds.Min.Z, ActorBounds.Min.Z));
		Bounds.Max = FVector(std::max(Bounds.Max.X, ActorBounds.Max.X), std::max(Bounds.Max.Y, ActorBounds.Max.Y), std::max(Bounds.Max.Z, ActorBounds.Max.Z));
	}
	if (!bHasBounds)
		return;

	const FVector Center = (Bounds.Min + Bounds.Max) * 0.5f;
	const FVector HalfExtent = (Bounds.Max - Bounds.Min) * 0.5f;
	const float Radius = std::sqrt(HalfExtent.X * HalfExtent.X + HalfExtent.Y * HalfExtent.Y + HalfExtent.Z * HalfExtent.Z);
	// 외부 카메라가 그리는 View(PIE 등)는 옮기지 않는다.
	const int32 ViewIndex = ViewportsPanel->GetEditorViewIndex();
	if (ViewIndex >= 0 && ViewIndex < FViewportsPanel::ViewCount && !ViewportsPanel->GetViewport(ViewIndex).HasGameClient())
		ViewportsPanel->GetEditorClient(ViewIndex).FocusOn(Center, Radius, ViewportsPanel->GetViewRect(ViewIndex));
}

void UEditorEngine::SpawnActorFromToolbar(UClass* Class)
{
	// PIE 중에는 보이지 않는 에디터 월드에 스폰되므로 막는다.
	if (!Class || IsPlaying())
		return;

	// 편집 중인 View 카메라 앞에 놓아 바로 보이게 한다.
	constexpr float SpawnDistance = 10.0f;
	const FViewport& Viewport = ViewportsPanel->GetViewport(ViewportsPanel->GetEditorViewIndex());
	FTransform SpawnTransform = FTransform::Identity;
	SpawnTransform.Location = Viewport.GetCameraLocation()
		+ Viewport.GetCameraForward() * SpawnDistance;

	if (AActor* NewActor = EditorWorld->SpawnActor(Class, NAME_None, &SpawnTransform))
		OutlinerPanel->SelectActor(NewActor);
}

void UEditorEngine::CopySelectedActors()
{
	if (!FJsonArchive::SaveActorsToBytes(Selection.GetActors(), CopiedActorData))
	{
		HTR_LOG(Warning, "Copy: nothing to copy");
		return;
	}
	HTR_LOG(Info, "Copied {} actor(s)", Selection.GetActors().Num());
}

// 복사한 위치에 그대로 만들고, 새 액터들을 선택해 바로 기즈모로 옮길 수 있게 한다.
void UEditorEngine::PasteCopiedActors()
{
	TArray<AActor*> PastedActors;
	if (!FJsonArchive::SpawnActorsFromBytes(EditorWorld, CopiedActorData, PastedActors) || PastedActors.IsEmpty())
	{
		HTR_LOG(Warning, "Paste: failed to create actors");
		return;
	}
	Selection.SelectActors(PastedActors);
	HTR_LOG(Info, "Pasted {} actor(s)", PastedActors.Num());
}

// View 하나의 씬을 그린다.
void UEditorEngine::RenderFrame(const FSceneView& SceneView, const FSceneRenderSettings& Settings, UWorld& ViewWorld, 
	const FSceneRenderTargets& Targets, FRenderQueue& InRenderQueue)
{
	FSceneRenderSettings ViewSettings = Settings;
	FSceneView View = SceneView;

	if (ViewWorld.IsPlayInEditor())
	{
		if (APlayerController* PlayerController = ViewWorld.GetPlayerController())
		{
			const FMinimalViewInfo& PlayerView = PlayerController->GetPlayerView();
			View.ConstrainedAspectRatio = PlayerView.AspectRatio;
			View.bConstrainAspectRatio = PlayerView.bConstrainAspectRatio;
			// PIE View는 PCM의 화면 효과를 쓴다.
			// 에디터 설정에서 비네트를 켰다면 PCM 값 대신 그 값으로 미리 볼 수 있게 덮어쓴다.
			const FVignetteSettings EditorVignette = ViewSettings.ScreenEffects.Vignette;
			ViewSettings.ScreenEffects = PlayerController->GetScreenEffects();
			if (EditorVignette.bEnabled)
				ViewSettings.ScreenEffects.Vignette = EditorVignette;

			if (APlayerActor* Player = Cast<APlayerActor>(PlayerController->GetPawn()))
			{
				ViewSettings.DamageEffectIntensity = Player->GetDamageEffectIntensity();
			}
		}
	}
	else if (LevelSequencerPanel)
	{
		float FadeAmount = 0.f;
		FLinearColor FadeColor;
		if (LevelSequencerPanel->GetFadePreview(FadeAmount, FadeColor))
		{
			ViewSettings.ScreenEffects.FadeAmount = FadeAmount;
			ViewSettings.ScreenEffects.FadeColor = FadeColor;
		}
	}

	SceneRenderer.Render(ViewWorld, View, ViewSettings, Targets, InRenderQueue);
}

// View Texture가 포함된 UI를 Swapchain 백버퍼에 합성한다. Present는 FEngineLoop가 한다.
void UEditorEngine::PresentFrame()
{
	// Swapchain 렌더링
	RenderCommand::BeginRenderPass(MainWindowSC->GetRenderingInfo());

	{
		SCOPE_CYCLE_COUNTER(STAT_ImGui);

		ImGuiRenderer->Begin();

		EditorUI->OnRender();

		ImGuiRenderer->End();
	}

	RenderCommand::EndRenderPass();
}

// ImGui를 정리한다. UObject 일괄 삭제와 공용 자원·Device 정리는 FEngineLoop가 이어서 한다.
void UEditorEngine::PreExit()
{
	// 에셋 탭은 자기 월드(Preview)를 가진다. 종료 때 모든 UObject가 한꺼번에 지워지기 전에 탭을 먼저 닫아
	// 탭이 자기 월드를 정리하게 한다. 늦게 지우면 이미 해제된 월드를 정리하려다 터진다
	while (EditorUI->GetTabs().Num() > 1)
		EditorUI->RemoveTab(EditorUI->GetTabs().Num() - 1);

	if (EditorWorld)
	{
		// 편집 월드 종료 전 액터 파괴 구독 해제
		EditorWorld->OnActorDestroyed.RemoveAll(this);
		DestroyWorldContext(EditorWorld);
		EditorWorld = nullptr;
	}

	if (PlayWorld)
	{
		DestroyWorldContext(PlayWorld);
		PlayWorld = nullptr;
	}

	// PIE 창이 열린 채 종료하면 Device가 살아 있을 때 Swapchain과 타깃을 먼저 정리한다.
	DestroyPIEWindow();
	ImGuiRenderer->Shutdown();
}

// 선택과 Gizmo 참조를 정리한 뒤 Actor를 삭제한다.
void UEditorEngine::DeleteActor(AActor* Actor)
{
	if (!Actor)
		return;
	// 실행 중 삭제는 게임 로직이 들고 있는 참조를 깨뜨릴 수 있어 Pause 상태에서만 허용한다.
	if (IsPlaying() && !bIsPaused)
		return;

	// 선택에서 먼저 빼야 기즈모·디테일·외곽선이 지워질 액터를 가리키지 않는다.
	Selection.Remove(Actor);
	GizmoDragStarts.Reset();

	// 에디터 조작은 World Tick 밖이라 즉시 파괴해도 안전하다.
	// 예약하면 일시정지 PIE에서는 World Tick이 돌지 않아 Resume 전까지 남는다.
	if (UWorld* World = Actor->GetWorld())
	{
		World->DestroyActor(Actor);
	}
}

void UEditorEngine::DeleteSelectedActors()
{
	if (IsPlaying() && !bIsPaused)
		return;

	// 지우는 동안 선택 목록이 바뀌므로 복사본으로 순회한다.
	const TArray<AActor*> ActorsToDelete = Selection.GetActors();
	Selection.Clear();
	for (AActor* Actor : ActorsToDelete)
		DeleteActor(Actor);
}

void UEditorEngine::DeleteComponent(UActorComponent* Component)
{
	if (!Component)
		return;
	if (IsPlaying() && !bIsPaused)
		return;
	DetailsPanel->SelectComponent(nullptr);
	Outline->SetTarget(nullptr);
	Component->GetOwner()->DestroyComponent(Component);
	// 지운 컴포넌트를 뺀 상태로 기즈모·디테일·외곽선을 선택에 다시 맞춘다.
	SyncSelectionToEditors();
}

void UEditorEngine::StartPlayInEditorSession()
{
	// 별도 창 모드면 패널 View는 모두 에디터 월드로 두고 창 하나에만 PIE 월드를 그린다.
	// 창을 만들지 못하면 선택한 View에서 실행한다.
	if (PlayMode == EPlayModeType::InEditorFloating && CreatePIEWindow())
	{
		PIEViewIndex = PIEWindowViewIndex;
	}
	else
	{
		// 지금 편집 중인 View 하나만 PIE 월드를 보여준다.
		PIEViewIndex = ViewportsPanel->GetEditorViewIndex();
	}

	FWorldContext& PIEContext = CreateNewWorldContext(EWorldType::PIE);
	PlayWorld = CreatePIEWorldByDuplication(PIEContext, EditorWorld);
	PIEContext.OnWorldChanged.AddRaw(this, &UEditorEngine::HandleOnPIEWorldChanged);

	// PIE 시작 전 편집 선택 해제 및 아웃라이너 월드 전환
	ResetSceneSelection();
	if (PlayWorld)
	{
		// BeginPlay 중 액터 파괴까지 포함하는 PIE 월드 구독
		PlayWorld->OnActorDestroyed.AddRaw(this, &UEditorEngine::HandleActorDestroyed);
		OutlinerPanel->SetWorld(PlayWorld);
		GameViewport = NewObject<UGameViewportClient>(nullptr, FName("MainViewport"));

		PlayWorld->BeginPlay();

		APlayerController* PlayerController = PlayWorld->GetPlayerController();
		APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		if (!PlayWorld->HasPlayerStart() && Pawn)
		{
			const int32 SourceViewIndex = ViewportsPanel->GetEditorViewIndex();
			const FViewCamera& EditorCam = ViewportsPanel->GetEditorClient(SourceViewIndex).GetCamera();

			FRotator ViewRotation = EditorCam.Transform.Rotation.ToFRotator();
			ViewRotation.Roll = 0.0f;

			Pawn->SetActorLocation(EditorCam.Transform.Location);
			PlayerController->SetControlRotation(ViewRotation);
			Pawn->FaceRotation(ViewRotation, 0.0f);
			PlayerController->UpdateCamera(0.0f);
		}
	}

}

UWorld* UEditorEngine::CreatePIEWorldByDuplication(FWorldContext& PIEContext, UWorld* InEditorWorld)
{
	UWorld* NewPIEWorld = UWorld::GetDuplicatedWorldForPIE(InEditorWorld);
	PIEContext.SetCurrentWorld(NewPIEWorld);
	return NewPIEWorld;
}

void UEditorEngine::EndPlayMap()
{
	if (!PlayWorld)
	{
		return;
	}

	DestroyWorldContext(PlayWorld);
	DestroyPIEWindow();

	delete GameViewport;
	GameViewport = nullptr;

	PIEViewIndex = InvalidViewIndex;

	OutlinerPanel->SetWorld(EditorWorld);
}

bool UEditorEngine::CreatePIEWindow()
{
	constexpr int32 DefaultWidth = 1280;
	constexpr int32 DefaultHeight = 720;

	TUniquePtr<FWindow> NewWindow = MakeUnique<FWindow>();
	if (!NewWindow->Create(::GetModuleHandleW(nullptr), DefaultWidth, DefaultHeight, L"Hitori Engine - PIE"))
	{
		HTR_LOG(Error, "Failed to create PIE window. Falling back to the selected viewport.");
		return false;
	}
	// 닫기 버튼은 PIE만 끝내고, ImGui는 메인 창 입력만 받도록 훅을 끊는다.
	NewWindow->SetQuitOnClose(false);
	NewWindow->SetReceivesWndProcHook(false);

	PIEWindow = std::move(NewWindow);
	PIESwapchain = MakeUnique<FSwapchain>(GetEngineLoop().GetRenderDevice(), PIEWindow.get());
	PIEWindowViewport.ReleaseRenderTargets();
	PIEWindow->Show();
	return true;
}

void UEditorEngine::DestroyPIEWindow()
{
	if (!PIEWindow)
		return;

	PIEWindowViewport.SetOutputSize({});
	PIEWindowViewport.EndPlay();

	PIEWindowViewport.ReleaseRenderTargets();
	bPIEWindowRendered = false;

	// Swapchain이 창 핸들을 출력 대상으로 쥐고 있으므로 창보다 먼저 놓는다.
	PIESwapchain.reset();
	PIEWindow->Destroy();
	PIEWindow.reset();

	// PIE 창이 포커스를 갖고 있었다면 에디터로 돌려준다.
	::SetForegroundWindow(MainWindow->GetHandle());
}

bool UEditorEngine::UpdatePIEWindowTargets()
{
	const uint32 Width = PIEWindow->GetWidth();
	const uint32 Height = PIEWindow->GetHeight();
	// 최소화 중에는 그릴 대상이 없다.
	if (Width == 0 || Height == 0 || ::IsIconic(PIEWindow->GetHandle()))
	{
		PIEWindowViewport.SetOutputSize({});
		return false;
	}

	if (PIEWindow->CheckResized())
		PIESwapchain->Resize(static_cast<int32>(Width), static_cast<int32>(Height));

	// PIE 창 View 출력 크기는 창 클라이언트 크기와 같다.
	PIEWindowViewport.SetOutputSize({static_cast<float>(Width), static_cast<float>(Height)});
	PIEWindowViewport.EnsureRenderTargets();
	return true;
}

void UEditorEngine::CopyPIEWindowToBackbuffer()
{
	const FRenderingInfo& BackbufferInfo = PIESwapchain->GetRenderingInfo();
	if (BackbufferInfo.ColorRenderTargets.Num() == 0)
		return;

	// 백버퍼는 SRV가 없어 FXAA·Fog 입력으로 쓸 수 없으므로 오프스크린 결과를 통째로 복사한다.
	// 창 크기 변경 직후 한 프레임은 크기가 어긋날 수 있어 그때는 건너뛴다.
	FTexture2D* Backbuffer = BackbufferInfo.ColorRenderTargets[0].Texture;
	const FSceneRenderTargets& Targets = PIEWindowViewport.GetRenderTargets();
	if (!Backbuffer || !Targets.ViewColor ||
		BackbufferInfo.ViewportSetting.Width != Targets.Width ||
		BackbufferInfo.ViewportSetting.Height != Targets.Height)
		return;

	RenderCommand::CopyTexture(Backbuffer, Targets.ViewColor.get());
	bPIEWindowRendered = true;
}


// 씬 변경으로 무효화된 에디터의 선택 참조를 모두 해제한다.
void UEditorEngine::ResetSceneSelection()
{
	Selection.Clear();
	GizmoDragStarts.Reset();
	Gizmo->SetTarget(nullptr, false);
	Outline->SetTarget(nullptr);
	DetailsPanel->SetTarget(nullptr);
}

// 새 씬 생성이 성공하면 에디터 선택 상태를 초기화한다.
void UEditorEngine::CreateNewScene()
{
	if (IsPlaying())
	{
		HTR_LOG(Warning, "Cannot create a new scene during PIE. Stop the play session first.");
		return;
	}
	if (!FEditorFileUtils::NewScene(EditorWorld))
		return;

	ResetSceneSelection();
}

// 씬 불러오기가 성공하면 에디터 선택 상태를 초기화한다.
void UEditorEngine::OpenScene()
{
	if (IsPlaying())
	{
		HTR_LOG(Warning, "Cannot open a scene during PIE. Stop the play session first.");
		return;
	}
	if (!FEditorFileUtils::LoadScene(EditorWorld))
		return;

	ResetSceneSelection();
}

// 공통 파일 유틸리티로 현재 씬을 저장한다.
void UEditorEngine::SaveCurrentScene()
{
	if (IsPlaying())
	{
		HTR_LOG(Warning, "Cannot save during PIE. Stop the play session first.");
		return;
	}
	FEditorFileUtils::SaveScene(EditorWorld);
}

// 공통 파일 유틸리티로 새 경로에 씬을 저장한다.
void UEditorEngine::SaveSceneAs()
{
	if (IsPlaying())
	{
		HTR_LOG(Warning, "Cannot save during PIE. Stop the play session first.");
		return;
	}
	FEditorFileUtils::SaveSceneAs(EditorWorld);
}
