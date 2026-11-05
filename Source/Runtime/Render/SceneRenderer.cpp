#include "EnginePCH.h"
#include "SceneRenderer.h"

#include "ColorWorkflow.h"
#include "DepthViewRenderer.h"
#include "FogRenderer.h"
#include "FXAARenderer.h"
#include "GammaEncodeRenderer.h"
#include "RenderCommand.h"
#include "Renderer.h"
#include "RenderingInfo.h"
#include "SceneRenderTargets.h"
#include "SkyboxRenderer.h"
#include "Component/TextRenderComponent.h"
#include "Core/ResourcePaths.h"
#include "Engine/Scene.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Text/TextRenderer.h"
#include "UObject/UObjectIterator.h"
#include "Render/DecalRenderer.h"
#include "Core/Stats/LightweightStats.h"
#include "Core/StatOverlay.h"

DECLARE_CYCLE_STAT("Decal Draw Time", STAT_DecalDrawTime);

FSceneRenderer::FSceneRenderer() = default;

FSceneRenderer::~FSceneRenderer() = default;

void FSceneRenderer::Init(FRenderer* InRenderer)
{
	Renderer = InRenderer;

	SkyboxRenderer = MakeUnique<FSkyboxRenderer>();
	SkyboxRenderer->Init(ResourcePaths::DefaultSkyPanorama);

	FogRenderer = MakeUnique<FFogRenderer>();
	FogRenderer->Init(Renderer);

	FXAARenderer = MakeUnique<FFXAARenderer>();
	FXAARenderer->Init();

	GammaEncodeRenderer = MakeUnique<FGammaEncodeRenderer>();
	GammaEncodeRenderer->Init();

	DepthViewRenderer = MakeUnique<FDepthViewRenderer>();
	DepthViewRenderer->Init();

	TextRenderer = MakeUnique<FTextRenderer>();
	TextRenderer->Init();

	DecalRenderer = MakeUnique<FDecalRenderer>();
	DecalRenderer->Init();
}

void FSceneRenderer::BeginFrame()
{
	bShadowAtlasRenderedThisFrame = false;

	// 프레임 전체 뷰의 데칼 통계 초기화
	FStatOverlay::SetDecalStats(FDecalStats{});
}

