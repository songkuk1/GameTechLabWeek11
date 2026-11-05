#include "EnginePCH.h"
#include "Scene.h"
#include "LightSceneProxy.h"
#include "Render/ColorWorkflow.h"
#include <Core/StatOverlay.h>
#include "Component/DecalComponent.h"
#include "Engine/DecalSceneProxy.h"

void FScene::AddPrimitive(UPrimitiveComponent* Component)
{
	if (!Component || Component->SceneProxy)
	{
		return;
	}

	FPrimitiveSceneProxy* Proxy = Component->CreateSceneProxy();
	if (!Proxy)
	{
		return;
	}

	Proxy->Scene = this;
	Proxy->PackedIndex = Proxies.Num();

	Component->SceneProxy = Proxy;

	MarkDirty(Proxy);
	MarkRenderStateDirty(Proxy);

	Proxies.Add(Proxy);
	PrimitiveBounds.Add(FAABB{});
	PrimitiveFlags.Add(0);

	bElementListChanged = true;
}

void FScene::RemovePrimitive(UPrimitiveComponent* Component)
{
	MarkShadowDirty();

	if (!Component || !Component->SceneProxy)
	{
		return;
	}

	FPrimitiveSceneProxy* Proxy = Component->SceneProxy;
	if (!Proxy) return;

	const uint32 Index = static_cast<uint32>(Proxy->PackedIndex);

	Proxies.RemoveAtSwap(Index);
	PrimitiveBounds.RemoveAtSwap(Index);
	PrimitiveFlags.RemoveAtSwap(Index);

	if (Index < static_cast<uint32>(Proxies.Num()))
		Proxies[Index]->PackedIndex = static_cast<int32>(Index);

	if (Proxy->bQueuedForUpdate)
	{
		const int32 Found = DirtyProxies.Find(Proxy);   // 프로젝트 TArray의 Find 이름에 맞게
		if (Found != INDEX_NONE) DirtyProxies.RemoveAtSwap(Found);
	}

	if (Proxy->bRenderStateQueued)
	{
		const int32 Found = RenderStateDirtyProxies.Find(Proxy);
		if (Found != INDEX_NONE) RenderStateDirtyProxies.RemoveAtSwap(Found);
	}

	Component->SceneProxy = nullptr;
	delete Proxy;

	bElementListChanged = true;
}

void FScene::RemoveAllPrimitives()
{
	// 하나씩 RemovePrimitive하면 대기열 검색(Find)이 물체 수만큼 반복되므로 통째로 비운다.
	for (FPrimitiveSceneProxy* Proxy : Proxies)
	{
		if (UPrimitiveComponent* Component = Proxy->GetComponent())
		{
			Component->SceneProxy = nullptr;
		}
		delete Proxy;
	}
	Proxies.Reset();
	PrimitiveBounds.Reset();
	PrimitiveFlags.Reset();
	DirtyProxies.Reset();
	RenderStateDirtyProxies.Reset();
	BVH.Clear();
	bElementListChanged = true;
}

void FScene::AddFogInfo(uint32 Id, const FFogInfo& FogInfo)
{
	FExponentialHeightFogSceneInfo sceneFogInfo;
	sceneFogInfo.Id = Id;
	sceneFogInfo.FogInfo = FogInfo;


	FogInfos.Add(sceneFogInfo);
}

void FScene::RemoveFogInfo(uint32 Id)
{
	// 렌더러는 FogInfos[0]만 쓰므로, 순서를 유지하려고 RemoveAtSwap 대신 RemoveAt을 쓴다.
	for (int32 i = 0; i < FogInfos.Num(); ++i)
	{
		if (FogInfos[i].Id == Id)
		{
			FogInfos.RemoveAt(i);
			return;
		}
	}
}

void FScene::RemoveAllFogInfo()
{
	FogInfos.Reset();

}

void FScene::UpdateFogInfo(uint32 Id, const FFogInfo& FogInfo)
{
	for (FExponentialHeightFogSceneInfo& SceneFogInfo : FogInfos)
	{
		if (SceneFogInfo.Id == Id)
		{
			SceneFogInfo.FogInfo = FogInfo;
			return;
		}


	}
}

