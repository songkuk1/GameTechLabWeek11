#pragma once

#include "Render/Vertex.h"
#include "StaticMeshData.h"

class FGeometryGenerator
{
public:
	static TUniquePtr<FModelMeshData> CreateTextQuad(const FVector& Start, const FVector& End, const FVector4& Color);
	static TUniquePtr<FModelMeshData> CreateLine(const FVector& Start, const FVector& End, const FVector4& Color);
	static TUniquePtr<FModelMeshData> CreatePlane(float Size, const FVector4& Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f));
	static TUniquePtr<FModelMeshData> CreateAxis();

	static TUniquePtr<FModelMeshData> CreateCone(float Radius, float Height, int Segments, const FVector4& Color);
	static TUniquePtr<FModelMeshData> CreateCylinder(float Radius, float Height, int Segments, const FVector4& Color);
	static TUniquePtr<FModelMeshData> CreateArrow(float BodyRadius, float BodyHeight, float HeadRadius, float HeadHeight, int Segments, const FVector4& Color); // Cylinder + Cone 합성
	static TUniquePtr<FModelMeshData> CreateScaleBar(float BodyRadius, float BodyLength, float HeadSize, float Segments, const FVector4& Color); // Cylinder + Cone 합성
	static TUniquePtr<FModelMeshData> CreateRing(float Radius, float TubeRadius, int Segments, int TubeSegments, const FVector4& Color);
	static TUniquePtr<FModelMeshData> CreateCube(float Size, const FVector4& Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f));

	static TUniquePtr<FModelMeshData> CreateSphere(float _radius, uint32 _numSlices, uint32 _numStacks, const FVector4& Color);

	static FModelMeshData* GetMeshData(const FString& InName);

	static void CreateDefaultMeshDatas();

private:
	inline static TMap<FString, TUniquePtr<FModelMeshData>> MeshDataMap;
};