void FSceneRenderer::Render(UWorld& World, const FSceneView& SceneView, const FSceneRenderSettings& Settings,
	const FSceneRenderTargets& Targets, FRenderQueue& RenderQueue)
{
	FLightingConstants Lighting;
	FShadowStats ShadowStats{};
	TArray<FPointLightInfo> PointLights;
	TArray<FSpotLightInfo> SpotLights;
	TArray<FDirectionalLightInfo> DirectionalLights;
	TArray<FShadowInfo> ShadowInfos;

	//프리뷰를 보여줄 LightProxy
	FLightSceneProxy* PreviewShadowMapLightProxy = nullptr;

	World.GetScene().BuildLightingConstants(Lighting);
	const bool bDrawShadows = Settings.bDrawShadows;

	FFrustumPlanes FrustumPlanes = ExtractFrustumPlanes(SceneView.ViewProjection);

	World.GetScene().BuildStructuredLightAndShadowData(PointLights, SpotLights, DirectionalLights, ShadowInfos, PreviewShadowMapLightProxy, ShadowStats, bDrawShadows, &FrustumPlanes, SceneView.Location);

	// 라이트 컬링은 View마다 다르지만 아틀라스는 하나라서, 다른 View가 방금 자기 라이트로 덮어썼을 수 있다.
	// 아틀라스에 지금 들어 있는 것이 이 View의 목록과 같고 그 뒤로 씬이 바뀌지 않았을 때만 재사용한다.
	const bool bSameShadowSet = ShadowAtlasWorld == &World
		&& ShadowAtlasInfos.Num() == ShadowInfos.Num()
		&& (ShadowInfos.IsEmpty() || std::memcmp(ShadowAtlasInfos.GetData(), ShadowInfos.GetData(), sizeof(FShadowInfo) * ShadowInfos.Num()) == 0);
	const bool bSceneShadowUnchanged = !World.GetScene().IsShadowDirty() || bShadowAtlasRenderedThisFrame;
	const bool bCacheHit = FScene::bCacheWholeSceneShadows && bSameShadowSet && bSceneShadowUnchanged
		&& bShadowAtlasDrawSkeletalMeshes == Settings.bDrawSkeletalMeshes;

	Renderer->SetLighting(Lighting, PointLights, SpotLights);
	Renderer->SetShadowFilterSettings(Settings.ShadowFilter);

	const FShadowInfo* DirectionalShadowInfo = bDrawShadows ? World.GetScene().GetDirectionalShadowInfo() : nullptr;

	//Shadow Info StructuredBuffer, ShadowMap 만들기
	if (DirectionalShadowInfo != nullptr || ShadowInfos.Num() > 0)
	{
		// 아틀라스를 다시 그리면 무엇을 그렸는지 기록한다. RenderShadowPass가 타일 배치를 채우므로 그 전에 복사한다.
		if (!bCacheHit)
		{
			ShadowAtlasWorld = &World;
			ShadowAtlasInfos = ShadowInfos;
			bShadowAtlasRenderedThisFrame = true;
			bShadowAtlasDrawSkeletalMeshes = Settings.bDrawSkeletalMeshes;
		}

		Renderer->RenderShadowPass(
			DirectionalShadowInfo,
			Lighting.Directional.Direction,
			SceneView.Location,
			SceneView.Forward,
			SceneView.FovDegrees,
			SceneView.AspectRatio,
			ShadowInfos,
			World.GetScene().Proxies,
			PreviewShadowMapLightProxy,
			ShadowStats,
			Settings.bVisualizeCascades,
			bCacheHit,
			Settings.bDrawSkeletalMeshes
		);
	}


	FStatOverlay::SetShadowStats(ShadowStats);

	Renderer->RenderQueueSorting(RenderQueue, SceneView.ViewProjection, SceneView.Location);

	RenderSurfacePrepass(SceneView, Settings, Targets);

	RenderDecalPass(SceneView, Settings, World, Targets);

	// 1. 불투명·반투명 Pass (Linear 씬 컬러)
	RenderOpaquePass(SceneView, Settings, World, Targets, RenderQueue, Lighting.Directional);
	RenderTranslucentPass(SceneView, Settings, World, Targets);

	if (Settings.bDepthView)
	{
		// 깊이 시각화는 표시용 타깃을 통째로 덮어쓰므로 인코딩이 필요 없다
		RenderDepthPass(SceneView, Targets);
	}
	else
	{
		// 데이터 시각화 뷰는 씬 컬러 값 자체가 결과이므로 안개·인코딩·FXAA로 값을 바꾸지 않는다.
		const EViewMode ViewMode = SceneView.RenderMode.ViewMode;
		const bool bVisualizeData = IsDataVisualization(ViewMode);

		//직교인 경우 안개 Pass를 그리지 않는다.
		if (!SceneView.bOrthographic && !bVisualizeData && Settings.bExponentialHeightFog)
			// 2. 안개 Pass (Linear 씬 컬러)
			RenderFogPass(SceneView, World, Targets);

		// 3. Gamma Encode Pass: Linear 씬 컬러 → (Lit이면 ACES) → 표시용 sRGB
		const bool bEncode = !bVisualizeData && ColorWorkflow::bLinear;
		const bool bToneMap = bEncode && Settings.bToneMapping && ViewMode == EViewMode::Lit;
		RenderGammaEncodePass(Targets, bEncode, bToneMap, Settings.ExposureEV, SceneView.bConstrainAspectRatio, SceneView.ConstrainedAspectRatio, Settings.DamageEffectIntensity, Settings.ScreenEffects);

		// 4. FXAA Pass: 밝기 계산이 감마 공간을 전제로 하므로 인코딩 뒤에 한다
		if (!bVisualizeData && Settings.bEnableFXAA)
			RenderFXAAPass(Targets);
	}
}

