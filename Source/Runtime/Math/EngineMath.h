#pragma once
#include <cmath>
#include "Vector.h"
#include "Vector2.h"
#include "Vector4.h"
//#include "TVector.h"
#include "Matrix.h"
#include "Quat.h"
#include "Rotator.h"
#include "Collision/Ray.h"
#include "VectorRegister.h"
#include "MatrixRegister.h"

#define PI 3.141592f
#define INDEX_NONE -1

namespace FMath
{
	//static float PI = std::acosf(-1);

	template <typename T>
	static inline T Min(T A, T B)
	{
		return (A >= B ? B : A);
	}

	template <typename T>
	static inline T Max(T A, T B)
	{
		return (A >= B ? A: B);
	}

	template <typename T>
	static inline T Clamp(T InValue, T InMin, T InMax)
	{
		return Max(InMin, Min(InValue, InMax));
	}

	static inline bool IsNearlyZero(float Value, float ErrorTolerance = 1e-20f)
	{
		return std::abs(Value) <= ErrorTolerance;
	}

	static inline bool IsNearlyEqual(float Value1, float Value2, float ErrorTolerance = 1e-4f)
	{
		return std::abs(Value1 - Value2) <= ErrorTolerance;
	}

	static inline float RadiansToDegrees(float Radian)
	{
		return Radian * (180.0f / PI);
	}

	static inline float DegreesToRadians(float Degree)
	{
		return Degree * (PI / 180.0f);
	}

	template <typename T>
	static inline T Lerp(const T& A, const T& B, float Ratio)
	{
		return A * (1.f - Ratio) + B * Ratio;
	}

	static float FInterpTo(float Current, float Target, float DeltaTime, float InterpSpeed)
	{
		if (InterpSpeed <= 0.0f)
		{
			return Target;
		}
		return Current + (Target - Current) * Clamp(DeltaTime * InterpSpeed, 0.0f, 1.0f);
	}

	static FVector VInterpolate(const FVector& Current, const FVector& Target, float DeltaTime, float InterpSpeed)
	{
		if (InterpSpeed <= 0.0f)
		{
			return Target;
		}

		return Current + (Target - Current) * Clamp(DeltaTime * InterpSpeed, 0.0f, 1.0f);
	}

	static FQuat QInterpTo(const FQuat& Current, const FQuat& Target, float DeltaTime, float InterpSpeed)
	{
		if (InterpSpeed <= 0.0f)
		{
			return Target;
		}

		return FQuat::Slerp(Current, Target, Clamp(DeltaTime * InterpSpeed, 0.0f, 1.0f));
	}

	static float ExpSmoothingAlpha(float DeltaTime, float Speed)
	{
		return Speed <= 0.0f ? 1.0f : 1.0f - std::exp(-Speed * DeltaTime);
	}

	static FVector VExpSmoothTo(const FVector& Current, const FVector& Target, float DeltaTime, float Speed)
	{
		return Current + (Target - Current) * ExpSmoothingAlpha(DeltaTime, Speed);
	}

	static FQuat QExpSmoothTo(const FQuat& Current, const FQuat& Target, float DeltaTime, float Speed)
	{
		return FQuat::Slerp(Current, Target, ExpSmoothingAlpha(DeltaTime, Speed));
	}

	template <typename TVertex>
	static void CalculateNormals(TVertex* Vertices, uint32 VertexCount, const uint32* Indices, uint32 IndexCount)
	{
		for (int32 i = 0; i < VertexCount; ++i)
		{
			Vertices[i].Normal = FVector();
		}

		for (int32 i = 0; i < IndexCount; i += 3)
		{
			uint32 IndexA = Indices[i];
			uint32 IndexB = Indices[i + 1];
			uint32 IndexC = Indices[i + 2];

			FVector A = Vertices[IndexA].Position;
			FVector B = Vertices[IndexB].Position;
			FVector C = Vertices[IndexC].Position;

			FVector Edge1 = B - A;
			FVector Edge2 = C - A;

			FVector Normal = FVector::Cross(Edge1, Edge2);

			Vertices[IndexA].Normal += Normal;
			Vertices[IndexB].Normal += Normal;
			Vertices[IndexC].Normal += Normal;
		}

		for (int32 i = 0; i < VertexCount; ++i)
		{
			Vertices[i].Normal = Vertices[i].Normal.Normalized();
		}
	}

	template <typename TVertex>
	static void CalculateTangents(TVertex* Vertices, uint32 VertexCount, const uint32* Indices, uint32 IndexCount)
	{
		TArray<FVector> Bitangents;
		Bitangents.Init(FVector(), VertexCount);

		for (int32 i = 0; i < VertexCount; ++i)
		{
			Vertices[i].Tangent = FVector4();
		}

		for (int32 i = 0; i < IndexCount; i += 3)
		{
			uint32 IndexA = Indices[i];
			uint32 IndexB = Indices[i + 1];
			uint32 IndexC = Indices[i + 2];

			FVector A = Vertices[IndexA].Position;
			FVector B = Vertices[IndexB].Position;
			FVector C = Vertices[IndexC].Position;

			FVector Edge1 = B - A;
			FVector Edge2 = C - A;

			FVector2 uvA = Vertices[IndexA].UV;
			FVector2 uvB = Vertices[IndexB].UV;
			FVector2 uvC = Vertices[IndexC].UV;

			float DeltaU1 = uvB.X - uvA.X;
			float DeltaV1 = uvB.Y - uvA.Y;
			float DeltaU2 = uvC.X - uvA.X;
			float DeltaV2 = uvC.Y - uvA.Y;

			float Det = (DeltaU1 * DeltaV2 - DeltaU2 * DeltaV1);

			if (std::abs(Det) < 1e-12f)
			{
				continue;
			}

			FVector FaceTangent = (Edge1 * DeltaV2 - Edge2 * DeltaV1) / Det;
			FVector FaceBitangent = (Edge2 * DeltaU1 - Edge1 * DeltaU2) / Det;

			Vertices[IndexA].Tangent += FaceTangent;
			Vertices[IndexB].Tangent += FaceTangent;
			Vertices[IndexC].Tangent += FaceTangent;

			Bitangents[IndexA] += FaceBitangent;
			Bitangents[IndexB] += FaceBitangent;
			Bitangents[IndexC] += FaceBitangent;
		}

		for (int32 i = 0; i < VertexCount; ++i)
		{
			TVertex& Vertex = Vertices[i];
			FVector Tangent = Vertex.Tangent.XYZ - Vertex.Normal * FVector::Dot(Vertex.Tangent.XYZ, Vertex.Normal);

			if (Tangent.Size() < 1e-6f)
			{
				const FVector Axis = std::abs(Vertex.Normal.Z) < 0.99f ? FVector(0.0f, 0.0f, 1.0f) : FVector(1.0f, 0.0f, 0.0f);
				Tangent = FVector::Cross(Axis, Vertex.Normal);
			}

			float Sign = (FVector::Dot(FVector::Cross(Vertex.Normal, Tangent), Bitangents[i]) < 0.0f) ? -1.0f : 1.0f;
			Vertex.Tangent = Tangent.Normalized();
			Vertex.Tangent.W = Sign;
		}
	}
}