void FScene::AddLight(ULightComponentBase* Component)
{
	if (!Component || Component->SceneProxy) return;

	FLightSceneProxy* Proxy = Component->CreateSceneProxy();
	if (!Proxy) return;

	Proxy->Scene = this;
	Component->SceneProxy = Proxy;
	// 다음 UpdateAllTransforms 전에 그려져도 기본값이 아닌 실제 값을 쓰도록 바로 채운다.
	Proxy->UpdateFromComponent();
	Lights.Add(Proxy);
}

void FScene::RemoveLight(ULightComponentBase* Component)
{
	MarkShadowDirty();
	if (!Component || !Component->SceneProxy) return;

	FLightSceneProxy* Proxy = Component->SceneProxy;

	const int32 Index = Lights.Find(Proxy);
	// Ambient·Directional은 등록 순서상 마지막 것을 쓰므로 순서를 유지한다.
	if (Index != INDEX_NONE) Lights.RemoveAt(Index);

	if (Proxy->bQueuedForUpdate)
	{
		const int32 Found = DirtyLights.Find(Proxy);
		if (Found != INDEX_NONE) DirtyLights.RemoveAtSwap(Found);
	}

	Component->SceneProxy = nullptr;
	delete Proxy;
}

void FScene::RemoveAllLights()
{
	for (FLightSceneProxy* Proxy : Lights)
	{
		Proxy->GetComponent()->SceneProxy = nullptr;
		delete Proxy;
	}
	Lights.Reset();
	DirtyLights.Reset();
}

void FScene::MarkLightDirty(FLightSceneProxy* Proxy)
{
	MarkShadowDirty();
	if (Proxy->bQueuedForUpdate) return;
	Proxy->bQueuedForUpdate = true;
	DirtyLights.Add(Proxy);
}

void FScene::BuildLightingConstants(FLightingConstants& Out) const
{
	Out = FLightingConstants{};

	for (const FLightSceneProxy* Proxy : Lights)
	{
		if (!Proxy->IsVisible()) continue;

		switch (Proxy->GetLightType())
		{
		case ELightType::Ambient:
			Out.Ambient.Color = static_cast<const FAmbientLightSceneProxy*>(Proxy)->GetInfo().Color;
			break;
		case ELightType::Directional:
			Out.Directional = static_cast<const FDirectionalLightSceneProxy*>(Proxy)->GetInfo();
			break;
		default:
			break;
		}
	}
}

const FShadowInfo* FScene::GetDirectionalShadowInfo() const
{
	const FShadowInfo* Result = nullptr;
	for (const FLightSceneProxy* Proxy : Lights)
	{
		if (Proxy->GetLightType() == ELightType::Directional && Proxy->ShouldCastShadow() && HasAnyShadowCaster(Proxy))
		{
			Result = &static_cast<const FDirectionalLightSceneProxy*>(Proxy)->GetShadowInfo();
		}
	}
	return Result;
}

void FScene::AddDecal(UDecalComponent* Component)
{
	if (!Component || Component->SceneProxy) return;

	FDecalSceneProxy* Proxy = new FDecalSceneProxy(Component, this);
	Component->SceneProxy = Proxy;
	Proxy->Refresh();
	Decals.Add(Proxy);
}

void FScene::RemoveDecal(UDecalComponent* Component)
{
	if (!Component || !Component->SceneProxy) return;

	FDecalSceneProxy* Proxy = Component->SceneProxy;
	if (Proxy->GetScene() != this)
	{
		return;
	}

	const int32 Index = Decals.Find(Proxy);
	if (Index != INDEX_NONE)
	{
		Decals.RemoveAt(Index);
	}
	Component->SceneProxy = nullptr;
	delete Proxy;
}

void FScene::RemoveAllDecals()
{
	for (FDecalSceneProxy* Proxy : Decals)
	{
		UDecalComponent* Component = Proxy->GetComponent();
		if (Component->SceneProxy == Proxy)
		{
			Component->SceneProxy = nullptr;
		}
		delete Proxy;
	}
	Decals.Reset();
}

