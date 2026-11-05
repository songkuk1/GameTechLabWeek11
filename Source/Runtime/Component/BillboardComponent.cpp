#include "EnginePCH.h"
#include "BillboardComponent.h"

#include "Asset/AssetManager.h"
#include "Serialization/TypeSerializer.h"

#include "GameFramework/Actor.h"
#include "Component/LightComponentBase.h"

// Billboard 컴포넌트의 초기 상태를 구성한다.
UBillboardComponent::UBillboardComponent()
{
	QuadMesh = UAssetManager::GetAssetByPath<UStaticMesh>("Quad");
	Material = UAssetManager::GetAssetByPath<UMaterial>("BillboardMaterial");
	SetHiddenInGame(true);
}

// Billboard 컴포넌트의 소멸을 처리한다.
UBillboardComponent::~UBillboardComponent()
{
}

// 기본 Mesh와 Material을 에셋 관리자에서 가져온다.
void UBillboardComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!Material)
	{
		Material = UAssetManager::GetAssetByPath<UMaterial>("BillboardMaterial");
	}
}

void UBillboardComponent::SetTexture(UTexture2D* InTexture)
{
	if (!InTexture) return;

	if (!Material)
	{
		Material = UAssetManager::GetAssetByPath<UMaterial>("BillboardMaterial");
	}
	if (!Material) return;

	if (!Material->bIsInstance)
	{
		Material = UMaterial::CreateInstance(Material);
	}

	if (Material->Textures.Num() > 0)
	{
		Material->Textures[0] = InTexture;
	}
	else
	{
		Material->Textures.Add(InTexture);
	}
}

void UBillboardComponent::SetTexture(const FString& InTexturePath)
{
	TexturePath = InTexturePath;
	if (UTexture2D* Tex = UAssetManager::Get().LoadTexture(InTexturePath))
	{
		SetTexture(Tex);
	}
}

FMatrix UBillboardComponent::GetBillboardMatrix(const FBillboardView& View) const
{
	const FVector Scale = GetWorldScale3D();
	return View.MakeMatrix(GetWorldLocation(), Scale.Y, Scale.Z);
}

// 부모 컴포넌트의 프레임 갱신을 호출한다.
void UBillboardComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
}

// 그릴 때 View마다 카메라 쪽으로 돌아가므로, 쿼드 AABB를 그대로 쓰면 BVH가 회전한 빌보드를 놓친다.
// 어느 방향으로 돌아도 쿼드를 감싸는 반지름을 쓰되, 렌더처럼 월드 스케일 Y·Z를 폭·높이로 본다.
FBox UBillboardComponent::CalcLocalBounds() const
{
	if (!QuadMesh)
		return Super::CalcLocalBounds();

	const FBox& Quad = QuadMesh->GetMeshData().AABB;
	const FVector Scale = GetWorldScale3D();
	const float HalfWidth = std::max(std::fabs(Quad.Min.Y), std::fabs(Quad.Max.Y)) * std::fabs(Scale.Y);
	const float HalfHeight = std::max(std::fabs(Quad.Min.Z), std::fabs(Quad.Max.Z)) * std::fabs(Scale.Z);
	const float Radius = std::sqrt(HalfWidth * HalfWidth + HalfHeight * HalfHeight);

	// 프록시가 이 상자에 컴포넌트 월드 행렬(스케일 포함)을 곱하므로 축마다 스케일을 미리 나눈다.
	constexpr float MinScale = 1.0e-4f;
	const FVector Half(
		Radius / std::max(std::fabs(Scale.X), MinScale),
		Radius / std::max(std::fabs(Scale.Y), MinScale),
		Radius / std::max(std::fabs(Scale.Z), MinScale));
	return FBox{ Half * -1.0f, Half };
}

void UBillboardComponent::SubmitToRenderQueue(FRenderQueue& RenderQueue, const FBillboardView& View)
{
	SubmitToRenderQueue(RenderQueue, GetBillboardMatrix(View));
}

// View별 Billboard 행렬과 Material을 렌더 패킷에 담는다.
void UBillboardComponent::SubmitToRenderQueue(FRenderQueue& RenderQueue, const FMatrix& BillboardWorldMatrix)
{
	if (QuadMesh == nullptr)
		return;

	if (!Material)
	{
		Material = UAssetManager::GetAssetByPath<UMaterial>("BillboardMaterial");
	}

	if (!TexturePath.empty() && (!Material || !Material->bIsInstance))
	{
		if (UTexture2D* Tex = UAssetManager::Get().LoadTexture(TexturePath))
		{
			SetTexture(Tex);
		}
	}

	if (Material == nullptr)
		return;

	if (bFollowLightColor)
	{
		if (const ULightComponentBase* LightParent = Cast<ULightComponentBase>(AttachParent))
		{
			BillboardParams.TintColor = LightParent->GetLightColor().ReinterpretAsLinear();
		}
	}

	FRenderPacket Packet;
	FVertexBuffer* VertexBuffer = QuadMesh->GetVertexBuffer(0);
	Packet.Geometry.SetVertexBuffers(&VertexBuffer, 1);
	Packet.Geometry.IndexBuffer = QuadMesh->GetIndexBuffer(0);
	Packet.Geometry.StartIndex = 0;
	Packet.Geometry.IndexCount = Packet.Geometry.IndexBuffer->GetIndexCount();
	Packet.Material = Material;
	Packet.Model = RenderQueue.StoreWorldMatrix(BillboardWorldMatrix);
	Packet.MaterialParamData = &BillboardParams;
	Packet.MaterialParamDataSize = sizeof(FBillboardMaterialParams);
	RenderQueue.Add(Packet);
}

void UBillboardComponent::Serialize(json& Handle, bool bIsLoading)
{
	Super::Serialize(Handle, bIsLoading);

	if (bIsLoading)
	{
		// 예전 파일은 "Material"이 문자열(경로)이라 형식을 확인하고 읽는다
		if (Handle.contains("Material") && Handle["Material"].is_object())
		{
			if (UMaterial* Loaded = UMaterial::LoadMaterial(Handle["Material"]))
			{
				Material = Loaded;   // 못 만들었으면 생성자 기본값 유지
			}
		}
	}
	else
	{
		Handle["Material"] = Material ? UMaterial::SaveMaterial(Material) : json(nullptr);
	}
}

void UBillboardComponent::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);

	UMaterial* Loaded = Material;
	UMaterial::SerializeMaterialReference(Record.EnterField("Material"), Loaded);

	// 불러오기에 실패하면(null) 생성자가 넣어 둔 기본 머티리얼을 유지한다
	if (Record.GetUnderlyingArchive().IsLoading() && Loaded)
		Material = Loaded;
}
