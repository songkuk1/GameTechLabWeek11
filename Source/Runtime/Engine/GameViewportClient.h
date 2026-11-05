#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "UI/WidgetDrawElements.h"
#include "Render/Buffer.h"
#include "Render/PipelineState.h"
#include "Math/Matrix.h"
#include "Math/Vector2.h"

class FWidgetTree;
class FTextRenderer;
struct FRenderingInfo;

class UGameViewportClient : public UObject
{
	DECLARE_CLASS(UGameViewportClient, UObject)

public:
	UGameViewportClient();

	void AddToViewport(FWidgetTree* InWidgetTree);

	FWidgetTree* GetWidgetTree() const;

	// 마지막으로 그린 게임 뷰포트 크기 (PIE면 PIE 뷰 크기). 아직 그린 적 없으면 (0, 0)
	FVector2 GetViewportSize() const { return ViewportSize; }

	void CollectDrawList(float ScreenWidth, float ScreenHeight);

	void Draw(float ScreenWidth, float ScreenHeight, FTextRenderer& TextRenderer);

private:
	void DrawBoxElement(const FWidgetBoxElement& BoxElement);

	void DrawTextElement(const FWidgetTextElement& TextElement, const FMatrix& VP, FTextRenderer& TextRenderer);

private:
	struct FViewConstants
	{
		FMatrix VP;
	};

	struct FQuadParmas
	{
		FMatrix World;
		FLinearColor Color;
		int32 HasTexture;
		int32 Padding[3];
	};

	FWidgetTree* WidgetTree = nullptr;
	FWidgetWindowElementList DrawList;

	FVector2 ViewportSize = FVector2(0.0f, 0.0f);

	FPipelineState QuadPipeline;

	FVertexBuffer* QuadVertexBuffer;
	FIndexBuffer* QuadIndicesBuffer;

	TUniquePtr<FConstantBuffer> ViewConstantsBuffer;
	TUniquePtr<FConstantBuffer> QuadParamsConstantsBuffer;
};
