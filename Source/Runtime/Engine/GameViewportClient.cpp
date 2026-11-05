#include "EnginePCH.h"
#include "GameViewportClient.h"
#include "UI/WidgetTree.h"
#include "UI/Widget.h"
#include "Asset/AssetManager.h"
#include "Render/Mesh.h"
#include "Render/RenderCommand.h"
#include "Render/RenderResourceManager.h"
#include "Render/RenderingInfo.h"
#include "Text/TextRenderer.h"

UGameViewportClient::UGameViewportClient()
{
	UStaticMesh* QuadMesh = UAssetManager::GetAssetByPath<UStaticMesh>("Quad");
	QuadVertexBuffer = QuadMesh->GetVertexBuffer(0);
	QuadIndicesBuffer = QuadMesh->GetIndexBuffer(0);

	QuadPipeline.Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/QuadShader.hlsl");
	QuadPipeline.DepthStencilState = EDepthStencilState::Disabled;
	QuadPipeline.RasterizerState = ERasterizerState::SolidNone;
	QuadPipeline.BlendState = EBlendState::AlphaBlend;

	ViewConstantsBuffer = RenderCommand::CreateConstantBuffer(sizeof(FViewConstants));
	QuadParamsConstantsBuffer = RenderCommand::CreateConstantBuffer(sizeof(FQuadParmas));
}

void UGameViewportClient::AddToViewport(FWidgetTree* InWidgetTree)
{
	DrawList.TextElements.Reset();
	DrawList.BoxElements.Reset();
	DrawList.Batched.Reset();

	WidgetTree = InWidgetTree;
}

FWidgetTree* UGameViewportClient::GetWidgetTree() const
{
	return WidgetTree;
}

void UGameViewportClient::CollectDrawList(float ScreenWidth, float ScreenHeight)
{
	ViewportSize = FVector2(ScreenWidth, ScreenHeight);

	if (!WidgetTree)
	{
		return;
	}

	DrawList.TextElements.Reset();
	DrawList.BoxElements.Reset();
	DrawList.Batched.Reset();

	FWidgetRect ScreenRect;
	ScreenRect.Min = FVector2(0.f, 0.f);
	ScreenRect.Max = FVector2(ScreenWidth, ScreenHeight);

	WidgetTree->GetRootWidget()->OnPaint(ScreenRect, DrawList);
}

void UGameViewportClient::Draw(float ScreenWidth, float ScreenHeight, FTextRenderer& TextRenderer)
{
	if (!DrawList.Batched.Num())
	{
		return;
	}

	FMatrix VP = Ortho(0, ScreenWidth, 0, ScreenHeight, 0.f, 1.f);

	FViewConstants ViewConstant;
	ViewConstant.VP = VP;

	RenderCommand::UpdateBufferData(ViewConstantsBuffer.get(), &ViewConstant, sizeof(FViewConstants));

	for (FWidgetElementInfo& Info: DrawList.Batched)
	{
		if (Info.Type == EWidgetDrawElementType::Box)
		{
			DrawBoxElement(DrawList.BoxElements[Info.Index]);
		}
		else if (Info.Type == EWidgetDrawElementType::Text)
		{
			DrawTextElement(DrawList.TextElements[Info.Index], VP, TextRenderer);
		}
	}
}

void UGameViewportClient::DrawBoxElement(const FWidgetBoxElement& BoxElement)
{
	FVector Scale = FVector(1.0, BoxElement.Rect.Width(), BoxElement.Rect.Height());
	FVector Location = FVector(0.f, BoxElement.Rect.Min.X + Scale.Y * 0.5f, BoxElement.Rect.Min.Y + Scale.Z * 0.5f);

	FQuadParmas Params;
	Params.World = FMatrix(
		Scale.X, 0.f, 0.f, 0.f,
		0.f, Scale.Y, 0.f, 0.f,
		0.f, 0.f, -Scale.Z, 0.f,
		0, Location.Y, Location.Z, 1.0f
	);
	Params.Color = BoxElement.Color;
	Params.HasTexture = BoxElement.Texture != nullptr;

	RenderCommand::UpdateBufferData(QuadParamsConstantsBuffer.get(), &Params, sizeof(FQuadParmas));

	RenderCommand::BindPipelineState(QuadPipeline);

	if (BoxElement.Texture)
	{
		RenderCommand::BindShaderResource(0, BoxElement.Texture, EShaderBindFlagBits::Pixel);
		RenderCommand::BindSamplerState(0, ESamplerState::LinearClamp, EShaderBindFlagBits::Pixel);
	}

	RenderCommand::BindConstantBuffer(0, ViewConstantsBuffer.get(), EShaderBindFlagBits::Vertex);
	RenderCommand::BindConstantBuffer(1, QuadParamsConstantsBuffer.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);

	RenderCommand::BindVertexBuffer(QuadVertexBuffer);
	RenderCommand::BindIndexBuffer(QuadIndicesBuffer);
	RenderCommand::DrawIndexed(QuadIndicesBuffer->GetIndexCount());
}

void UGameViewportClient::DrawTextElement(const FWidgetTextElement& TextElement, const FMatrix& VP, FTextRenderer& TextRenderer)
{
	float OutMinX, OutMaxX, OutMinY, OutMaxY;
	if (!TextRenderer.ComputeTextBounds(TextElement.Text, 1.f, *TextElement.Atlas, OutMinX, OutMaxX, OutMinY, OutMaxY))
	{
		return;
	}

	float TextWidth = OutMaxX - OutMinX;
	float TextHeight = OutMaxY - OutMinY;

	float RatioW = TextElement.Rect.Width() / TextWidth;
	float RatioH = TextElement.Rect.Height() / TextHeight;
	float MinRatio = FMath::Min(RatioW, RatioH);

	FVector Location = FVector(0.f, TextElement.Rect.Min.X - OutMinX * MinRatio, TextElement.Rect.Min.Y + OutMaxY * MinRatio);

	FMatrix WorldMatrix(
		1.f, 0.f, 0.f, 0.f,
		0.f, MinRatio, 0.f, 0.f,
		0.f, 0.f, -MinRatio, 0.f,
		0, Location.Y, Location.Z, 1.0f
	);

	TextRenderer.OnRender(TextElement.Text, WorldMatrix, 1.0f, *TextElement.Atlas, VP, TextElement.Color);
}