void FScene::MarkDecalDirty(FDecalSceneProxy* Proxy)
{
	if (Proxy && Proxy->GetScene() == this)
	{
		Proxy->bDirty = true;
	}
}

void FScene::UpdateAllTransforms()
{
	for (FPrimitiveSceneProxy* Proxy : RenderStateDirtyProxies)
	{
		Proxy->UpdateRenderState();
		Proxy->bRenderStateQueued = false;
	}
	RenderStateDirtyProxies.Reset();

	for (FPrimitiveSceneProxy* Proxy : DirtyProxies)
	{
		Proxy->UpdateTransform();
		PrimitiveBounds[Proxy->PackedIndex] = Proxy->GetBounds();
		PrimitiveFlags[Proxy->PackedIndex] = Proxy->GetComponent()->IsVisible() ? 1 : 0;
		Proxy->bQueuedForUpdate = false;
	}

	if (bLightColorsLinear != ColorWorkflow::bLinear)
	{
		bLightColorsLinear = ColorWorkflow::bLinear;
		for (FLightSceneProxy* Proxy : Lights)
			MarkLightDirty(Proxy);
	}

	for (FLightSceneProxy* Proxy : DirtyLights)
	{
		Proxy->UpdateFromComponent();
		Proxy->bQueuedForUpdate = false;
	}
	DirtyLights.Reset();

	for (FDecalSceneProxy* Proxy : Decals)
	{
		if (Proxy->bDirty)
		{
			Proxy->Refresh();
		}
	}

	const bool bAnyMoved = DirtyProxies.Num() > 0;
	DirtyProxies.Reset();

	if (bElementListChanged)
	{
		BuildBVH();
		bElementListChanged = false;
	}
	else if (bAnyMoved)
	{
		BVH.Refit();
	}
}

void FScene::BuildBVH()
{
	BVH.Clear();
	// BillboardComponents.Reset();

	TArray<FPrimitiveSceneProxy*> Elements;
	Elements.Reserve(Proxies.Num());
	for (FPrimitiveSceneProxy* Proxy : Proxies)
	{
		if (Proxy && Proxy->GetComponent())
			Elements.Add(Proxy);
	}

	BVH.Build(std::span(Elements.GetData(), Elements.Num()));
}

void FScene::MarkDirty(FPrimitiveSceneProxy* Proxy)
{
	MarkShadowDirty();
	if (Proxy->bQueuedForUpdate) return;            // 중복 추가 방지
	Proxy->bQueuedForUpdate = true;
	DirtyProxies.Add(Proxy);
}

void FScene::MarkRenderStateDirty(FPrimitiveSceneProxy* Proxy)
{
	MarkShadowDirty();
	if (!Proxy || Proxy->bRenderStateQueued) return;
	Proxy->bRenderStateQueued = true;
	RenderStateDirtyProxies.Add(Proxy);
}

bool FScene::IsFrustemCulling(const FLightSceneProxy* Proxy, const FFrustumPlanes* FrustemPlanes) const
{
	if (FrustemPlanes == nullptr) return true;
	
	switch (Proxy->GetLightType())
	{
		case ELightType::Ambient:
		case ELightType::Directional:
			return true;
		case ELightType::Point:
		case ELightType::Spot:
		{
			FAABB Bounds;
			if (!TryGetLightBounds(Proxy, Bounds)) return true;
			if (FrustemPlanes == nullptr) return true;
			return IsAABBInFrustum(Bounds, *FrustemPlanes);
		}
		default:
			return true;
	}

	return false;
}

