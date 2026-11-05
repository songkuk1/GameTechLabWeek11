#include "EnginePCH.h"
#include "GameEngine.h"

#include "Core/Stats/LightweightStats.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Actor/PlayerActor.h"
#include "Launch/LaunchEngineLoop.h"
#include "Render/ColorWorkflow.h"
#include "Render/RenderCommand.h"
#include "Render/Swapchain.h"
#include "Serialization/JsonArchive.h"
#include "Script/LuaScriptSystem.h"
#include "Engine/GameViewportClient.h"

FEngineConfig UGameEngine::GetConfig() const
{
	return {
		.Title = L"Game",
		.Width = 1920,
		.Height = 1080,
		.bBorderless = false,
		.SyncInterval = 1,
		.bCreateDepthBuffer = false,
		.bExitOnEscape = true
	};
}

bool UGameEngine::Init()
{
	if (!Super::Init())
	{
		return false;
	}

	FWorldContext& WorldContext = CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(UWorld::CreateWorld(EWorldType::Game, true));
	World = WorldContext.World();
	WorldContext.OnWorldChanged.AddRaw(this, &UGameEngine::HandleOnPIEWorldChanged);

	GameViewport = NewObject<UGameViewportClient>(nullptr, FName("MainViewport"));

	FString ScenePath = "Assets/Scenes/Title.scene";
	if (__argc > 1 && __argv[1] && __argv[1][0] != '\0')
	{
		ScenePath = __argv[1];
	}

	if (!FJsonArchive::LoadWorld(World, ScenePath))
	{
		HTR_LOG(Warning, "Failed to load scene: {}", ScenePath);
	}

	World->BeginPlay();

	SceneRenderer.Init(GetEngineLoop().GetRenderer());
	SceneRenderTargets.Resize(GetEngineLoop().GetViewportWidth(), GetEngineLoop().GetViewportHeight());

	TextRenderer.Init();

	ColorWorkflow::bLinear = true;

	return true;
}

void UGameEngine::Tick(float DeltaTime)
{
	TickWorldTravel();

	const float GameDeltaTime = DeltaTime * World->GetGlobalTimeDilation();

	World->Tick(GameDeltaTime);
	FLuaScriptSystem::Get().Update(GameDeltaTime);

	const uint32 Width = GetEngineLoop().GetViewportWidth();
	const uint32 Height = GetEngineLoop().GetViewportHeight();
	if (Width == 0 || Height == 0)
		return;

	APlayerController* PlayerController = World->GetPlayerController();
	if (!PlayerController)
	{
		return;
	}

	const FMinimalViewInfo& PlayerView = PlayerController->GetPlayerView();
	const float AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);

	const FMatrix Projection = PlayerView.CalculateProjectionMatrix(AspectRatio);
	const FMatrix ViewProjection = PlayerView.CalculateViewMatrix() * Projection;
	const FFrustumPlanes Frustum = ExtractFrustumPlanes(ViewProjection);
	const FVector Forward = PlayerView.Rotation.Quaternion().RotateVector(FVector{ 1.0f, 0.0f, 0.0f }).Normalized();

	const float ScaleX = Projection.M[1][0];
	const float ScaleY = Projection.M[2][1];

	// LOD에 카메라 정보 저장
	FLODViewContext LODView{
		.Width = Width,
		.Height = Height,
		.CameraPosition = PlayerView.Location,
		.CameraForward = Forward,
		.ProjectionScaleSquared = std::max(ScaleX * ScaleX, ScaleY * ScaleY),
		.NearZ = PlayerView.NearClip,
		.bOrthographic = PlayerView.bOrthographic,
		.ViewProjection = ViewProjection
	};
	LODView.Prepare();

	FRenderer* Renderer = GetEngineLoop().GetRenderer();
	RenderQueue.Reset();

	const FBillboardView BillboardView = FBillboardView::FromView(PlayerView);

	World->GatherRenderPackets(RenderQueue, &LODView, &Frustum, Renderer, &BillboardView);

	const FSceneView SceneView{
		.ViewProjection = ViewProjection,
		.Location = PlayerView.Location,
		.Forward = Forward,
		.FovDegrees = PlayerView.FOV,
		.AspectRatio = AspectRatio,
		.ConstrainedAspectRatio = PlayerView.AspectRatio,
		.bConstrainAspectRatio = PlayerView.bConstrainAspectRatio,
		.bOrthographic = PlayerView.bOrthographic,
	};

	SceneRenderer.BeginFrame();

	FSceneRenderSettings RenderSettings{};

	// Fade, 비네트 등 화면 효과는 Pawn 종류와 상관없이 PCM 값을 쓴다
	RenderSettings.ScreenEffects = PlayerController->GetScreenEffects();

	if (APlayerActor* Player = Cast<APlayerActor>(PlayerController->GetPawn()))
	{
		//RenderSettings.DamageEffectIntensity = 1.0f;
		RenderSettings.DamageEffectIntensity = Player->GetDamageEffectIntensity();
	}
	SceneRenderer.Render(*World, SceneView, RenderSettings, SceneRenderTargets, RenderQueue);

	// ========== UI 렌더링 ===========
	FRenderingInfo UIInfo;
	UIInfo.ViewportSetting.Width = SceneRenderTargets.Width;
	UIInfo.ViewportSetting.Height = SceneRenderTargets.Height;
	FRenderingDesc UIColor;
	UIColor.Texture = SceneRenderTargets.ViewColor.get();
	UIColor.LoadOp = ERenderTargetLoadOp::Load;
	UIInfo.ColorRenderTargets.Add(UIColor);
	UIInfo.DepthStencil.Texture = nullptr;

	RenderCommand::BeginRenderPass(UIInfo);
	GameViewport->CollectDrawList(Width, Height);
	GameViewport->Draw(Width, Height, TextRenderer);
	RenderCommand::EndRenderPass();
	// ================================

	World->GetScene().ClearShadowDirty();

	const FRenderingInfo& Backbuffer = GetEngineLoop().GetSwapchain()->GetRenderingInfo();
	if (Backbuffer.ColorRenderTargets.Num() > 0 && Backbuffer.ViewportSetting.Width == SceneRenderTargets.Width && Backbuffer.ViewportSetting.Height == SceneRenderTargets.Height)
		RenderCommand::CopyTexture(Backbuffer.ColorRenderTargets[0].Texture, SceneRenderTargets.ViewColor.get());

	FStatRegistry::EndFrame();
}

void UGameEngine::OnResize(uint32 Width, uint32 Height)
{
	Super::OnResize(Width, Height);

	SceneRenderTargets.Resize(Width, Height);
}

void UGameEngine::PreExit()
{
	Super::PreExit();

	if (!World)
	{
		return;
	}

	World->EndPlay();
	FLuaScriptSystem::Get().StopAllCoroutines();
	World->ClearWorld();

	delete GameViewport;
	GameViewport = nullptr;

	DestroyWorldContext(World);
	World = nullptr;

	SceneRenderTargets.Release();
}

void UGameEngine::HandleOnPIEWorldChanged(UWorld* OldWorld, UWorld* NewWorld)
{
	if (OldWorld == World)
	{
		OldWorld->EndPlay();
		// 이전 월드 액터(owner)를 붙잡은 코루틴이 삭제 후에 재개되면 해제된 메모리를 읽는다.
		FLuaScriptSystem::Get().StopAllCoroutines();

		World = NewWorld;
	}
}

