#include "EnginePCH.h"
#include "LightComponentBase.h"
#include "Engine/LightSceneProxy.h"
#include "Engine/Scene.h"

void ULightComponentBase::MarkSceneProxyDirty()
{
	if (SceneProxy)
		SceneProxy->GetScene()->MarkLightDirty(SceneProxy);
}

void ULightComponentBase::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
	
	if (Intensity < 0.0f)
		Intensity = 0.0f;

	MarkSceneProxyDirty();
}

void ULightComponentBase::OnTransformDirty()
{
	Super::OnTransformDirty();
	MarkSceneProxyDirty();
}
