#include "EnginePCH.h"
#include "AnimationRuntime.h"
#include "Math/Matrix.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"

#include <cmath>
#include <algorithm>
#include <cassert>

namespace
{
	struct FBlendTransform
	{
		FVector Position;
		FQuat Rotation;
		FVector Scale;
	};

    bool DecomposeBlendMatrix(const FMatrix& Matrix, FBlendTransform& OutTransform)
    {
        const FVector AxisX(Matrix[0][0], Matrix[0][1], Matrix[0][2]);
        const FVector AxisY(Matrix[1][0], Matrix[1][1], Matrix[1][2]);
        const FVector AxisZ(Matrix[2][0], Matrix[2][1], Matrix[2][2]);

        OutTransform.Scale = FVector(AxisX.Size(), AxisY.Size(), AxisZ.Size());

        if (OutTransform.Scale.X <= 1.e-8f || OutTransform.Scale.Y <= 1.e-8f || OutTransform.Scale.Z <= 1.e-8f)
        {
            return false;
        }

        FMatrix RotationMatrix = Matrix;

        for (int32 Column = 0; Column < 3; ++Column)
        {
            RotationMatrix[0][Column] /= OutTransform.Scale.X;
            RotationMatrix[1][Column] /= OutTransform.Scale.Y;
            RotationMatrix[2][Column] /= OutTransform.Scale.Z;
        }

        OutTransform.Position = FVector(Matrix[3][0], Matrix[3][1],  Matrix[3][2]);
        OutTransform.Rotation = MatrixToRotator(RotationMatrix).Quaternion();

        return true;
    }

    FMatrix ComposeBlendMatrix(const FBlendTransform& Transform)
    {
        const FMatrix RotationMatrix = Transform.Rotation.ToFMatrix();
        const FVector& Scale = Transform.Scale;
        const FVector& Position = Transform.Position;

        return FMatrix(
            RotationMatrix[0][0] * Scale.X, RotationMatrix[0][1] * Scale.X, RotationMatrix[0][2] * Scale.X, 0.0f,
            RotationMatrix[1][0] * Scale.Y, RotationMatrix[1][1] * Scale.Y, RotationMatrix[1][2] * Scale.Y, 0.0f,
            RotationMatrix[2][0] * Scale.Z, RotationMatrix[2][1] * Scale.Z, RotationMatrix[2][2] * Scale.Z, 0.0f,
            Position.X,                     Position.Y,                     Position.Z,                     1.0f
        );
    }

    FMatrix BlendTransformMatrices(const FMatrix& MatrixA, const FMatrix& MatrixB, float Alpha)
    {
        if (Alpha <= 0.0f)
        {
            return MatrixA;
        }

        if (Alpha >= 1.0f)
        {
            return MatrixB;
        }

        FBlendTransform A;
        FBlendTransform B;

        const bool bValidA = DecomposeBlendMatrix(MatrixA, A);
        const bool bValidB = DecomposeBlendMatrix(MatrixB, B);

        if (!bValidA || !bValidB)
        {
            return Alpha < 0.5f ? MatrixA : MatrixB;
        }

        FBlendTransform Result;

        Result.Position = A.Position + (B.Position - A.Position) * Alpha;

        Result.Scale = A.Scale + (B.Scale - A.Scale) * Alpha;

        Result.Rotation = FQuat::Slerp(A.Rotation, B.Rotation, Alpha);

        return ComposeBlendMatrix(Result);
    }
}

void AnimationRuntime::BlendPoses(const FPoseContext& PoseA, const FPoseContext& PoseB, float Alpha, FPoseContext& OutPose)
{
    assert(PoseA.Pose.Num() == PoseB.Pose.Num());
    if (PoseA.Pose.Num() != PoseB.Pose.Num())
    {
        return;
    }
    Alpha = std::clamp(Alpha, 0.0f, 1.0f);
    const int32 Num = PoseA.Pose.Num();
    OutPose.Pose.SetNum(Num);
    for (int32 Index = 0; Index < Num; ++Index)
    {
        OutPose.Pose[Index] = BlendTransformMatrices(PoseA.Pose[Index], PoseB.Pose[Index], Alpha);
    }
}
