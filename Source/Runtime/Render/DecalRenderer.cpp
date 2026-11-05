#include "EnginePCH.h"

#include "DecalRenderer.h"
#include "Engine/DecalSceneProxy.h"
#include "Render/Material.h"
#include "RenderResourceManager.h"
#include "RenderCommand.h"
#include "Asset/AssetManager.h"
#include "Render/ColorWorkflow.h"
#include "Component/DecalComponent.h"
#include "Render/Texture2D.h"
#include "Math/Frustum.h"
#include "Core/EngineTimer.h"
#include <algorithm>

bool FDecalRenderer::Init()
{
	// TODO: SV_VertexID 기반 박스 생성으로 전용 VB/IB 대체 검토

	// 중심 원점 및 한 변 길이 1의 박스 위치
	const FVector Vertices[] =
	{
		FVector(-0.5f, -0.5f, -0.5f),
		FVector(-0.5f,  0.5f, -0.5f),
		FVector(0.5f,  0.5f, -0.5f),
		FVector(0.5f, -0.5f, -0.5f),
		FVector(-0.5f, -0.5f,  0.5f),
		FVector(-0.5f,  0.5f,  0.5f),
		FVector(0.5f,  0.5f,  0.5f),
		FVector(0.5f, -0.5f,  0.5f)
	};

	// 기존 Cube와 동일한 바깥 방향의 삼각형 연결
	const uint32 Indices[] =
	{
		0, 1, 2, 0, 2, 3, // -Z 면
		7, 6, 5, 7, 5, 4, // +Z 면
		1, 5, 6, 1, 6, 2, // +Y 면
		4, 0, 3, 4, 3, 7, // -Y 면
		4, 5, 1, 4, 1, 0, // -X 면
		3, 2, 6, 3, 6, 7  // +X 면
	};

	VolumeVertexBuffer = RenderCommand::CreateStaticVertexBuffer(Vertices, sizeof(Vertices), sizeof(FVector));
	VolumeIndexBuffer = RenderCommand::CreateStaticIndexBuffer(Indices, static_cast<uint32>(sizeof(Indices) / sizeof(Indices[0])));
	ConstantBuffer = RenderCommand::CreateConstantBuffer(sizeof(FDecalConstants));

	// 실제 GPU 자원의 초기화 결과 확인
	if (!VolumeVertexBuffer->GetBuffer() || !VolumeIndexBuffer->GetBuffer() || !ConstantBuffer->GetBuffer())
	{
		VolumeVertexBuffer.reset();
		VolumeIndexBuffer.reset();
		ConstantBuffer.reset();
		HTR_LOG(Error, "[Decal] Failed to create volume buffers");
		return false;
	}


	PipelineState.Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/DecalShader.hlsl");
	PipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	PipelineState.RasterizerState = ERasterizerState::DecalCullFront;
	PipelineState.BlendState = EBlendState::AlphaBlend;
	PipelineState.DepthStencilState = EDepthStencilState::Disabled;

	return true;
}

