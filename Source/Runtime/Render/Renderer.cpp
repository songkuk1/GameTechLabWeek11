#include "EnginePCH.h"
#include "Renderer.h"
#include "Shader.h"
#include "Mesh.h"

#include "Core/EngineTimer.h"
#include "Core/Stats/LightweightStats.h"

#include "Engine/PrimitiveSceneProxy.h"

#include "RenderCommand.h"
#include "RenderResourceManager.h"
#include "ColorWorkflow.h"
#include "Asset/ModelData.h"

#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <bit>

#include "Engine/LightSceneProxy.h"
#include <Core/StatOverlay.h>

DECLARE_CYCLE_STAT("Draw Render Packets", STAT_DrawRenderPackets);
DECLARE_CYCLE_STAT("Render Queue Sorting", STAT_RenderQueueSorting);
DECLARE_CYCLE_STAT("Upload Per-Object CB", STAT_UploadPerObjectCB);
DECLARE_CYCLE_STAT("Shadow Pass (Total)", STAT_Shadow);

namespace
{
	// VSSetConstantBuffers1의 오프셋은 16개 상수(256바이트) 단위여야 하므로 오브젝트마다 256바이트 칸을 쓴다.
	constexpr uint32 PerObjectSlotConstants = 16;

	constexpr uint32 MinPerObjectSlots = 1024;

	const FMatrix& GetPacketWorld(const FRenderPacket& Packet)
	{
		if (Packet.Proxy) return Packet.Proxy->GetLocalToWorld();
		return Packet.Model ? *Packet.Model : FMatrix::Identity;
	}

	FMatrix GetPacketNormalMatrix(const FRenderPacket& Packet)
	{
		if (Packet.Proxy) return Packet.Proxy->GetNormalMatrix();
		return MakeNormalMatrix(GetPacketWorld(Packet));
	}
}