void FSceneRenderer::RenderSurfacePrepass(const FSceneView& SceneView, const FSceneRenderSettings& Settings, const FSceneRenderTargets& Targets)
{
	FRenderingInfo PassInfo;
	PassInfo.ViewportSetting.Width = Targets.Width;
	PassInfo.ViewportSetting.Height = Targets.Height;

	FRenderingDesc& ObjectMaskDesc = PassInfo.ColorRenderTargets.AddDefaulted_GetRef();
	ObjectMaskDesc.Texture = Targets.ObjectMask.get();
	ObjectMaskDesc.LoadOp = ERenderTargetLoadOp::Clear;

	FRenderingDesc& NormalDesc = PassInfo.ColorRenderTargets.AddDefaulted_GetRef();
	NormalDesc.Texture = Targets.Normal.get();
	NormalDesc.LoadOp = ERenderTargetLoadOp::Clear;
	NormalDesc.ClearValue.ColorClearValue = FVector4(0.0f);

	PassInfo.DepthStencil.Texture = Targets.Depth.get();
	PassInfo.DepthStencil.LoadOp = ERenderTargetLoadOp::Clear;
	PassInfo.DepthStencil.ClearValue.DepthClearValue = 1.0f;
	PassInfo.DepthStencil.ClearValue.StencilClearValue = 0;

	RenderCommand::BeginRenderPass(PassInfo);

	if (Settings.bDrawPrimitives)
	{
		Renderer->RenderSurface();
	}

	RenderCommand::EndRenderPass();
}