// 가시 데칼 볼륨의 DBuffer 제출 및 횟수 집계
void FDecalRenderer::Render(const TArray<FDecalSceneProxy*>& InDecalProxies, const FMatrix& ViewProjection,
	FTexture2D* InDepthTexture, FTexture2D* InObjectMaskTexture, FTexture2D* InNormalTexture, uint32& OutDrawCount, bool bGameView)
{
	OutDrawCount = 0;

	if (!VolumeVertexBuffer)
	{
		return;
	}

	// 현재 뷰에 영향을 주는 데칼 선별
	VisibleDecals.Reset();
	const FFrustumPlanes Frustum = ExtractFrustumPlanes(ViewProjection);
	for (const FDecalSceneProxy* Proxy : InDecalProxies)
	{
		if (!Proxy->IsVisible() || (bGameView && Proxy->GetComponent()->IsHiddenInGame())
			|| !IsAABBInFrustum(MakeWorldBounds(Proxy->GetBounds()), Frustum))
		{
			continue;
		}
		VisibleDecals.Add(Proxy);
	}
	if (VisibleDecals.IsEmpty())
	{
		return;
	}

	// 합성 순서와 동일 순위의 결정적 정렬
	std::sort(VisibleDecals.begin(), VisibleDecals.end(),
		[](const FDecalSceneProxy* Left, const FDecalSceneProxy* Right)
		{
			if (Left->GetSortOrder() != Right->GetSortOrder())
			{
				return Left->GetSortOrder() < Right->GetSortOrder();
			}
			return Left->GetId() < Right->GetId();
		});

	// 뷰에서 공유하는 기존 기본 리소스와 시간
	UTexture2D* WhiteTexture = UAssetManager::GetAssetByPath<UTexture2D>("WhiteTexture");
	UTexture2D* FlatNormalTexture = UAssetManager::GetAssetByPath<UTexture2D>("FlatNormalTexture");
	const float TotalTime = EngineTimer::GetTotalTime();

	FDecalConstants DecalConstants{};
	DecalConstants.InvViewProjection = ViewProjection.Inverse();
	DecalConstants.InvTargetSize = FVector2(
		1.0f / static_cast<float>(InDepthTexture->GetWidth()),
		1.0f / static_cast<float>(InDepthTexture->GetHeight()));

	RenderCommand::BindPipelineState(PipelineState);
	RenderCommand::BindConstantBuffer(0, ConstantBuffer.get(),
		EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
	RenderCommand::BindShaderResource(0, InDepthTexture, EShaderBindFlagBits::Pixel);
	RenderCommand::BindShaderResource(1, InObjectMaskTexture, EShaderBindFlagBits::Pixel);

	// 투영 TBN 기준의 프리패스 기하 노멀 입력
	RenderCommand::BindShaderResource(4, InNormalTexture, EShaderBindFlagBits::Pixel);

	// 모든 데칼이 공유하는 투영 전용 박스 연결
	RenderCommand::BindVertexBuffer(VolumeVertexBuffer.get());
	RenderCommand::BindIndexBuffer(VolumeIndexBuffer.get());
	const uint32 IndexCount = VolumeIndexBuffer->GetIndexCount();

	for (const FDecalSceneProxy* DecalProxy : VisibleDecals)
	{
		UMaterial* DecalMaterial = DecalProxy->GetMaterial();
		if (!DecalMaterial)
		{
			continue;
		}

		// 미지정 및 기본 중립 노멀의 기여 제외
		UTexture2D* NormalMap = DecalMaterial->Textures.Num() > 1
			? DecalMaterial->Textures[1] : nullptr;
		const bool bAffectNormal = DecalMaterial->bAffectNormal
			&& NormalMap != nullptr && NormalMap != FlatNormalTexture;
		if (!DecalMaterial->bAffectBaseColor && !bAffectNormal)
		{
			continue;
		}

		// 기존 데칼 색상과 머티리얼 색상의 조명 전 결합
		const FLinearColor DecalColor = ColorWorkflow::ToShaderColor(DecalProxy->GetColor());
		const FLinearColor BaseColor = ColorWorkflow::ToShaderColor(DecalMaterial->BaseColor);
		DecalConstants.InvWorld = DecalProxy->GetWorldToDecal();
		DecalConstants.WorldViewProjection = DecalProxy->GetDecalToWorld() * ViewProjection;
		DecalConstants.Tint = FLinearColor(DecalColor.R * BaseColor.R, DecalColor.G * BaseColor.G,
			DecalColor.B * BaseColor.B, DecalColor.A * BaseColor.A * DecalProxy->GetFadeAlpha());

		// 기존 머티리얼 설정의 데칼 셰이더 전달
		DecalConstants.UVScrollOffset = DecalMaterial->UVScrollSpeed * TotalTime;
		DecalConstants.bAffectBaseColor = DecalMaterial->bAffectBaseColor ? 1u : 0u;
		DecalConstants.bAffectNormal = bAffectNormal ? 1u : 0u;
		DecalConstants.bFlipNormalGreen = DecalMaterial->bFlipNormalGreen ? 1u : 0u;
		RenderCommand::UpdateBufferData(ConstantBuffer.get(), &DecalConstants);

		// 미지정 색상 텍스처의 기존 흰색 리소스 대체
		UTexture2D* Albedo = DecalMaterial->Textures.Num() > 0 && DecalMaterial->Textures[0]
			? DecalMaterial->Textures[0] : WhiteTexture;
		RenderCommand::BindShaderResource(2, Albedo, EShaderBindFlagBits::Pixel);

		// 색상 변환을 적용하지 않는 원시 노멀 데이터 입력
		RenderCommand::BindShaderResource(3, NormalMap ? NormalMap : FlatNormalTexture,
			EShaderBindFlagBits::Pixel, true);
		RenderCommand::BindSamplerState(0, DecalMaterial->SamplerState, EShaderBindFlagBits::Pixel);

		// 음수 스케일에 따른 볼륨 면 방향 반전 보정
		const bool bMirrored = DecalProxy->GetDecalToWorld().Determinant() < 0.0f;
		RenderCommand::SetRasterizerState(
			bMirrored ? ERasterizerState::DecalCullBack : ERasterizerState::DecalCullFront);
		RenderCommand::DrawIndexed(IndexCount);
		++OutDrawCount;
	}

	// 다음 패스의 입력·출력 자원 충돌 방지
	for (uint32 Slot = 0; Slot <= 4; ++Slot)
	{
		RenderCommand::UnbindShaderResource(Slot, EShaderBindFlagBits::Pixel);
	}

	// 후속 렌더링의 기본 상태 복원
	RenderCommand::SetRasterizerState(ERasterizerState::SolidBack);
	RenderCommand::SetBlendState(EBlendState::Opaque);
	RenderCommand::SetDepthStencilState(EDepthStencilState::Default);
}
