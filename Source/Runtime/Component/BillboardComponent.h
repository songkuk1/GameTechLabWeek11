#pragma once

#include "PrimitiveComponent.h"
#include "Render/Material.h"

struct FBillboardView;

class UBillboardComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UBillboardComponent, UPrimitiveComponent)

	REFLECT_START(ClassName)
	REFLECT_END()

public:
	UBillboardComponent();
	virtual ~UBillboardComponent() override;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;

	virtual int32 GetNumMaterials() const override { return 1; }
	virtual UMaterial* GetMaterial(int32 SlotIndex) const override { return SlotIndex == 0 ? Material : nullptr; }
	virtual void SetMaterial(int32 SlotIndex, UMaterial* InMaterial) override { if (SlotIndex == 0) Material = InMaterial; }

	virtual const FModelMeshData* GetMeshData() const override { return QuadMesh ? &QuadMesh->GetMeshData() : nullptr; }
	virtual FBox CalcLocalBounds() const override;

	void SetTintColor(const FLinearColor& InColor) { BillboardParams.TintColor = InColor; bFollowLightColor = false; }
	void SetTintColor(const FColor& InColor) { BillboardParams.TintColor = InColor.ReinterpretAsLinear(); bFollowLightColor = false; }
	const FLinearColor& GetTintColor() const { return BillboardParams.TintColor; }

	void SetFollowLightColor(bool bFollow) { bFollowLightColor = bFollow; }
	bool IsFollowingLightColor() const { return bFollowLightColor; }

	void SetTexture(UTexture2D* InTexture);
	void SetTexture(const FString& TexturePath);
	const FString& GetTexturePath() const { return TexturePath; }

	FMatrix GetBillboardMatrix(const FBillboardView& View) const;
	// View가 없으면 그릴 수 없음
	virtual void SubmitToRenderQueue(FRenderQueue& RenderQueue) override {}
	// View별 Billboard 행렬을 계산해 같은 렌더 패킷 형식으로 제출한다.
	virtual void SubmitToRenderQueue(FRenderQueue& RenderQueue, const FBillboardView& View);
	// View별 Adapter가 계산한 Billboard 행렬을 사용해 같은 렌더 패킷 형식으로 제출한다.
	void SubmitToRenderQueue(FRenderQueue& RenderQueue, const FMatrix& BillboardWorldMatrix);

	virtual void Serialize(json& Handle, bool bIsLoading) override;

	using Super::Serialize;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
protected:
	UMaterial* Material = nullptr;
	UStaticMesh* QuadMesh = nullptr;
	FBillboardMaterialParams BillboardParams;
	FString TexturePath;
	bool bFollowLightColor = true;
private:

};
