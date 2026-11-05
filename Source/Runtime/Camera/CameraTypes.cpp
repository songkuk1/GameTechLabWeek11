#include "EnginePCH.h"
#include "CameraTypes.h"

FMatrix FMinimalViewInfo::CalculateViewMatrix() const
{
    FQuat WorldQuat = Rotation.Quaternion();
    FMatrix RotationMatrix = WorldQuat.ToFMatrix();

    FMatrix InverseTranslation = FMatrix::MakeTranslation(-Location);
    FMatrix InverseRotation = RotationMatrix.GetTransposed();

    return InverseTranslation * InverseRotation;
}

FMatrix FMinimalViewInfo::CalculateProjectionMatrix(float Aspect) const
{
	if (bOrthographic)
	{
        const float Width = OrthoWidth;
        const float Height = Width / Aspect;

        const float XScale = 2.0f / Width;
        const float YScale = 2.0f / Height;

        const float ZScale = 1.0f / (FarClip - NearClip);
        const float ZOffset = -NearClip / (FarClip - NearClip);

        return {
            0.0f, 0.0f, ZScale, 0.0f,
            XScale, 0.0f, 0.0f, 0.0f,
            0.0f, YScale, 0.0f, 0.0f,
            0.0f, 0.0f, ZOffset, 1.0f
        };
	}

    const float HalfFOV = FMath::DegreesToRadians(FOV) * 0.5f;

    const float YScale = 1.0f / tan(HalfFOV);
    const float XScale = YScale / Aspect;

    const float ZScale = FarClip / (FarClip - NearClip);
    const float ZOffset = -NearClip * FarClip / (FarClip - NearClip);

    //Forward가 X인 좌표
    return {
        0.0f, 0.0f, ZScale, 1.0f,
        XScale, 0.0f, 0.0f, 0.0f,
        0.0f, YScale, 0.0f, 0.0f,
        0.0f, 0.0f, ZOffset, 0.0f
    };
}

FMatrix FMinimalViewInfo::CalculateViewProjectionMatrix(float Aspect) const
{
    return CalculateViewMatrix() * CalculateProjectionMatrix(Aspect);
}

FRay FMinimalViewInfo::DeProjection(const FVector2& MousePos, float ScreenWidth, float ScreenHeight) const
{
    // 마우스 화면 좌표를 NDC 좌표(-1 ~ 1)로 변환
    const float NDCX = 2.0f * MousePos.X / ScreenWidth - 1.0f;
    const float NDCY = 1.0f - 2.0f * MousePos.Y / ScreenHeight;


    // Projection / View 행렬의 역행렬
    const FMatrix InvProjection = CalculateProjectionMatrix(ScreenWidth / ScreenHeight).Inverse();
    const FMatrix InvView = CalculateViewMatrix().Inverse();


    // NDC 공간의 Near / Far 지점
    FVector4 NearPoint(NDCX, NDCY, 0.0f, 1.0f);
    FVector4 FarPoint(NDCX, NDCY, 1.0f, 1.0f);


    // Projection 역변환
    NearPoint = NearPoint * InvProjection;
    FarPoint = FarPoint * InvProjection;


    // Perspective Divide 복원
    NearPoint /= NearPoint.W;
    FarPoint /= FarPoint.W;


    // View 역변환
    NearPoint = NearPoint * InvView;
    FarPoint = FarPoint * InvView;


    // Homogeneous 좌표 정리
    NearPoint /= NearPoint.W;
    FarPoint /= FarPoint.W;

    FRay Ray;

    if (bOrthographic)
    {
        Ray.Origin = FVector(NearPoint.X, NearPoint.Y, NearPoint.Z);
    }
    // 원근 투영은 모든 Ray가 카메라 위치에서 시작
    else
    {
        Ray.Origin = Location;
    }

    // 시작점에서 Far 지점을 향하는 방향을 Ray 방향으로 사용
    Ray.Direction = (FVector(FarPoint.X, FarPoint.Y, FarPoint.Z) - Ray.Origin).Normalized();

    return Ray;
}

void FMinimalViewInfo::BlendViewInfo(const FMinimalViewInfo& OtherInfo, float OtherWeight)
{
	AspectRatio = FMath::Lerp(AspectRatio, OtherInfo.AspectRatio, OtherWeight);
	bConstrainAspectRatio |= OtherInfo.bConstrainAspectRatio;
}

FBillboardView FBillboardView::FromView(const FMinimalViewInfo& View)
{
	const FQuat Rotation = View.Rotation.Quaternion();
    return {
        .Location = View.Location,
        .Forward = Rotation.RotateVector(FVector(1.0f, 0.0f, 0.0f)).Normalized(),
        .Right = Rotation.RotateVector(FVector(0.0f, 1.0f, 0.0f)).Normalized(),
        .Up = Rotation.RotateVector(FVector(0.0f, 0.0f, 1.0f)).Normalized()
    };
}

FMatrix FBillboardView::MakeMatrix(const FVector& Position, float Width, float Height) const
{
    FMatrix Matrix;
    Matrix.SetIdentity();

	FVector Facing = -Forward;

    Matrix.M[0][0] = Facing.X; Matrix.M[0][1] = Facing.Y; Matrix.M[0][2] = Facing.Z;
    Matrix.M[1][0] = Right.X * Width; Matrix.M[1][1] = Right.Y * Width; Matrix.M[1][2] = Right.Z * Width;
    Matrix.M[2][0] = Up.X * Height; Matrix.M[2][1] = Up.Y * Height; Matrix.M[2][2] = Up.Z * Height;
    Matrix.M[3][0] = Position.X; Matrix.M[3][1] = Position.Y; Matrix.M[3][2] = Position.Z;

    return Matrix;
}
