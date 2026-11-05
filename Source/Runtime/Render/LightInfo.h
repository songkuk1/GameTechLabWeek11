#pragma once

#include "Math/Vector.h"

// Linear
struct FAmbientLightInfo
{
	FVector Color;
	float Padding = 0.0f;
};

struct FDirectionalLightInfo
{
	FVector Color;
	float Padding0 = 0.0f;
	FVector Direction;
	int32 bCastShadow = 0;
	//float Padding1 = 0.0f;
};

struct FPointLightInfo
{
	FVector Color;
	float AttenuationRadius = 0.0f;
	FVector Position;
	int32 ShadowIndex = -1;
	//float Padding = 0.0f;
};

struct FSpotLightInfo
{
	FVector Color;
	float AttenuationRadius = 0.0f;
	FVector Position;
	float CosInnerCone = 1.0f;
	FVector Direction;
	float CosOuterCone = 1.0f;
	int32 ShadowIndex = -1;
	uint32 Padding_s[3];
};

struct FLightingConstants
{
	FAmbientLightInfo Ambient;
	FDirectionalLightInfo Directional;
};

struct FShadowInfo
{
	FMatrix LightViewProjection;	// 64
	uint32 AtlasIndex = 0;			// 4
	uint32 AtlasStartX = 0;			// 4
	uint32 AtlasStartY = 0;			// 4
	uint32 AtlasSize{};				// 4

	float Bias = 0.001f;			// 4
	float NormalBias = 2.0f;
	float Near = 0.f;
	float Far = 1.f;
		
	float LightSize;
	float ProjScale;
	float Padding[2] = {};
};


struct FDirectionalShadowConstants // b5
{
	FMatrix CascadeViewProjection[4]; // 4 Cascade VP Matrices
	FVector4 CascadeSplitSpheres[4]; // xyz: WorldCenter, w: Radius
	FVector4 CascadeOffsets[4]; // 4096 Atlas 2x2 offsets
	FVector4 CascadeBiases; // Cascade depth biases
	int32 NumCascades = 4; 
	float NormalBias = 2.0f;
	uint32 bVisualizeCascades = 0;
	int32 Padding = 0;
};

static_assert(sizeof(FAmbientLightInfo) == 16);
static_assert(sizeof(FDirectionalLightInfo) == 32);
static_assert(sizeof(FPointLightInfo) == 32);
static_assert(sizeof(FSpotLightInfo) == 64);
static_assert(sizeof(FShadowInfo) == 112);
static_assert(sizeof(FLightingConstants) == 48);
static_assert(sizeof(FDirectionalShadowConstants) == 416);