bool FScene::TryGetLightBounds(const FLightSceneProxy* Proxy, FAABB& OutBounds) const
{
	switch (Proxy->GetLightType())
	{
	case ELightType::Point:
	{
		const FPointLightSceneProxy* PointProxy = static_cast<const FPointLightSceneProxy*>(Proxy);
		FPointLightInfo PointLightInfo = PointProxy->GetInfo();
		FVector Extent = FVector{ PointLightInfo.AttenuationRadius, PointLightInfo.AttenuationRadius, PointLightInfo.AttenuationRadius };
		OutBounds = { PointLightInfo.Position, Extent };
		return true;
	}
	case ELightType::Spot:
	{
		const FSpotLightSceneProxy* SpotProxy = static_cast<const FSpotLightSceneProxy*>(Proxy);
		FSpotLightInfo SpotLightInfo = SpotProxy->GetInfo();
		FVector Extent = FVector{ SpotLightInfo.AttenuationRadius, SpotLightInfo.AttenuationRadius, SpotLightInfo.AttenuationRadius };
		OutBounds = { SpotLightInfo.Position, Extent };
		return true;
	}
	default:
		return false;
	}
}


bool FScene::HasAnyShadowCaster(const FLightSceneProxy* LightProxy) const
{
	FAABB Bounds;
	if (TryGetLightBounds(LightProxy, Bounds)) {
		// Proxies(Object) 와 LightProxy(Light) AABB 에 겹치는 Caster가 있는 경우에만 true
		for (auto CasterProxy : Proxies) {
			if (!CasterProxy || !CasterProxy->IsVisible() || !CasterProxy->GetRenderState() || !CasterProxy->IsCastShadow()) {
				continue;
			}

			FAABB CasterBounds = CasterProxy->GetBounds();
			if (abs(Bounds.Center.X - CasterBounds.Center.X) <= Bounds.Extent.X + CasterBounds.Extent.X
				&& abs(Bounds.Center.Y - CasterBounds.Center.Y) <= Bounds.Extent.Y + CasterBounds.Extent.Y
				&& abs(Bounds.Center.Z - CasterBounds.Center.Z) <= Bounds.Extent.Z + CasterBounds.Extent.Z) return true;
		}

		return false;
	}
	
	// directional light : 범위 무한이므로 씬 전체 검사 필요
	for (auto CasterProxy : Proxies) {
		if (CasterProxy && CasterProxy->IsVisible() && CasterProxy->GetRenderState() && CasterProxy->IsCastShadow()) {
			return true;
		}
	}


	return false;
}

bool FScene::HasAnyShadowReceiver(const FLightSceneProxy* LightProxy, const FFrustumPlanes* FrustemPlanes) const
{
	if (FrustemPlanes == nullptr) return true;
	FAABB Bounds;
	if (TryGetLightBounds(LightProxy, Bounds)) {
		// Proxies(Object) 와 LightProxy(Light) AABB 에 겹치는 Caster가 있는 경우에만 true
		for (auto CasterProxy : Proxies) {
			
			if (!CasterProxy || !CasterProxy->IsVisible() || !CasterProxy->GetRenderState() || !IsAABBInFrustum(CasterProxy->GetBounds(), *FrustemPlanes)) {
				continue;
			}
			FAABB CasterBounds = CasterProxy->GetBounds();
			if (abs(Bounds.Center.X - CasterBounds.Center.X) <= Bounds.Extent.X + CasterBounds.Extent.X
				&& abs(Bounds.Center.Y - CasterBounds.Center.Y) <= Bounds.Extent.Y + CasterBounds.Extent.Y
				&& abs(Bounds.Center.Z - CasterBounds.Center.Z) <= Bounds.Extent.Z + CasterBounds.Extent.Z) return true;
		}

		return false;
	}

	// directional light : 범위 무한이므로 씬 전체 검사 필요
	for (auto CasterProxy : Proxies) {
		if (CasterProxy && CasterProxy->IsVisible() && CasterProxy->GetRenderState()) {
			return true;
		}
	}


	return false;
}

bool FScene::IsDrawSize(const FLightSceneProxy* Proxy, const FVector& cameraPos) const
{
	FAABB Bounds;
	if (!TryGetLightBounds(Proxy, Bounds)) return true;	//directional light
	float dir = (cameraPos - Bounds.Center).Length();
	if (dir <= KINDA_SMALL_NUMBER) return true;	// 코앞에 있다! 이런경우 나눌때 큰일남 
	float ratio = Bounds.Extent.X / dir;

	if (ratio >= 0.03) return true;	// 언리얼 기본값
	else return false;
}