bool FRenderer::Init()
{
	bUsePerObjectSlots = RenderCommand::SupportsConstantBufferOffsets();
	PerObjectCB = RenderCommand::CreateConstantBuffer(sizeof(FPerObjectConstants));
	ViewCB = RenderCommand::CreateConstantBuffer(sizeof(FViewConstants));
	ShadowViewCB = RenderCommand::CreateConstantBuffer(sizeof(FMatrix));
	LightingCB = RenderCommand::CreateConstantBuffer(sizeof(FLightingConstants));
	FLightingConstants LightingConstant{};
	RenderCommand::UpdateBufferData(LightingCB.get(), &LightingConstant);

	FShaderProgramDesc SurfaceStaticSPDesc;
	SurfaceStaticSPDesc.Path = "Resources/Shader/SurfaceShader.hlsl";
	SurfaceStaticSPDesc.Defines = {
		{ "USE_SKINNING", "0" }
	};
	SurfaceStaticSPDesc.InputLayout = {
		{ "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, static_cast<UINT>(offsetof(FVertexPNCT, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, static_cast<UINT>(offsetof(FVertexPNCT, Normal)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT",      0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(FVertexPNCT, Tangent)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",        0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(FVertexPNCT, Color)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, static_cast<UINT>(offsetof(FVertexPNCT, UV)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	SurfaceStaticPipeline.Shader = FRenderResourceManager::RegisterProgram("Surface_Static", SurfaceStaticSPDesc);

	FShaderProgramDesc SurfaceSkinningSPDesc;
	SurfaceSkinningSPDesc.Path = "Resources/Shader/SurfaceShader.hlsl";

	SurfaceSkinningSPDesc.Defines = { 
		{ "USE_SKINNING", "1" } 
	};
	SurfaceSkinningSPDesc.InputLayout = {
		{ "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, static_cast<UINT>(offsetof(FVertexPNCT, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, static_cast<UINT>(offsetof(FVertexPNCT, Normal)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT",      0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(FVertexPNCT, Tangent)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",        0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(FVertexPNCT, Color)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, static_cast<UINT>(offsetof(FVertexPNCT, UV)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_SINT,  1, static_cast<UINT>(offsetof(FModelVertexInfluences, BoneIndices)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, static_cast<UINT>(offsetof(FModelVertexInfluences, Weights)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BONECOUNT",	  0, DXGI_FORMAT_R32_SINT,			 1, static_cast<UINT>(offsetof(FModelVertexInfluences, InfluenceCount)), D3D11_INPUT_PER_VERTEX_DATA, 0 }

	};
	SurfaceSkinningPipeline.Shader = FRenderResourceManager::RegisterProgram("Surface_Skinned", SurfaceSkinningSPDesc);

	TileLightCulling.Init();
	GPUOcclusion.Init();   // 실패해도 오클루전만 못 쓸 뿐 렌더링은 된다

	return true;
}

// 필요한 칸 수가 용량을 넘을 때만 두 배씩 키워 재할당을 드물게 한다.
void FRenderer::EnsurePerObjectSlotCapacity(uint32 SlotCount)
{
	if (SlotCount <= PerObjectSlotCapacity)
		return;

	uint32 NewCapacity = std::max(PerObjectSlotCapacity * 2, MinPerObjectSlots);
	while (NewCapacity < SlotCount)
		NewCapacity *= 2;

	PerObjectSlotCB = RenderCommand::CreateConstantBuffer(NewCapacity * ObjectSlotBytes);
	if (!PerObjectSlotCB || !PerObjectSlotCB->GetBuffer())
	{
		HTR_LOG(Warning, "Per-object constant buffer ({} slots) creation failed. Falling back to per-draw updates.", NewCapacity);
		PerObjectSlotCB.reset();
		PerObjectSlotCapacity = 0;
		return;
	}
	PerObjectSlotCapacity = NewCapacity;
}

// 원래 패킷 순서로 World·Normal 행렬을 올린다. 정렬 목록은 원래 패킷 번호의 칸을 바인딩한다.
void FRenderer::UploadPerObjectConstants()
{
	SCOPE_CYCLE_COUNTER(STAT_UploadPerObjectCB);

	const uint32 Count = static_cast<uint32>(RenderPackets.size());
	if (!bUsePerObjectSlots || Count == 0)
		return;

	EnsurePerObjectSlotCapacity(Count);
	if (!PerObjectSlotCB)
	{
		bUsePerObjectSlots = false;
		return;
	}

	uint8* Dest = static_cast<uint8*>(RenderCommand::MapWriteDiscard(PerObjectSlotCB.get()));
	if (!Dest)
	{
		bUsePerObjectSlots = false;
		return;
	}

	// 매핑된 메모리는 write-combined라 순차 쓰기만 하고 읽지 않는다.
	for (uint32 Index = 0; Index < Count; ++Index)
	{
		FRenderPacket& P = RenderPackets[Index];
		const uint32 ObjectMask = P.Proxy ? P.Proxy->GetObjectMask() : 0u;
		WriteObjectSlot(Dest, Index, GetPacketWorld(P), GetPacketNormalMatrix(P), ObjectMask);
		P.Slot = Index;
	}

	RenderCommand::Unmap(PerObjectSlotCB.get());
}

// 불투명 우선·반투명 거리순으로 정렬해 View 행렬과 Section 범위로 그린다.
void FRenderer::RenderAll(FRenderQueue& InQueue, const FMatrix& ViewProjection, const FVector& CameraPosition)
{
	RenderQueueSorting(InQueue, ViewProjection, CameraPosition);
	RenderOpaque();
	RenderTranslucent();
}

void FRenderer::RenderSurface()
{
	SCOPE_CYCLE_COUNTER(STAT_DrawRenderPackets);
	
	RenderCommand::BindConstantBuffer(0, ViewCB.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);

	FPipelineState* LastPipelinePtr = nullptr;
	for (const FStaticDrawGroup* Group : StaticGroups)
	{
		RenderCommand::BindVertexBuffers(Group->Geometry.VertexBuffers, Group->Geometry.VertexBufferCount);
		RenderCommand::BindIndexBuffer(Group->Geometry.IndexBuffer);
		 
		for (const FStaticDrawItem& Item : Group->Items)
		{
			const bool bUseSkinning = Item.Proxy->IsUsingSkinning();
			FPipelineState* Pipeline = bUseSkinning ? &SurfaceSkinningPipeline : &SurfaceStaticPipeline;

			if (LastPipelinePtr != Pipeline)
			{
				RenderCommand::BindPipelineState(*Pipeline);
				LastPipelinePtr = Pipeline;
			}

			if (bUsePerObjectSlots && Item.Slot != InvalidObjectSlot)
			{
				RenderCommand::BindConstantBufferRange(1, PerObjectSlotCB.get(), Item.Slot * PerObjectSlotConstants, PerObjectSlotConstants, EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
			}
			else
			{
				RenderCommand::BindConstantBuffer(1, PerObjectCB.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
				UpdatePerObjectConstants(Item.Proxy->GetLocalToWorld(), Item.Proxy->GetNormalMatrix(), Item.Proxy->GetObjectMask());
			}

			if (bUseSkinning)
			{
				ID3D11ShaderResourceView* BoneMatricesSRV = Item.Proxy->GetBoneMatricesSRV();
				RenderCommand::GetContext()->VSSetShaderResources(0, 1, &BoneMatricesSRV);
			}

			RenderCommand::DrawIndexed(Item.IndexCount, Item.StartIndex);
		}
	}

	for (uint32 Index = 0; Index < FirstTranslucentIndex; ++Index)
	{
		const FRenderPacket& Packet = RenderPackets[Index];
		const bool bUseSkinning = Packet.Proxy && Packet.Proxy->IsUsingSkinning();
		
		FPipelineState* Pipeline = bUseSkinning ? &SurfaceSkinningPipeline : &SurfaceStaticPipeline;

		if (LastPipelinePtr != Pipeline)
		{
			RenderCommand::BindPipelineState(*Pipeline);
			LastPipelinePtr = Pipeline;
		}

		RenderCommand::BindVertexBuffers(Packet.Geometry.VertexBuffers, Packet.Geometry.VertexBufferCount);
		RenderCommand::BindIndexBuffer(Packet.Geometry.IndexBuffer);

		if (bUsePerObjectSlots && Packet.Slot != InvalidObjectSlot)
		{
			RenderCommand::BindConstantBufferRange(1, PerObjectSlotCB.get(), Packet.Slot * PerObjectSlotConstants, PerObjectSlotConstants, EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
		}
		else
		{
			RenderCommand::BindConstantBuffer(1, PerObjectCB.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
			UpdatePerObjectConstants(Packet);
		}

		if (bUseSkinning)
		{
			ID3D11ShaderResourceView* BoneMatricesSRV = Packet.Proxy->GetBoneMatricesSRV();
			RenderCommand::GetContext()->VSSetShaderResources(0, 1, &BoneMatricesSRV);
		}

		RenderCommand::DrawIndexed(Packet.Geometry.IndexCount, Packet.Geometry.StartIndex);
	}

	RenderCommand::UnbindShaderResource(0, EShaderBindFlagBits::Vertex);

	LastVertexBufferCount = 0;
	LastIndexBuffer = nullptr;
	LastMaterial = nullptr;
}

void FRenderer::RenderOpaque()
{
	DrawStaticGroups();
	DrawPackets(0, FirstTranslucentIndex);
}

// RenderOpaque가 남긴 반투명 패킷을 먼 것부터 그린다.
void FRenderer::RenderTranslucent()
{
	DrawPackets(FirstTranslucentIndex, RenderPackets.Num());
	RenderPackets.Reset();
	FirstTranslucentIndex = 0;
	StaticGroups.clear();   // 묶음 메모리는 World 것이므로 이번 프레임이 끝나면 놓는다
}

// 스태틱 메시 묶음을 정렬 키 순서로 그린다. 바인딩은 묶음마다 한 번, 항목마다는 칸 바인딩과 드로우만 한다.
void FRenderer::DrawStaticGroups()
{
	if (StaticGroups.empty())
		return;

	SCOPE_CYCLE_COUNTER(STAT_DrawRenderPackets);
	// 픽셀 단위 조명이 카메라 위치를 읽으므로 PS에도 바인딩한다.
	RenderCommand::BindConstantBuffer(0, ViewCB.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
	RenderCommand::BindConstantBuffer(3, LightingCB.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);

	UMaterial* BoundMaterial = nullptr;
	int8 LastReceiveShadow = -1;
	int8 LastUseSkinning = -1;

	for (const FStaticDrawGroup* Group : StaticGroups)
	{
		RenderCommand::BindVertexBuffers(Group->Geometry.VertexBuffers, Group->Geometry.VertexBufferCount);
		RenderCommand::BindIndexBuffer(Group->Geometry.IndexBuffer);

		// 한계: 같은 머티리얼인데 ReceiveShadow가 번갈아 다르면 셰이더를 매번 재바인딩하게 됨.
		// 제대로 하려면 정렬 키에 플래그를 넣어 같은 값끼리 모아야 하는데, 이번 마감 범위 밖입니다.
		for (const FStaticDrawItem& Item : Group->Items)
		{
			const bool bReceiveShadow = Item.Proxy ? Item.Proxy->IsReceiveShadow() : true;
			const bool bUseSkinning = Item.Proxy ? Item.Proxy->IsUsingSkinning() : false;
			const int8 CurrentRS = bReceiveShadow ? 1 : 0;
			const int8 CurrentSkinning = bUseSkinning ? 1 : 0;
			const bool bMaterialChanged = (Group->Material != BoundMaterial);
			const bool bShadowChanged = (CurrentRS != LastReceiveShadow);
			const bool bSkinningChanged = CurrentSkinning != LastUseSkinning;

			if (bMaterialChanged || bShadowChanged || bSkinningChanged)
			{
				BoundMaterial = Group->Material;
				LastReceiveShadow = CurrentRS;
				LastUseSkinning = CurrentSkinning;
				BindMaterial(BoundMaterial, bReceiveShadow, bUseSkinning);
				if (bMaterialChanged)
				{
					FRenderPacket MaterialOnly;           // 머티리얼 파라미터 갱신은 패킷을 받으므로 머티리얼만 채워 넘긴다
					MaterialOnly.Material = BoundMaterial;
					UpdateMaterialParams(MaterialOnly);
				}
			}

			if (bUsePerObjectSlots && Item.Slot != InvalidObjectSlot)
			{
				RenderCommand::BindConstantBufferRange(2, PerObjectSlotCB.get(), Item.Slot * PerObjectSlotConstants, PerObjectSlotConstants, EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
			}
			else
			{
				RenderCommand::BindConstantBuffer(2, PerObjectCB.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
				UpdatePerObjectConstants(Item.Proxy->GetLocalToWorld(), Item.Proxy->GetNormalMatrix(), Item.Proxy->GetObjectMask());
			}

			if (Item.Proxy)
			{
				Item.Proxy->BindSRVs();
			}

			RenderCommand::DrawIndexed(Item.IndexCount, Item.StartIndex);
		}
	}

	// 뒤따르는 DrawPackets가 처음부터 다시 바인딩하도록 기록을 비운다.
	LastVertexBufferCount = 0;
	LastIndexBuffer = nullptr;
	LastMaterial = nullptr;
}

void FRenderer::RenderQueueSorting(FRenderQueue& InQueue, const FMatrix& ViewProjection, const FVector& CameraPosition)
{
	FViewConstants ViewConstants;
	ViewConstants.ViewProjection = ViewProjection;
	ViewConstants.CameraPosition = CameraPosition;

	RenderCommand::UpdateBufferData(ViewCB.get(), &ViewConstants);
	{
		SCOPE_CYCLE_COUNTER(STAT_RenderQueueSorting);

		std::swap(RenderPackets, InQueue);
		InQueue.Reset();

		std::sort(RenderPackets.begin(), RenderPackets.end(), [](const FRenderPacket& A, const FRenderPacket& B) {
			if (A.Material->BlendState != B.Material->BlendState)
			{
				return A.Material->BlendState < B.Material->BlendState;
			}

			if (A.Material->BlendState != EBlendState::Opaque)
			{
				return A.CameraToParticleDistance > B.CameraToParticleDistance;
			}

			if (A.Material->SortID != B.Material->SortID)
			{
				return A.Material->SortID < B.Material->SortID;
			}

			if (A.Geometry.VertexBufferCount != B.Geometry.VertexBufferCount)
			{
				return A.Geometry.VertexBufferCount < B.Geometry.VertexBufferCount;
			}

			for (int32 i = 0; i < A.Geometry.VertexBufferCount; i++)
			{
				if (A.Geometry.VertexBuffers[i] != B.Geometry.VertexBuffers[i])
				{
					return A.Geometry.VertexBuffers[i] < B.Geometry.VertexBuffers[i];
				}
			}
			
			return A.Geometry.IndexBuffer < B.Geometry.IndexBuffer;
		});

		FirstTranslucentIndex = 0;
		for (int32 i = 0; i < RenderPackets.Num(); i++)
		{
			if (RenderPackets[i].Material->BlendState == EBlendState::AlphaBlend)
			{
				break;
			}
			++FirstTranslucentIndex;
		}

		// ④ 스태틱 메시 묶음: 빈 묶음을 빼고 키 순으로 정렬한다. 조각마다 같은 키 묶음이 있으므로 정렬하면 서로 붙는다.
		std::erase_if(StaticGroups, [](const FStaticDrawGroup* Group) { return Group->Items.empty(); });
		std::sort(StaticGroups.begin(), StaticGroups.end(), [](const FStaticDrawGroup* A, const FStaticDrawGroup* B) { 
			if (A->Material->SortID != B->Material->SortID)
			{
				return A->Material->SortID < B->Material->SortID;
			}

			if (A->Geometry.VertexBufferCount != B->Geometry.VertexBufferCount)
			{
				return A->Geometry.VertexBufferCount < B->Geometry.VertexBufferCount;
			}

			for (int32 i = 0; i < A->Geometry.VertexBufferCount; ++i)
			{
				if (A->Geometry.VertexBuffers[i] != B->Geometry.VertexBuffers[i])
				{
					return A->Geometry.VertexBuffers[i] < B->Geometry.VertexBuffers[i];
				}
			}

			return A->Geometry.IndexBuffer < B->Geometry.IndexBuffer;
		});
	}
	// Gather uploads group and packet slots together. Other queues need an upload here.
	if (!bObjectConstantsPrepared)
		UploadPerObjectConstants();
	bObjectConstantsPrepared = false;
}

// 정렬된 패킷 중 [Begin, End) 범위를 View 행렬과 Section 범위로 그린다.
void FRenderer::DrawPackets(uint32 Begin, uint32 End)
{
	SCOPE_CYCLE_COUNTER(STAT_DrawRenderPackets);

	LastVertexBufferCount = 0;
	LastIndexBuffer = nullptr;
	LastMaterial = nullptr;
	int8 LastReceiveShadow = -1;
	int8 LastUseSkinning = -1;

	RenderCommand::BindConstantBuffer(0, ViewCB.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
	RenderCommand::BindConstantBuffer(3, LightingCB.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);

	// 한계: 같은 머티리얼인데 ReceiveShadow가 번갈아 다르면 셰이더를 매번 재바인딩하게 됨.
	// 제대로 하려면 정렬 키에 플래그를 넣어 같은 값끼리 모아야 하는데, 이번 마감 범위 밖입니다.
	for (uint32 k = Begin; k < End; ++k)          // k = 정렬된 위치
	{
		const FRenderPacket& RenderPacket = RenderPackets[k];

		if (!RenderPacket.Geometry.HasSameBuffers(LastVertexBuffers, LastVertexBufferCount, LastIndexBuffer)) 
		{
			RenderCommand::BindVertexBuffers(RenderPacket.Geometry.VertexBuffers, RenderPacket.Geometry.VertexBufferCount);
			RenderCommand::BindIndexBuffer(RenderPacket.Geometry.IndexBuffer);

			std::memcpy(LastVertexBuffers, RenderPacket.Geometry.VertexBuffers, sizeof(FVertexBuffer*) * RenderPacket.Geometry.VertexBufferCount);
			LastVertexBufferCount = RenderPacket.Geometry.VertexBufferCount;
			LastIndexBuffer = RenderPacket.Geometry.IndexBuffer;
		}

		const bool bReceiveShadow = RenderPacket.Proxy ? RenderPacket.Proxy->IsReceiveShadow() : true;
		const bool bUseSkinning = RenderPacket.Proxy ? RenderPacket.Proxy->IsUsingSkinning() : false;
		const int8 CurrentRS = bReceiveShadow ? 1 : 0;
		const int8 CurrentSkinning = bUseSkinning ? 1 : 0;
		if (RenderPacket.Material != LastMaterial || CurrentRS != LastReceiveShadow || CurrentSkinning != LastUseSkinning)
		{
			BindMaterial(RenderPacket.Material, bReceiveShadow, bUseSkinning);
		}

		if (RenderPacket.Material != LastMaterial || RenderPacket.MaterialParamData)
		{
			UpdateMaterialParams(RenderPacket);
		}

		if (bUsePerObjectSlots && RenderPacket.Slot != InvalidObjectSlot)
		{
			RenderCommand::BindConstantBufferRange(2, PerObjectSlotCB.get(), RenderPacket.Slot * PerObjectSlotConstants, PerObjectSlotConstants, EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
		}
		else
		{
			RenderCommand::BindConstantBuffer(2, PerObjectCB.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
			UpdatePerObjectConstants(RenderPacket);
		}

		if (RenderPacket.Proxy)
		{
			RenderPacket.Proxy->BindSRVs();
		}

		RenderCommand::DrawIndexed(RenderPacket.Geometry.IndexCount, RenderPacket.Geometry.StartIndex);

		LastMaterial = RenderPacket.Material;
		LastReceiveShadow = CurrentRS;
		LastUseSkinning = CurrentSkinning;
	}
}

FOcclusionMeasureResult FRenderer::MeasureOpaqueOcclusion()
{
	FOcclusionMeasureResult Result;

	// 불투명 드로우를 두 경로(스태틱 묶음, 일반 패킷)에서 한 목록으로 모은다. 그린 순서와 같게 묶음 먼저.
	struct FMeasureDraw
	{
		const FGeometryBinding* Geometry;
		UMaterial* Material;
		uint32 Slot;
		uint32 StartIndex;
		uint32 IndexCount;
		const FMatrix* World;       // 칸이 없을 때만 쓴다
		const void* ObjectKey;      // 같은 물체의 섹션을 한 물체로 센다
		bool bOccludedByGpu;
		const FRenderPacket* Packet;
		bool bUseSkinning;
		const FPrimitiveSceneProxy* Proxy;
	};

	std::vector<FMeasureDraw> Draws;
	for (const FStaticDrawGroup* Group : StaticGroups)
	{
		for (const FStaticDrawItem& Item : Group->Items)
		{
			if (!Group->Geometry.VertexBufferCount || !Group->Geometry.IndexBuffer || !Group->Material || !Item.Proxy || !Item.IndexCount)
				continue;
			Draws.push_back({ &Group->Geometry, Group->Material, Item.Slot, Item.StartIndex, Item.IndexCount, &Item.Proxy->GetLocalToWorld(), Item.Proxy, Item.bOccludedByGpu != 0, nullptr, Item.Proxy->IsUsingSkinning(), Item.Proxy });
		}
	}

	for (uint32 k = 0; k < FirstTranslucentIndex; ++k)
	{
		const FRenderPacket& Packet = RenderPackets[k];
		if (!Packet.Geometry.VertexBufferCount || !Packet.Geometry.IndexBuffer || !Packet.Material || !Packet.Geometry.IndexCount)
			continue;
		// 프록시가 없는 패킷(빌보드 등)은 패킷 자체를 한 물체로 센다.
		const void* Key = Packet.Proxy ? static_cast<const void*>(Packet.Proxy) : static_cast<const void*>(&Packet);
		Draws.push_back({ &Packet.Geometry, Packet.Material, Packet.Slot, Packet.Geometry.StartIndex, Packet.Geometry.IndexCount, &GetPacketWorld(Packet), Key, Packet.bOccludedByGpu, &Packet, Packet.Proxy && Packet.Proxy->IsUsingSkinning(), Packet.Proxy });
	}

	const uint32 Count = static_cast<uint32>(Draws.size());
	if (Count == 0)
		return Result;

	const auto StartTime = std::chrono::high_resolution_clock::now();

	ID3D11Device* Device = RenderCommand::GetDevice();
	ID3D11DeviceContext* Context = RenderCommand::GetContext();

	// 같은 셰이더·같은 행렬로 다시 그리면 깊이가 비트 단위로 같으므로,
	// LESS_EQUAL이면 최종 깊이 버퍼에서 이 물체가 이긴 픽셀만 통과한다.
	if (!DepthLessEqualReadOnly)
	{
		D3D11_DEPTH_STENCIL_DESC Desc{};
		Desc.DepthEnable = TRUE;
		Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		Desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		if (FAILED(Device->CreateDepthStencilState(&Desc, DepthLessEqualReadOnly.GetAddressOf())))
			return Result;
	}

	while (OcclusionQueries.Num() < Count)
	{
		const D3D11_QUERY_DESC QueryDesc{ D3D11_QUERY_OCCLUSION, 0 };
		ComPtr<ID3D11Query> Query;
		if (FAILED(Device->CreateQuery(&QueryDesc, Query.GetAddressOf())))
			return Result;
		OcclusionQueries.Add(std::move(Query));
	}

	RenderCommand::BindConstantBuffer(0, ViewCB.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
	RenderCommand::BindConstantBuffer(3, LightingCB.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
	if (!bUsePerObjectSlots)
		RenderCommand::BindConstantBuffer(2, PerObjectCB.get(), EShaderBindFlagBits::Vertex);

	const FGeometryBinding* BoundGeometry = nullptr;
	UMaterial* BoundMaterial = nullptr;
	bool bBoundUseSkinning = false;

	// 1) 드로우마다 쿼리를 걸고 다시 그린다. 본 패스와 같은 바인딩(칸 또는 PerObjectCB)을 쓴다.
	for (uint32 Index = 0; Index < Count; ++Index)
	{
		const FMeasureDraw& Draw = Draws[Index];

		const FGeometryBinding& Geometry = *Draw.Geometry;
		if (!BoundGeometry || !Geometry.HasSameBuffers(BoundGeometry->VertexBuffers, BoundGeometry->VertexBufferCount, BoundGeometry->IndexBuffer))
		{
			RenderCommand::BindVertexBuffers(Geometry.VertexBuffers, Geometry.VertexBufferCount);
			RenderCommand::BindIndexBuffer(Geometry.IndexBuffer);
		}

		if (Draw.Material != BoundMaterial || Draw.bUseSkinning != bBoundUseSkinning)
		{
			BindMaterial(Draw.Material, true, Draw.bUseSkinning);
			// BindMaterial이 바꾼 상태를 측정용으로 덮어쓴다.
			RenderCommand::SetBlendState(EBlendState::NoColorWrite);
			Context->OMSetDepthStencilState(DepthLessEqualReadOnly.Get(), 0);
		}
		if (Draw.Packet && (Draw.Material != BoundMaterial || Draw.Packet->MaterialParamData))
			UpdateMaterialParams(*Draw.Packet);

		if (bUsePerObjectSlots && Draw.Slot != InvalidObjectSlot)
			RenderCommand::BindConstantBufferRange(2, PerObjectSlotCB.get(),
				Draw.Slot * PerObjectSlotConstants, PerObjectSlotConstants, EShaderBindFlagBits::Vertex);
		else
		{
			RenderCommand::BindConstantBuffer(2, PerObjectCB.get(), EShaderBindFlagBits::Vertex);
			// 패킷이 없는 스태틱 묶음도 포함하는 측정용 행렬 폴백
			const uint32 ObjectMask = Draw.Proxy ? Draw.Proxy->GetObjectMask() : 0u;
			UpdatePerObjectConstants(*Draw.World, MakeNormalMatrix(*Draw.World), ObjectMask);
		}

		ID3D11Query* Query = OcclusionQueries[Index].Get();
		Context->Begin(Query);
		if (Draw.Proxy)
		{
			Draw.Proxy->BindSRVs();
		}
		RenderCommand::DrawIndexed(Draw.IndexCount, Draw.StartIndex);
		Context->End(Query);

		BoundGeometry = Draw.Geometry;
		BoundMaterial = Draw.Material;
		bBoundUseSkinning = Draw.bUseSkinning;
	}

	// 2) 결과를 기다려 모은다. 한 물체가 Section 여러 개로 나뉘면 하나라도 보이면 보이는 것으로 친다.
	std::unordered_map<const void*, bool> ObjectVisible;
	ObjectVisible.reserve(Count);
	for (uint32 Index = 0; Index < Count; ++Index)
	{
		const FMeasureDraw& Draw = Draws[Index];

		UINT64 Samples = 0;
		while (Context->GetData(OcclusionQueries[Index].Get(), &Samples, sizeof(Samples), 0) == S_FALSE) {}

		const uint64 Triangles = Draw.IndexCount / 3;
		const bool bVisible = Samples > 0;

		// Cull을 끄고 판정만 한 드로우: 가렸다고 했는데 최종 화면에 픽셀이 남았으면 잘못 가린 것이다.
		if (Draw.bOccludedByGpu)
		{
			++Result.GPUOccludedDraws;
			if (bVisible)
				++Result.FalseCulls;
		}

		++Result.TotalDraws;
		Result.TotalTriangles += Triangles;
		if (bVisible)
		{
			++Result.VisibleDraws;
			Result.VisibleTriangles += Triangles;
		}

		bool& bObjectVisible = ObjectVisible[Draw.ObjectKey];
		bObjectVisible = bObjectVisible || bVisible;
	}

	Result.TotalObjects = static_cast<uint32>(ObjectVisible.size());
	for (const auto& [Key, bVisible] : ObjectVisible)
		Result.VisibleObjects += bVisible ? 1 : 0;

	// 뒤따르는 Grid·반투명 패스를 위해 상태를 되돌린다.
	RenderCommand::SetBlendState(EBlendState::Opaque);
	RenderCommand::SetDepthStencilState(EDepthStencilState::Default);

	Result.ElapsedMs = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - StartTime).count();
	Result.bValid = true;
	return Result;
}

uint8* FRenderer::BeginObjectConstants(uint32 MaxSlots)
{
	bObjectConstantsPrepared = false;
	if (!bUsePerObjectSlots || MaxSlots == 0) return nullptr;
	EnsurePerObjectSlotCapacity(MaxSlots);
	if (!PerObjectSlotCB) { bUsePerObjectSlots = false; return nullptr; }
	return static_cast<uint8*>(RenderCommand::MapWriteDiscard(PerObjectSlotCB.get()));
}

void FRenderer::EndObjectConstants()
{
	if (PerObjectSlotCB)
	{
		RenderCommand::Unmap(PerObjectSlotCB.get());
		bObjectConstantsPrepared = true;
	}
}

void FRenderer::SetViewMode(const FViewRenderMode& Mode)
{
	for (int32 Skinning = 0; Skinning < 2; Skinning++)
	{
		for (int32 NM = 0; NM < 2; NM++) {
			for (int32 RS = 0; RS < 2; RS++) {
				ViewModeShaders[Skinning][NM][RS] = FRenderResourceManager::GetUberLitProgram(Mode, NM, RS, Skinning);
			}
		}
	}
}

void FRenderer::SetLighting(const FLightingConstants& GlobalLights,
	const TArray<FPointLightInfo>& PointLights, const TArray<FSpotLightInfo>& SpotLights)
{
	// 이전 View의 PS SRV 바인딩을 끊은 뒤 동적 LocalLight 버퍼를 다시 쓴다.
	TileLightCulling.UnbindForShading();
	RenderCommand::UpdateBufferData(LightingCB.get(), &GlobalLights);
	LocalLight.UploadLights(PointLights, SpotLights);

}

// Material마다 Shader/Texture/Sampler/State 꽂기
void FRenderer::BindMaterial(UMaterial* material, bool bReceiveShadow, bool bUseSkinning)
{
	// 스태틱 메시 머티리얼만 조명 대상이다. 빌보드·파티클(ParticleSubUV)은 자기 셰이더를 그대로 쓴다.
	if (material->ParamLayout == EMaterialParamLayout::StaticMesh)
	{
		if (!ViewModeShaders[0][1])
			SetViewMode(FViewRenderMode{});

		bool bUseNormalMap = material->Textures.size() > 1 && material->Textures[1] != nullptr;
		if (bUseNormalMap)
		{
			RenderCommand::BindShaderProgram(ViewModeShaders[bUseSkinning ? 1 : 0][1][bReceiveShadow ? 1 : 0]);
		}
		else
		{
			RenderCommand::BindShaderProgram(ViewModeShaders[bUseSkinning ? 1 : 0][0][bReceiveShadow ? 1 : 0]);
		}
	}
	else
	{
		RenderCommand::BindShaderProgram(material->Shader);
	}
	RenderCommand::SetBlendState(material->BlendState);
	// 반투명은 뒤에 그려지는 Grid·다른 반투명을 가리지 않도록 깊이를 쓰지 않는다.
	const bool bTranslucent = material->BlendState != EBlendState::Opaque;
	RenderCommand::SetDepthStencilState(bTranslucent && material->DepthStencilState == EDepthStencilState::Default
		? EDepthStencilState::ReadOnly : material->DepthStencilState);

	for (int i = 0; i < material->Textures.size(); i++)
	{
		// 같은 파일이 먼저 sRGB로 캐시됐을 수 있어 노멀맵 슬롯은 불러온 방식과 무관하게 원본 값으로 읽는다.
		const bool bNormalMapSlot = material->ParamLayout == EMaterialParamLayout::StaticMesh && i == 1;
		RenderCommand::BindShaderResource(i, material->Textures[i], EShaderBindFlagBits::Pixel, bNormalMapSlot);
	}
	RenderCommand::BindSamplerState(0, material->SamplerState, EShaderBindFlagBits::Pixel);
}

// b1 내용 채우고 꽂기
void FRenderer::UpdateMaterialParams(const FRenderPacket& RenderPacket)
{
	switch (RenderPacket.Material->ParamLayout)
	{
	case EMaterialParamLayout::StaticMesh:
	{
		//break; // 라이팅 적용 시 제거

		const float TotalTime = EngineTimer::GetTotalTime();

		FStaticMeshMaterialParams Params{};
		Params.BaseColor = ColorWorkflow::ToShaderColor(RenderPacket.Material->BaseColor);
		Params.UVScrollOffset = RenderPacket.Material->UVScrollSpeed * TotalTime;
		Params.bOpaque = RenderPacket.Material->BlendState == EBlendState::Opaque ? 1u : 0u;
		Params.Shininess = RenderPacket.Material->Shininess;
		const FLinearColor Specular = ColorWorkflow::ToShaderColor(RenderPacket.Material->SpecularColor);
		Params.SpecularColor = FVector(Specular.R, Specular.G, Specular.B);
		Params.bFlipNormalGreen = RenderPacket.Material->bFlipNormalGreen ? 1u : 0u;

		// 수신 머티리얼의 색상 합성 방식을 셰이더 상수로 전달
		Params.DecalColorBlendMode = static_cast<uint32>(RenderPacket.Material->DecalColorBlendMode);

		RenderCommand::UpdateBufferData(RenderPacket.Material->ParamBuffer.get(), &Params, sizeof(FStaticMeshMaterialParams));
		// Gouraud는 VS에서 스페큘러를 계산하므로 Shininess가 VS에도 필요하다.
		RenderCommand::BindConstantBuffer(1, RenderPacket.Material->ParamBuffer.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
		break;
	}
	case EMaterialParamLayout::ParticleSubUV:
	{
		if (RenderPacket.Material->ParamBuffer && RenderPacket.MaterialParamData != nullptr)
		{
			RenderCommand::UpdateBufferData(RenderPacket.Material->ParamBuffer.get(), RenderPacket.MaterialParamData, RenderPacket.MaterialParamDataSize);
			RenderCommand::BindConstantBuffer(1, RenderPacket.Material->ParamBuffer.get(), EShaderBindFlagBits::Pixel);
		}
		break;
	}
	case EMaterialParamLayout::Billboard:
	{
		if (RenderPacket.Material->ParamBuffer && RenderPacket.MaterialParamData != nullptr)
		{
			RenderCommand::UpdateBufferData(RenderPacket.Material->ParamBuffer.get(), RenderPacket.MaterialParamData, RenderPacket.MaterialParamDataSize);
			RenderCommand::BindConstantBuffer(1, RenderPacket.Material->ParamBuffer.get(), EShaderBindFlagBits::Pixel);
		}
		break;
	}
	case EMaterialParamLayout::None:
	{
		break;
	}
	}
}

// b0 MVP 채우고 꽂기
void FRenderer::UpdatePerObjectConstants(const FRenderPacket& RenderPacket)
{
	// 빌보드·파티클처럼 Proxy 없이 제출된 패킷은 수신 플래그를 기록하지 않는다.
	const uint32 ObjectMask = RenderPacket.Proxy ? RenderPacket.Proxy->GetObjectMask() : 0u;
	UpdatePerObjectConstants(GetPacketWorld(RenderPacket), GetPacketNormalMatrix(RenderPacket), ObjectMask);
}

void FRenderer::UpdatePerObjectConstants(const FMatrix& World, const FMatrix& NormalMatrix, uint32 ObjectMask)
{
	FPerObjectConstants Constants{};
	Constants.World = World;
	Constants.NormalMatrix = NormalMatrix;
	Constants.ObjectMask = ObjectMask;

	RenderCommand::UpdateBufferData(PerObjectCB.get(), &Constants);
}

void FRenderer::EnsureTileLightCapacity(const FMatrix& ViewProjection, uint32 Width, uint32 Height, bool bVisualizeLightCount)
{
	if (TileLightCulling.EnsureTileCapacity(Width, Height))
		TileLightCulling.Dispatch(LocalLight, ViewProjection, Width, Height, bVisualizeLightCount);

}

void FRenderer::RenderShadowPass(
	const FShadowInfo* DirectionalShadow,
	const FVector& LightDirection,
	const FVector& CameraPosition,
	const FVector& CameraForward,
	float FovDegrees,
	float AspectRatio,
	TArray<FShadowInfo>& ShadowInfos,
	const TArray<FPrimitiveSceneProxy*>& Casters,
	FLightSceneProxy* PreviewShadowMapProxy,
	FShadowStats& ShadowStats,
	bool bVisualizeCascades,
	bool bCacheHit,
	bool bDrawSkeletalMeshes)
{
	SCOPE_CYCLE_COUNTER(STAT_Shadow);

	//이전 pass에서의 바인딩을 끊는다.
	ShadowMapManager.UnbindForShading();
	
	// Shadow Atlas 를 계산한다.
	ShadowMapManager.BuildAtlas(ShadowInfos);


	// Directional 상수 버퍼 준비 (카메라 기준 4개 외접구 및 Light VP 행렬 계산 후 b5 업로드)
	if (DirectionalShadow != nullptr)
	{
		ShadowMapManager.SetupDirectionalShadow(*DirectionalShadow, LightDirection, CameraPosition, CameraForward, FovDegrees, AspectRatio, bVisualizeCascades);
	}

	// Local Light들 준비
	ShadowMapManager.UploadShadowInfos(ShadowInfos);

	ID3D11DeviceContext* Context = RenderCommand::GetContext();

	//공통 파이프라인 상태 (층마다 바뀌지 않음)
	FShaderProgram* DepthShader = FRenderResourceManager::GetShaderProgram("Resources/Shader/ShaderMap.hlsl");

	//VS만 바인딩
	Context->VSSetShader(DepthShader->VertexShader->GetShader(), nullptr, 0);
	Context->IASetInputLayout(DepthShader->VertexShader->GetLayout());
	Context->PSSetShader(nullptr, nullptr, 0);

	RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	RenderCommand::SetRasterizerState(ERasterizerState::ShadowDepth);
	// Depth on, LESS, Write
	RenderCommand::SetDepthStencilState(EDepthStencilState::Default);
	RenderCommand::SetBlendState(EBlendState::Opaque);

	RenderCommand::BindConstantBuffer(0, ShadowViewCB.get(), EShaderBindFlagBits::Vertex);
	RenderCommand::BindConstantBuffer(2, PerObjectCB.get(), EShaderBindFlagBits::Vertex);

	// 아틀라스
	// 캐스터 메쉬들을 그리는 공통 람다
	auto DrawCasters = [&]() {
		for (FPrimitiveSceneProxy* Proxy : Casters)
		{
			if (!Proxy || !Proxy->IsVisible() || !Proxy->GetRenderState() || (!bDrawSkeletalMeshes && Proxy->IsUsingSkinning()))
			{
				continue;
			}

			if (!Proxy->IsCastShadow()) {
				ShadowStats.NumSkippedCasterDraws++;
				continue;
			}

			const FCachedMeshLOD& LOD = Proxy->GetLOD(0);
			if (!LOD.VertexBufferCount || !LOD.IndexBuffer)
			{
				continue;
			}

			UpdatePerObjectConstants(Proxy->GetLocalToWorld(), Proxy->GetNormalMatrix(), Proxy->GetObjectMask());
			RenderCommand::BindVertexBuffers(LOD.VertexBuffers, LOD.VertexBufferCount);
			RenderCommand::BindIndexBuffer(LOD.IndexBuffer);

			for (uint32 i = 0; i < LOD.NumSections; ++i)
			{
				const FCachedMeshSection& Section = Proxy->GetSection(LOD.FirstSection + i);
				if (Section.Material && Section.Material->BlendState != EBlendState::Opaque)
					continue;
				RenderCommand::DrawIndexed(Section.IndexCount, Section.StartIndex);
				ShadowStats.NumShadowDrawCalls++;
			}
		}
	};

	if (DirectionalShadow != nullptr)
	{
		// 4개의 캐스케이드를 4096 Atlas의 2x2 분할 영역에 각각 렌더
		for (uint32 Cascade = 0; Cascade < 4; ++Cascade)
		{
			ShadowMapManager.BeginDirectionalCascadeRender(Cascade);
			const FMatrix& CascadeVP = ShadowMapManager.GetCascadeMatrix(Cascade);
			RenderCommand::UpdateBufferData(ShadowViewCB.get(), &CascadeVP, sizeof(FMatrix));
			DrawCasters();
		}
		ShadowMapManager.EndDirectionalShadowRender();
	}

	if (!bCacheHit)
	{
		ShadowMapManager.ClearAtlas();

		//층(라이트)마다
		for (int32 Slice = 0; Slice < ShadowInfos.Num(); ++Slice)
		{
			//렌더 전 개별 바인딩
			ShadowMapManager.BeginShadowRenderWithAtlas(ShadowInfos[Slice]);


			//라이트마다의 VP 행렬 Map Unmap
			RenderCommand::UpdateBufferData(ShadowViewCB.get(), &ShadowInfos[Slice].LightViewProjection, sizeof(FMatrix));

			DrawCasters();
		}

		//DSV 해제
		if (ShadowInfos.Num() > 0)
		{
			ShadowMapManager.EndShadowPass();
		}
	}

	// TODO : 아틀라스 프리뷰
	//Preview 인덱스의 ShadowMap 서브 리소스를 복사한다.
	if (PreviewShadowMapProxy != nullptr)
	{
		ShadowMapManager.UpdatePreview(PreviewShadowMapProxy, ShadowInfos);
	}

	if (!bCacheHit) {
		// 다 새로 그리기
		int32 RenderedCount = ShadowInfos.Num();
		if (DirectionalShadow != nullptr)
		{
			RenderedCount += 4;
		}
		ShadowStats.NumShadowMapsRendered = RenderedCount;
		ShadowStats.NumCachedShadowMaps = 0;
	}
	else {
		// 재사용 
		int32 RenderedCount = 0;
		if (DirectionalShadow != nullptr)
		{
			RenderedCount += 4;
		}
		ShadowStats.NumShadowMapsRendered = RenderedCount;
		ShadowStats.NumCachedShadowMaps = ShadowInfos.Num();
	}

	int64 TotalBytes = (int64)ShadowMapManager.GetAtlasResolution() * ShadowMapManager.GetAtlasResolution() * 4 * ShadowMapManager.GetNumAtlas();
	if (ShadowMapManager.GetDirectionalShadowSRV() != nullptr)
	{
		TotalBytes += (int64)ShadowMapManager.GetDirectionalResolution() * ShadowMapManager.GetDirectionalResolution() * 4;
	}

	ShadowStats.ShadowMapMemoryBytes = TotalBytes;
}

void FRenderer::BindShadowForShading()
{
	ShadowMapManager.BindForShading();
}
