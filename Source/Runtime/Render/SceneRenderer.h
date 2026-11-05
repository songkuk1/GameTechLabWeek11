#pragma once

#include "ShadowFilterSettings.h"
#include "ViewMode.h"
#include "Camera/CameraTypes.h"

class FDepthViewRenderer;
struct FShadowInfo;
class FTextRenderer;
class FGammaEncodeRenderer;
class FFXAARenderer;
class FFogRenderer;
class FSkyboxRenderer;
class FDecalRenderer;
struct FDirectionalLightInfo;
class FRenderQueue;
class FTexture2D;
struct FRenderingInfo;
struct FSceneRenderTargets;
class UWorld;
class FRenderer;

struct FSceneView
{
	FMatrix ViewProjection;
	FVector Location;
	FVector Forward;
	float FovDegrees = 60.0f;
	float AspectRatio = 1.777778f; // 16:9
	float ConstrainedAspectRatio = 1.777778f; // 16:9
	bool bConstrainAspectRatio = false;
	bool bOrthographic = false;
	FViewRenderMode RenderMode;
};

struct FSceneRenderSettings
{
	bool bDrawPrimitives = true;
	bool bDrawSkeletalMeshes = true;
	bool bDrawDecals = true;
	bool bDrawShadows = true;
	FShadowFilterSettings ShadowFilter;
	bool bExponentialHeightFog = true;
	bool bToneMapping = true;
	float ExposureEV = 0.6f;
	bool bEnableFXAA = true;
	bool bDepthView = false;
	bool bVisualizeCascades = false;
	bool bVisualizeTileLightCount = false;
	float DamageEffectIntensity = 0.0f;
	// Fade, 비네트 등 화면 효과 (게임·PIE는 PCM 값, 에디터 View는 에디터 설정 값)
	FCameraScreenEffects ScreenEffects;
};

// 씬(섀도우맵 -> 불투명, 텍스트, 반투명 -> 안개 -> 감마 -> FXAA)을 그리는 렌더러. 결과는 ViewInfo에 남는다.
class FSceneRenderer final
{
public:
	FSceneRenderer();
	~FSceneRenderer();

	void Init(FRenderer* InRenderer);
	void BeginFrame();
	void Render(UWorld& World, const FSceneView& SceneView, const FSceneRenderSettings& Settings,
		const FSceneRenderTargets& Targets, FRenderQueue& RenderQueue);

private:
	void RenderSurfacePrepass(const FSceneView& SceneView, const FSceneRenderSettings& Settings, const FSceneRenderTargets& Targets);

	// 불투명 물체에 대한 Pass
	void RenderOpaquePass(const FSceneView& SceneView, const FSceneRenderSettings& Settings, const UWorld& ViewWorld, const FSceneRenderTargets& Targets, FRenderQueue& RenderQueue, const FDirectionalLightInfo& DirectionalLight);
	void RenderDecalPass(const FSceneView& SceneView, const FSceneRenderSettings& Settings, const UWorld& ViewWorld, const FSceneRenderTargets& Targets);
	// 반투명 물체에 대한 Pass
	void RenderTranslucentPass(const FSceneView& SceneView, const FSceneRenderSettings& Settings, const UWorld& ViewWorld, const FSceneRenderTargets& Targets);
	// 월드에 배치된 텍스트 그리기
	void DrawWorldText(const FSceneView& SceneView, const UWorld& ViewWorld);
	// 안개에 대한 Pass
	void RenderFogPass(const FSceneView& SceneView, const UWorld& ViewWorld, const FSceneRenderTargets& Targets);
	// Gamma Encode Pass: Linear 씬 컬러를 톤매핑하고 화면 표시용 sRGB로 인코딩한다
	void RenderGammaEncodePass(const FSceneRenderTargets& Targets, bool bEncode,
		bool bToneMap, float ExposureEV, bool bLetterbox, float TargetAspect, float InDamageIntensity, const FCameraScreenEffects& ScreenEffect);

	// FXAA Pass
	void RenderFXAAPass(const FSceneRenderTargets& Targets);
	// Depth Pass
	void RenderDepthPass(const FSceneView& SceneView, const FSceneRenderTargets& Targets);

	FRenderer* Renderer = nullptr;
	TUniquePtr<FSkyboxRenderer> SkyboxRenderer;
	TUniquePtr<FFogRenderer> FogRenderer;
	TUniquePtr<FFXAARenderer> FXAARenderer;
	TUniquePtr<FGammaEncodeRenderer> GammaEncodeRenderer;
	TUniquePtr<FDepthViewRenderer> DepthViewRenderer;
	TUniquePtr<FTextRenderer> TextRenderer;
	TUniquePtr<FDecalRenderer> DecalRenderer;

	// 여러 View가 아틀라스 하나를 같이 쓰므로 마지막으로 그린 내용을 기억한다.
	const UWorld* ShadowAtlasWorld = nullptr;
	TArray<FShadowInfo> ShadowAtlasInfos;
	bool bShadowAtlasRenderedThisFrame = false;
	bool bShadowAtlasDrawSkeletalMeshes = true;
};