void FScene::BuildStructuredLightAndShadowData(TArray<FPointLightInfo>& OutPointLight, TArray<FSpotLightInfo>& OutSpotLight,
	TArray<FDirectionalLightInfo>& OutDirectionalLight, TArray<FShadowInfo>& OutShadowInfo, FLightSceneProxy*& OutPreviewShadowMapLightProxy, 
	FShadowStats & ShadowStats, bool bShadow, const FFrustumPlanes* FrustemPlanes, const FVector& CameraPos) const
{
	OutPointLight.Reset();
	OutSpotLight.Reset();
	OutShadowInfo.Reset();

	for (FLightSceneProxy* Proxy : Lights)
	{
		// 안 보이거나 그림자가 없으면 -1로 남는다
		Proxy->ShadowIndex = -1;

		if (!Proxy->IsVisible())
		{
			continue;
		}

		// Preview가 켜져있다면 넣기 (Directional, Spot 등)
		if (Proxy->bPreviewShadowMap)
		{
			OutPreviewShadowMapLightProxy = Proxy;
			Proxy->bPreviewShadowMap = false;
		}

		switch (Proxy->GetLightType())
		{
		case ELightType::Directional:
		{
			FDirectionalLightSceneProxy* DirectionalProxy = static_cast<FDirectionalLightSceneProxy*>(Proxy);
			FDirectionalLightInfo DirectionalLightInfo = DirectionalProxy->GetInfo();
			FShadowInfo DirectionalShadowInfo = DirectionalProxy->GetShadowInfo();

			if (!(bShadow  && DirectionalProxy->ShouldCastShadow() && IsFrustemCulling(DirectionalProxy, FrustemPlanes) 
				&& HasAnyShadowCaster(Proxy) && IsDrawSize(Proxy, CameraPos) && HasAnyShadowReceiver(Proxy, FrustemPlanes))) {
				ShadowStats.NumSkippedLights++;
			}

			ShadowStats.DirectionalLightCount++;
			OutDirectionalLight.Add(DirectionalLightInfo);
			break;
		}
		case ELightType::Point:
		{
			FPointLightSceneProxy* PointProxy = static_cast<FPointLightSceneProxy*>(Proxy);
			FPointLightInfo PointLightInfo = PointProxy->GetInfo();
			const TArray<FShadowInfo>& PointShadowInfos = PointProxy->GetShadowInfos();

			if (bShadow && PointProxy->ShouldCastShadow() && IsFrustemCulling(PointProxy, FrustemPlanes) 
				&& HasAnyShadowCaster(Proxy) && IsDrawSize(Proxy, CameraPos) && HasAnyShadowReceiver(Proxy, FrustemPlanes)) {
				PointLightInfo.ShadowIndex = OutShadowInfo.Num();
				Proxy->ShadowIndex = PointLightInfo.ShadowIndex;
				OutShadowInfo.Append(PointShadowInfos);
			}
			else {
				ShadowStats.NumSkippedLights++;
			}

			OutPointLight.Add(PointLightInfo);
			ShadowStats.PointLightCount++;
			break;
		}
		case ELightType::Spot:
		{
			FSpotLightSceneProxy* SpotProxy = static_cast<FSpotLightSceneProxy*>(Proxy);
			FSpotLightInfo SpotLightInfo = SpotProxy->GetInfo();
			if (bShadow && SpotProxy->ShouldCastShadow() && IsFrustemCulling(SpotProxy, FrustemPlanes) 
				&& HasAnyShadowCaster(Proxy) && IsDrawSize(Proxy, CameraPos) && HasAnyShadowReceiver(Proxy, FrustemPlanes)) {
				FShadowInfo SpotShadowInfo = SpotProxy->GetShadowInfo();
				SpotLightInfo.ShadowIndex = OutShadowInfo.Add(SpotShadowInfo);
				Proxy->ShadowIndex = SpotLightInfo.ShadowIndex;
			}
			else {
				ShadowStats.NumSkippedLights++;
			}
			OutSpotLight.Add(SpotLightInfo);
			ShadowStats.SpotLightCount++;
			break;
		}			
		default:
		{
			break;
		}
		}
	}
}

