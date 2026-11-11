#pragma once

#include "Engine.h"
#include "Render/SceneRenderer.h"
#include "Render/SceneRenderTargets.h"
#include "Text/TextRenderer.h"
#include "GameEngine.generated.h"

class FTextRenderer;

UCLASS()
class UGameEngine : public UEngine
{
	GENERATED_BODY()

public:
	FEngineConfig GetConfig() const override;

	bool Init() override;
	void Tick(float DeltaTime) override;
	void OnResize(uint32 Width, uint32 Height) override;
	void PreExit() override;

private:
	void HandleOnPIEWorldChanged(UWorld* OldWorld, UWorld* NewWorld);

private:
	UWorld* World = nullptr;

	FRenderQueue RenderQueue;
	FSceneRenderer SceneRenderer;
	FTextRenderer TextRenderer;
	FSceneRenderTargets SceneRenderTargets;
};
