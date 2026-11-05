#pragma once

#include "Buffer.h"
#include "PipelineState.h"
#include "Containers/Array.h"
#include "Math/Matrix.h"
#include "Math/Color.h"
#include "Math/Vector2.h"

class FDecalSceneProxy;
class FTexture2D;

// 데칼 투영의 GPU 자원 및 드로우 제출 관리
class FDecalRenderer
{
public:
	bool Init();

	void Render(const TArray<FDecalSceneProxy*>& InDecalProxies, const FMatrix& ViewProjection,
		FTexture2D* InDepthTexture, FTexture2D* InObjectMaskTexture, FTexture2D* InNormalTexture,
		uint32& OutDrawCount, bool bGameView);
private:
	// 볼륨 투영 변환 및 페이드가 반영된 데칼 색상
	struct FDecalConstants
	{
		FMatrix InvViewProjection;
		FMatrix InvWorld;
		FMatrix WorldViewProjection;
		FLinearColor Tint;
		FVector2 InvTargetSize;
		FVector2 UVScrollOffset;
		uint32 bAffectBaseColor;
		uint32 bAffectNormal;
		uint32 bFlipNormalGreen;
		uint32 DecalPadding;
	};
	static_assert(sizeof(FDecalConstants) == 240);

	FPipelineState PipelineState;
	TUniquePtr<FConstantBuffer> ConstantBuffer;

	// 프로젝션용 박스 버퍼
	TUniquePtr<FVertexBuffer> VolumeVertexBuffer;
	TUniquePtr<FIndexBuffer> VolumeIndexBuffer;


	// 뷰별 가시 데칼 목록의 메모리 재사용
	TArray<const FDecalSceneProxy*> VisibleDecals;
};