void FSceneRenderer::RenderOpaquePass(const FSceneView& SceneView, const FSceneRenderSettings& Settings, const UWorld& ViewWorld,  const FSceneRenderTargets& Targets, FRenderQueue& RenderQueue, const FDirectionalLightInfo& DirectionalLight)
{
	// 뷰 모드가 셰이더 permutation을, Wireframe 여부가 Fill Mode를 정한다. 삼각형 연결은 그대로다.
	const FViewRenderMode ViewRenderMode = SceneView.RenderMode;
	const bool bVisualizeLightCount = Settings.bVisualizeTileLightCount;
	if (ViewRenderMode.ViewMode == EViewMode::Lit)
	{
		Renderer->EnsureTileLightCapacity(
			SceneView.ViewProjection,
			Targets.Width,
			Targets.Height, bVisualizeLightCount);
	}

	// 배경은 화면에서 sRGB 0.3 회색으로 보여야 한다. Linear 작업 흐름에서는 인코딩될 것이므로 디코딩해 지운다.
	FRenderingInfo PassInfo;
	PassInfo.ViewportSetting.Width = Targets.Width;
	PassInfo.ViewportSetting.Height = Targets.Height;

	// 출력 순서는 UberLit의 SV_Target0/1/2에 대응한다.
	FRenderingDesc SceneColorDesc;
	SceneColorDesc.Texture = Targets.SceneColor.get();
	SceneColorDesc.LoadOp = ERenderTargetLoadOp::Clear;

	const float ClearValue = ColorWorkflow::bLinear ? FLinearColor::SRGBToLinear(0.3f) : 0.3f;
	SceneColorDesc.ClearValue.ColorClearValue = FVector4(ClearValue, ClearValue, ClearValue, 1.0f);

	PassInfo.ColorRenderTargets.Add(SceneColorDesc);

	PassInfo.DepthStencil.Texture = Targets.Depth.get();
	PassInfo.DepthStencil.LoadOp = ERenderTargetLoadOp::Clear;
	PassInfo.DepthStencil.ClearValue.DepthClearValue = 1.0f;
	PassInfo.DepthStencil.ClearValue.StencilClearValue = 0;

	RenderCommand::BeginRenderPass(PassInfo);

	const bool bDrawPrimitives = Settings.bDrawPrimitives;
	const ERasterizerState SceneRasterizerState = ViewRenderMode.ViewMode == EViewMode::Wireframe
		? ERasterizerState::Wireframe : ERasterizerState::SolidBack;

	// 렌더 루프 — 반드시 RenderAll보다 먼저
	// 직교는 시선이 모두 평행해 파노라마의 한 점만 확대되므로 하늘을 그리지 않고 지우기 색을 배경으로 쓴다.
	// 데이터 시각화 뷰도 하늘은 데이터가 없으므로 그리지 않는다.
	if (!SceneView.bOrthographic && !IsDataVisualization(ViewRenderMode.ViewMode))
	{
		SkyboxRenderer->OnRender(SceneView.ViewProjection, SceneView.Location, DirectionalLight);
	}

	RenderCommand::EndRenderPass();

	// 배경 출력 이후의 불투명 표면 데이터 타깃 연결
	PassInfo.ColorRenderTargets[0].LoadOp = ERenderTargetLoadOp::Load;
	FRenderingDesc NormalDesc;
	NormalDesc.Texture = Targets.Normal.get();
	NormalDesc.LoadOp = ERenderTargetLoadOp::Clear;
	NormalDesc.ClearValue.ColorClearValue = FVector4(0.f);
	PassInfo.ColorRenderTargets.Add(NormalDesc);
	FRenderingDesc ObjectMaskDesc;
	ObjectMaskDesc.Texture = Targets.ObjectMask.get();
	ObjectMaskDesc.LoadOp = ERenderTargetLoadOp::Clear;
	ObjectMaskDesc.ClearValue.ColorClearValue = FVector4(0.f);
	PassInfo.ColorRenderTargets.Add(ObjectMaskDesc);
	PassInfo.DepthStencil.LoadOp = ERenderTargetLoadOp::Load;
	RenderCommand::BeginRenderPass(PassInfo);

	if (bDrawPrimitives)
	{
		RenderCommand::SetRasterizerState(SceneRasterizerState);
		RenderCommand::SetBlendState(EBlendState::Opaque);
		RenderCommand::SetDepthStencilState(EDepthStencilState::Default);
		RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		Renderer->SetViewMode(ViewRenderMode);
		
		// 불투명을 먼저 그리고 반투명을 먼 것부터 그린다.
		// Grid는 깊이를 쓰지 않는 반투명 위에도 겹칠 수 있지만, 언리얼의 에디터 프리미티브처럼 Overlay Pass에서 마지막에 합성한다.
		RenderCommand::BindShaderResource(4, Targets.DecalTexture.get(), EShaderBindFlagBits::Pixel);
		RenderCommand::BindShaderResource(5, Targets.DecalNormal.get(), EShaderBindFlagBits::Pixel);
		Renderer->BindShadowForShading();
		Renderer->RenderOpaque();
		RenderCommand::UnbindShaderResource(4, EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
		RenderCommand::UnbindShaderResource(5, EShaderBindFlagBits::Pixel);
		// 장면 Wireframe이 Grid·Gizmo·UI로 전파되지 않도록 복원한다.
		RenderCommand::SetRasterizerState(ERasterizerState::SolidBack);
	}

	RenderCommand::EndRenderPass();
}

void FSceneRenderer::RenderDecalPass(const FSceneView& SceneView, const FSceneRenderSettings& Settings,
	const UWorld& ViewWorld, const FSceneRenderTargets& Targets)
{
	FTexture2D* ObjectMaskTexture = Targets.ObjectMask.get();
	FTexture2D* DepthTexture = Targets.Depth.get();

	FRenderingInfo PassInfo;

	FRenderingDesc& RenderTarget = PassInfo.ColorRenderTargets.AddDefaulted_GetRef();
	RenderTarget.Texture = Targets.DecalTexture.get();
	RenderTarget.LoadOp = ERenderTargetLoadOp::Clear;
	RenderTarget.ClearValue.ColorClearValue = FVector4(0.f);

	// 독립적인 노멀 적용 비중을 가진 두 번째 DBuffer
	FRenderingDesc& NormalDesc = PassInfo.ColorRenderTargets.AddDefaulted_GetRef();
	NormalDesc.Texture = Targets.DecalNormal.get();
	NormalDesc.LoadOp = ERenderTargetLoadOp::Clear;
	NormalDesc.ClearValue.ColorClearValue = FVector4(0.0f);

	PassInfo.DepthStencil.Texture = nullptr;
	PassInfo.ViewportSetting.Width = Targets.Width;
	PassInfo.ViewportSetting.Height = Targets.Height;

	RenderCommand::BeginRenderPass(PassInfo);

	// 시각화 및 표시 비활성 뷰에서도 이전 프레임의 데칼 색상 제거
	FDecalStats DecalStats = FStatOverlay::GetDecalStats();
	const EViewMode ViewMode = SceneView.RenderMode.ViewMode;
	if (Settings.bDrawDecals && Settings.bDrawPrimitives && !Settings.bDepthView && !Settings.bVisualizeCascades
		&& !Settings.bVisualizeTileLightCount && (ViewMode == EViewMode::Lit || ViewMode == EViewMode::Unlit
		|| ViewMode == EViewMode::WorldNormal))
	{
		uint32 DrawCount = 0;

		// 현재 뷰의 CPU 제출 시간 측정
		FScopeCycleCounter DecalTimer(STAT_DecalDrawTime, true);
		DecalRenderer->Render(ViewWorld.GetScene().Decals, SceneView.ViewProjection, DepthTexture, ObjectMaskTexture,
			Targets.Normal.get(), DrawCount, ViewWorld.GetWorldType() != EWorldType::Editor);
		const uint64 DrawCycles = DecalTimer.Finish();

		// 동일 프레임의 뷰별 제출 수와 CPU 처리 시간 합산
		DecalStats.DrawCount += DrawCount;
		DecalStats.DrawTime += static_cast<float>(FPlatformTime::ToMilliseconds64(DrawCycles));
	}

	RenderCommand::UnbindShaderResource(0, EShaderBindFlagBits::Pixel);
	RenderCommand::EndRenderPass();

	FStatOverlay::SetDecalStats(DecalStats);
}

void FSceneRenderer::RenderTranslucentPass(const FSceneView& SceneView, const FSceneRenderSettings& Settings, const UWorld& ViewWorld, const FSceneRenderTargets& Targets)
{
	if (!Settings.bDrawPrimitives)
	{
		return;
	}

	// 표면 데이터 타깃을 제외한 기존 텍스트·반투명 출력
	FRenderingInfo PassInfo;
	PassInfo.ViewportSetting.Width = Targets.Width;
	PassInfo.ViewportSetting.Height = Targets.Height;
	FRenderingDesc& SceneColorDesc = PassInfo.ColorRenderTargets.AddDefaulted_GetRef();
	SceneColorDesc.Texture = Targets.SceneColor.get();
	SceneColorDesc.LoadOp = ERenderTargetLoadOp::Load;
	PassInfo.DepthStencil.Texture = Targets.Depth.get();
	PassInfo.DepthStencil.LoadOp = ERenderTargetLoadOp::Load;
	RenderCommand::BeginRenderPass(PassInfo);

	const ERasterizerState SceneRasterizerState = SceneView.RenderMode.ViewMode == EViewMode::Wireframe
		? ERasterizerState::Wireframe : ERasterizerState::SolidBack;
	DrawWorldText(SceneView, ViewWorld);
	RenderCommand::SetRasterizerState(SceneRasterizerState); // TextRenderer가 상태를 바꾸므로 되돌림
	Renderer->RenderTranslucent();
	// 장면 Wireframe이 Grid·Gizmo·UI로 전파되지 않도록 복원한다.
	RenderCommand::SetRasterizerState(ERasterizerState::SolidBack);
	RenderCommand::EndRenderPass();
}

void FSceneRenderer::DrawWorldText(const FSceneView& SceneView, const UWorld& ViewWorld)
{
	for (TObjectIterator<UTextRenderComponent> TextComponent; TextComponent; ++TextComponent)
	{
		if (!TextComponent || !TextComponent->GetFont() || !TextComponent->IsVisible())
		{
			continue;
		}
		// 에디터·PIE 월드의 컴포넌트가 함께 순회되므로 이 View의 월드만 그린다.
		if (!TextComponent->GetOwner() || TextComponent->GetOwner()->GetWorld() != &ViewWorld)
		{
			continue;
		}

		TextRenderer->OnRender(
			TextComponent->GetText(),
			TextComponent->GetWorldMatrix(),
			TextComponent->GetTextSize(),
			*TextComponent->GetFont(),
			SceneView.ViewProjection
		);
	}
}

void FSceneRenderer::RenderFogPass(const FSceneView& SceneView, const UWorld& ViewWorld, const FSceneRenderTargets& Targets)
{
	const TArray<FExponentialHeightFogSceneInfo>& SceneFogInfos = ViewWorld.GetScene().FogInfos;

	if (SceneFogInfos.Num() == 0)
		return;

	FRenderingInfo PassInfo;

	FRenderingDesc& RenderTarget = PassInfo.ColorRenderTargets.AddDefaulted_GetRef();
	RenderTarget.Texture = Targets.SceneColor.get();
	RenderTarget.LoadOp = ERenderTargetLoadOp::Load;

	PassInfo.DepthStencil.Texture = nullptr;
	PassInfo.ViewportSetting.Width = Targets.Width;
	PassInfo.ViewportSetting.Height = Targets.Height;

	RenderCommand::BeginRenderPass(PassInfo);

	FogRenderer->OnRender(Targets.Depth.get(), SceneView.ViewProjection, SceneView.Location, SceneFogInfos[0].FogInfo);

	//Fog는 여러개있더라도 1개의 Fog만 인식해서 그리도록 해야한다.

	RenderCommand::EndRenderPass();
}

void FSceneRenderer::RenderGammaEncodePass(const FSceneRenderTargets& Targets, bool bEncode, bool bToneMap,
	float ExposureEV, bool bLetterbox, float TargetAspect, float InDamageIntensity, const FCameraScreenEffects& ScreenEffect)
{
	FTexture2D* SceneColor = Targets.SceneColor.get();
	FTexture2D* ViewColor = Targets.ViewColor.get();
	if (!SceneColor || !ViewColor)
		return;

	// 출력은 표시용 타깃 전체를 덮어쓰므로 Clear·깊이가 필요 없다
	FRenderingInfo PassInfo;
	PassInfo.ViewportSetting.Width = Targets.Width;
	PassInfo.ViewportSetting.Height = Targets.Height;

	FRenderingDesc ColorDesc{};
	ColorDesc.Texture = ViewColor;
	ColorDesc.LoadOp = ERenderTargetLoadOp::DontCare;
	PassInfo.ColorRenderTargets.Add(ColorDesc);
	PassInfo.DepthStencil.Texture = nullptr;

	RenderCommand::BeginRenderPass(PassInfo);
	GammaEncodeRenderer->OnRender(SceneColor, bEncode, bToneMap, ExposureEV, bLetterbox, TargetAspect, InDamageIntensity,
		Targets.Width, Targets.Height, ScreenEffect);

	RenderCommand::EndRenderPass();

	// 다음 프레임에 SceneColor를 RTV로 쓸 때 SRV로 남아 있으면 D3D가 강제로 해제하므로 먼저 비운다
	RenderCommand::UnbindShaderResource(0, EShaderBindFlagBits::Pixel);
}

void FSceneRenderer::RenderFXAAPass(const FSceneRenderTargets& Targets)
{
	FTexture2D* FxaaTarget = Targets.FxaaTemp.get();
	FTexture2D* SceneColor = Targets.ViewColor.get();
	if (!SceneColor || !FxaaTarget)
		return;

	// 출력은 임시 텍스처, 깊이 없음, 어차피 전부 덮어쓰니 Clear 불필요
	FRenderingInfo PassInfo;
	PassInfo.ViewportSetting.Width = Targets.Width;
	PassInfo.ViewportSetting.Height = Targets.Height;

	FRenderingDesc ColorDesc{};
	ColorDesc.Texture = FxaaTarget;
	ColorDesc.LoadOp = ERenderTargetLoadOp::DontCare;
	PassInfo.ColorRenderTargets.Add(ColorDesc);
	PassInfo.DepthStencil.Texture = nullptr;

	//장면 텍스처를 읽어서 임시 텍스처에 FXAA 결과를 그린다
	RenderCommand::BeginRenderPass(PassInfo);
	FXAARenderer->OnRender(SceneColor);
	RenderCommand::EndRenderPass();

	// SceneColor가 SRV로 남아 있으면 Overlay 패스에서 RTV로 쓸 때 D3D가 강제로 해제하므로 먼저 비운다
	RenderCommand::UnbindShaderResource(0, EShaderBindFlagBits::Pixel);

	RenderCommand::CopyTexture(SceneColor, FxaaTarget);
}

void FSceneRenderer::RenderDepthPass(const FSceneView& SceneView, const FSceneRenderTargets& Targets)
{
	FRenderingInfo PassInfo;
	PassInfo.ViewportSetting.Width = Targets.Width;
	PassInfo.ViewportSetting.Height = Targets.Height;
	FRenderingDesc ColorDesc;
	ColorDesc.Texture = Targets.ViewColor.get();
	ColorDesc.LoadOp = ERenderTargetLoadOp::DontCare;
	PassInfo.ColorRenderTargets.Add(ColorDesc);
	PassInfo.DepthStencil.Texture = nullptr;

	RenderCommand::BeginRenderPass(PassInfo);
	DepthViewRenderer->OnRender(Targets.Depth.get(), SceneView.ViewProjection, SceneView.Location, SceneView.Forward);
	RenderCommand::EndRenderPass();
}
