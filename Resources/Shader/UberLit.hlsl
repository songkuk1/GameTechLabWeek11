#pragma pack_matrix(row_major)

#ifndef HITORI_UBER_LIT_INCLUDED
#define HITORI_UBER_LIT_INCLUDED
#include "ColorBlend.hlsli"
// EViewMode 순서와 일치해야 한다.
#define VIEWMODE_LIT          0
#define VIEWMODE_UNLIT        1
#define VIEWMODE_WORLDNORMAL  3

// ELightingModel 순서와 일치해야 한다.
#define LIGHTING_MODEL_LAMBERT      0
#define LIGHTING_MODEL_PHONG        1
#define LIGHTING_MODEL_BLINNPHONG   2

// EShadingModel 순서와 일치해야 한다.
#define SHADING_MODEL_GOURAUD  0
#define SHADING_MODEL_PHONG    1

// 기본값: Lit, Blinn-Phong, Phong
#ifndef VIEWMODE
#define VIEWMODE VIEWMODE_LIT
#endif

#ifndef LIGHTING_MODEL
#define LIGHTING_MODEL LIGHTING_MODEL_BLINNPHONG
#endif

#ifndef SHADING_MODEL
#define SHADING_MODEL SHADING_MODEL_PHONG
#endif

#ifndef HAS_NORMAL_MAP
#define HAS_NORMAL_MAP 0
#endif
#define USE_NORMAL_MAP (SHADING_MODEL == SHADING_MODEL_PHONG && HAS_NORMAL_MAP)

#ifndef RECEIVE_SHADOW
#define RECEIVE_SHADOW 1
#endif 

#define GOURAUD (VIEWMODE == VIEWMODE_LIT && SHADING_MODEL == SHADING_MODEL_GOURAUD)

// EShadowFilterMode(ShadowFilterSettings.h)와 순서가 같아야 한다.
#define SHADOW_FILTER_HARD   0
#define SHADOW_FILTER_HW_PCF 1
#define SHADOW_FILTER_PCF    2
#define SHADOW_FILTER_PCSS   3

struct FAmbientLightInfo
{
    float3 Color;
    float Padding;
};

struct FDirectionalLightInfo
{
    float3 Color;
    float Padding0;
    float3 Direction;
    int ShadowIndex;
    //float Padding1;
};

struct FPointLightInfo
{
    float3 Color;
    float AttenuationRadius;
    float3 Position;
    int ShadowIndex;
    //float Padding;
};

struct FSpotLightInfo
{
    float3 Color;
    float AttenuationRadius;
    float3 Position;
    float CosInnerCone;
    float3 Direction;
    float CosOuterCone;
    int ShadowIndex;
    float3 Padding_s;
};

struct FShadowInfo
{
    matrix LightViewProjection;
    uint AtlasIndex;
    uint AtlasStartX;
    uint AtlasStartY;
    uint AtlasSize;
    
    float Bias;
    float NormalBias;
    float Near;
    float Far;
    
    float LightSize;
    float ProjScale;
    float2 Padding;
};

struct FVSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Tangent : TANGENT;
    float4 Color : COLOR; // 지금은 쓰지 않지만 나중에 쓰게 될 수 있음
    float2 UV : TEXCOORD;
#if USE_SKINNING
    int4 BoneIndices : BLENDINDICES;
    float4 BoneWeights : BLENDWEIGHT;
    int BoneInfluenceCount : BONECOUNT;
#endif
};

struct FPSInput
{
    float4 Position : SV_POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
    float3 WorldPosition : TEXCOORD1;
#if USE_NORMAL_MAP
    float3 Tangent : TANGENT;
    int BitangentSign : BITANGENT_SIGN;
#endif
#if GOURAUD
    float3 DiffuseLight : TEXCOORD2;
    float3 SpecularLight : TEXCOORD3;
#endif
};

struct FPSOutput
{
    float4 Color : SV_Target0;
    float4 Normal : SV_Target1;
    uint ObjectMask : SV_Target2;
};

cbuffer ViewConstants : register(b0)
{
    matrix ViewProjection;
    float3 CameraPosition;
    float ViewPadding;
};

cbuffer StaticMeshMaterialParams : register(b1)
{
    float4 BaseColor;
    float2 UVScrollOffset;
    uint bOpaque;
    float Shininess;
    float3 SpecularColor;
    uint bFlipNormalGreen;
    uint DecalColorBlendMode;
    uint3 MaterialPadding;
};

cbuffer PerObjectConstants : register(b2)
{
    matrix World;
    matrix NormalMatrix; // World의 역전치
    uint ObjectMask;
    uint3 PerObjectConstantsPadding;
};

cbuffer LightingConstants : register(b3)
{
    FAmbientLightInfo AmbientLight;
    FDirectionalLightInfo DirectionalLight;
};

cbuffer TileLightConstants : register(b4)
{
    matrix TileInverseViewProjection;
    uint TileScreenWidth;
    uint TileScreenHeight;
    uint TileCountX;
    uint TileCountY;
    uint StructuredPointLightCount;
    uint StructuredSpotLightCount;
    uint PointTileStride;
    uint SpotTileStride;
    uint bVisualizeLightCount;
    uint3 TileCullingPadding;
};

cbuffer DirectionalShadowConstants : register(b5)
{
    matrix CascadeViewProjection[4];
    float4 CascadeSplitSpheres[4];
    float4 CascadeOffsets[4];
    float4 CascadeBiases;
    int NumCascades;
    float NormalBias;
    uint bVisualizeCascades;
    float CSMPadding;
};

// Settings 창의 그림자 필터 옵션 (FShadowFilterConstants와 같은 레이아웃)
cbuffer ShadowFilterConstants : register(b6)
{
    uint ShadowFilterMode;
    uint bShadowPoisson;
    uint bShadowGaussian;
    uint bShadowRotate;
    float ShadowPCFRadiusTexels;
    float ShadowMaxFilterTexels;
    float ShadowSearchScale;
    float ShadowFilterPadding;
};

StructuredBuffer<FPointLightInfo> StructuredPointLights : register(t8);
StructuredBuffer<FSpotLightInfo> StructuredSpotLights : register(t9);
StructuredBuffer<uint> PointTileList : register(t10);
StructuredBuffer<uint> SpotTileList : register(t11);
StructuredBuffer<FShadowInfo> StructuredShadowData : register(t12);

#if USE_SKINNING
// StructuredBuffer 행렬의 CPU 행 우선 데이터 해석 보정
StructuredBuffer<float4x4> BoneMatrices : register(t13);
#endif

Texture2D AlbedoTexture : register(t0);

#if USE_NORMAL_MAP
Texture2D NormalTexture : register(t1);
#endif

Texture2DArray<float> ShadowTextureArray : register(t2);
Texture2D<float> DirectionalLightShadowmapAtlas : register(t3);
Texture2D DecalTexture : register(t4);
Texture2D <float4> DecalNormalTexture:register(t5);

SamplerState AlbedoSampler : register(s0);
SamplerState ShadowPointSampler : register(s1);
SamplerComparisonState ShadowCmpSampler : register(s2);

// [-1, 1] 사각형 안의 Poisson Disk 16점 (최대 길이 1.2339). GetFilterTap에서 반지름 1 안으로 정규화한다.
static const float2 Poisson16[16] =
{
    float2(-0.94201624, -0.39906216), float2(0.94558609, -0.76890725),
    float2(-0.09418410, -0.92938870), float2(0.34495938, 0.29387760),
    float2(-0.91588581, 0.45771432), float2(-0.81544232, -0.87912464),
    float2(-0.38277543, 0.27676845), float2(0.97484398, 0.75648379),
    float2(0.44323325, -0.97511554), float2(0.53742981, -0.47373420),
    float2(-0.26496911, -0.41893023), float2(0.79197514, 0.19090188),
    float2(-0.24188840, 0.99706507), float2(-0.81409955, 0.91437590),
    float2(0.19984126, 0.78641367), float2(0.14383161, -0.14100790)
};

static const float PoissonInvMaxLen = 1.0f / 1.2339f;
// Gaussian 가중치 exp(-r^2 * K). 탭이 반지름 1 기준이라 K = 1 / (2σ²), σ = 0.5
static const float ShadowGaussK = 2.0f;

float3 DiffuseTerm(float3 Il, float3 N, float3 L)
{
    return Il * max(dot(N, L), 0.0f);
}

// Phong: A(R), B(V) Blinn-Phong: A(N), B(H)
float3 SpecularTerm(float3 Il, float3 A, float3 B, float n)
{
    return Il * pow(max(dot(A, B), 0.0f), n);
}

float ComputeDistanceAttenuation(float Distance, float Radius)
{
    return pow(saturate(1.0f - pow(Distance / Radius, 4)), 2);
}

void AccumulateLight(float3 Il, float3 N, float3 L, float3 V, float n, inout float3 DiffuseLight, inout float3 SpecularLight)
{
    float NdotL = max(dot(N, L), 0.0f);
    DiffuseLight += Il * NdotL; // Lambert
    if (NdotL > 0.0f)
    {
#if LIGHTING_MODEL == LIGHTING_MODEL_PHONG
        float3 R = 2.0f * dot(N, L) * N - L;
        // ★ NdotL을 곱해 빛이 스치는 어두운 곳에서는 스펙큘러가 0으로 부드럽게 사라지게 함
        SpecularLight += SpecularTerm(Il, R, V, n) * NdotL;
#elif LIGHTING_MODEL == LIGHTING_MODEL_BLINNPHONG
        float3 H = normalize(L + V);
        SpecularLight += SpecularTerm(Il, N, H, n) * NdotL;
#endif
    }
}

float3 GetLightCountColor(uint LightCount)
{
    if (LightCount == 0u)
        return float3(0.0f, 0.0f, 0.0f);
    if (LightCount <= 4u)
        return float3(0.0f, 0.0f, 1.0f);
    if (LightCount <= 16u)
        return float3(0.0f, 1.0f, 0.0f);
    if (LightCount <= 32u)
        return float3(1.0f, 1.0f, 0.0f);
    if (LightCount <= 64u)
        return float3(1.0f, 0.5f, 0.0f);
    return float3(1.0f, 0.0f, 0.0f);
}

// 일단 GOURAUD 전용
void ComputeLighting(float3 N, float3 WorldPosition, float3 V, float n, out float3 DiffuseLight, out float3 SpecularLight)
{
    DiffuseLight = 0.0f;
    SpecularLight = 0.0f;

    // Directional Light
    {
        float3 Il = DirectionalLight.Color;
        float3 L = -DirectionalLight.Direction;
        float GeoFactor = 1.0f;
        AccumulateLight(Il * GeoFactor, N, L, V, n, DiffuseLight, SpecularLight);
    }
}

// 라이트에서 픽셀 방향에서 절댓값이 가장 큰 축이 그 픽셀이 들어 있는 면이다
uint SelectCubeFace(float3 LightToPixel)
{
    float3 A = abs(LightToPixel);
    if (A.x >= A.y && A.x >= A.z)
         // Forward, Back
        return LightToPixel.x > 0 ? 0 : 1;
    if (A.y >= A.z)
        // Right, Left
        return LightToPixel.y > 0 ? 2 : 3;
    // Up, Down
    return LightToPixel.z > 0 ? 4 : 5;
}

//비선형인(NDC.z인) ShadowMap 샘플링 뎁스값을 선형으로 만듭니다.
float LinearizeShadowDepth(float depth, float Near, float Far)
{
    //Projection의 ZScale = F / F-N
    float ZScale = Far / (Far - Near);
    
    //ZOffset = -NF / F - N
    float ZOffset = (-Near * Far) / (Far - Near);
    
    //            [0      0       ZScale  1]
    //[x y z 1] * [XScale 0       0       0]
    //            [0      YScale  0       0]
    //            [0      0       ZOffset 0]
    
    //depth(NDC.z) = Clip.z / Clip.w
    //              = (ZScale * x + ZOffset) / x
    //x = ZOffset / (depth - ZScale)
    return ZOffset / (depth - ZScale);
}

// Interleaved Gradient Noise 픽셀 좌표 → 0~1
float IGN(float2 PixelPos)
{
    return frac(52.9829189f * frac(dot(PixelPos, float2(0.06711056f, 0.00583715f))));
}

float BasicShadow(float2 UV, float TextureIndex, float Ref, float2 TileMin, float2 TileMax)
{
    float2 SampleUV = clamp(UV, TileMin, TileMax);
    float StoredDepth = ShadowTextureArray.SampleLevel(ShadowPointSampler, float3(SampleUV, TextureIndex), 0);
    return Ref > StoredDepth ? 0.0f : 1.0f;
}

float PCF_CmpLinear(float2 UV, float TextureIndex, float Ref, float2 TileMin, float2 TileMax)
{
    float2 SampleUV = clamp(UV, TileMin, TileMax);
    //비교 샘플러 사용
    float StoredDepth = ShadowTextureArray.SampleCmpLevelZero(ShadowCmpSampler, float3(SampleUV, TextureIndex), Ref);
    return StoredDepth;
}

float PCF_Software3x3(float2 TexelSize, float2 UV, float TextureIndex, float Ref, float2 TileMin, float2 TileMax)
{
    //3x3 Software PCF
    //해상도 512로 일단 하드코딩
    float Lit;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            float2 SampleUV = clamp(UV + float2(x, y) * TexelSize, TileMin, TileMax);
            
            float Stored = ShadowTextureArray.SampleLevel(ShadowPointSampler, float3(SampleUV, TextureIndex), 0);
            Lit += (Ref) > Stored ? 0.0 : 1.0;
        }
    }
    Lit /= 9.0;
    return Lit;   
}

float PCF_Software5x5(float2 TexelSize, float2 UV, float TextureIndex, float Ref, float2 TileMin, float2 TileMax)
{
    //5x5 Software PCF
    float Lit = 0;
    for (int y = -2; y <= 2; ++y)
    {
        for (int x = -2; x <= 2; ++x)
        {
            float2 SampleUV = clamp(UV + float2(x, y) * TexelSize, TileMin, TileMax);
            float Stored = ShadowTextureArray.SampleLevel(ShadowPointSampler, float3(SampleUV, TextureIndex), 0);
            Lit += (Ref) > Stored ? 0.0 : 1.0;
        }
    }
    Lit /= 25.0;
    return Lit;
}

float PCF_CmpLinear_Software3x3(float2 TexelSize, float2 UV, float TextureIndex, float Ref, float2 TileMin, float2 TileMax)
{
    //Comparison Linear + 3x3 Software PCF
    float Lit = 0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            float2 SampleUV = clamp(UV + float2(x, y) * TexelSize, TileMin, TileMax);
            float Stored = ShadowTextureArray.SampleCmpLevelZero(ShadowCmpSampler, float3(SampleUV, TextureIndex), Ref);
            Lit += Stored;
        }
    }
    Lit /= 9.0;
    return Lit;
}

// 다중 탭 필터 공용 함수 (PCF, PCSS)
// 탭 위치는 반지름 1 기준이고, 호출하는 쪽에서 반경(아틀라스 UV)을 곱한다.

// 탭 수: Poisson 16, 격자 5x5 = 25
uint GetShadowTapCount()
{
    return bShadowPoisson != 0u ? 16u : 25u;
}

// i번째 탭 위치. Poisson은 반지름 1 안으로 정규화 후 회전, 격자는 [-1, 1] 사각형의 5x5 (회전 없음)
float2 GetFilterTap(uint Index, float2x2 Rot)
{
    // 격자 경로(Index 최대 24)에서도 Poisson16 배열 밖을 읽지 않도록 % 16
    float2 PoissonTap = mul(Rot, Poisson16[Index % 16u] * PoissonInvMaxLen);
    float2 GridTap = float2((int)(Index % 5u) - 2, (int)(Index / 5u) - 2) * 0.5f;
    return bShadowPoisson != 0u ? PoissonTap : GridTap;
}

// 픽셀별 회전 행렬. Poisson + Rotation일 때만 회전하고, 아니면 단위 행렬
float2x2 GetShadowRotation(float2 PixelPos)
{
    float Angle = (bShadowPoisson != 0u && bShadowRotate != 0u) ? IGN(PixelPos) * 6.2831853f : 0.0f;
    float s, c;
    sincos(Angle, s, c);
    return float2x2(c, -s, s, c);
}

// 반경(아틀라스 UV) 안에서 비교 샘플링한 평균. Gaussian이 켜지면 중심에 가까운 탭에 큰 가중치
float FilterShadow(float2 UV, float TextureIndex, float Ref, float RadiusUV, float2 TileMin, float2 TileMax, float2x2 Rot)
{
    float Sum = 0.0f;
    float WeightSum = 0.0f;
    uint Count = GetShadowTapCount();

    [loop]
    for (uint i = 0; i < Count; ++i)
    {
        float2 Tap = GetFilterTap(i, Rot);
        float Weight = bShadowGaussian != 0u ? exp(-dot(Tap, Tap) * ShadowGaussK) : 1.0f;
        float2 TapUV = clamp(UV + Tap * RadiusUV, TileMin, TileMax); // 타일(면) 밖으로 나가지 않게
        Sum += Weight * ShadowTextureArray.SampleCmpLevelZero(ShadowCmpSampler, float3(TapUV, TextureIndex), Ref);
        WeightSum += Weight;
    }
    return Sum / max(WeightSum, 1e-5f);
}

// PCSS: Blocker Search → 반그림자 폭 → 가변 반경 필터
float PCSS(float2 TexelSize, float2 UV, float TextureIndex, float ReceiverNdcZ, FShadowInfo Info, float2 TileMin, float2 TileMax, float2x2 Rot)
{
    const float Ref = ReceiverNdcZ - Info.Bias;

    // 광원부터 Receiver까지의 선형 거리
    float dR = LinearizeShadowDepth(ReceiverNdcZ, Info.Near, Info.Far);

    // 타일 UV(타일 전체 = 1) 길이를 아틀라스 UV 길이로 바꾸는 비율
    float TileToAtlas = Info.AtlasSize * TexelSize.x;
    float MaxFilterUV = TexelSize.x * ShadowMaxFilterTexels;

    // 1) Blocker Search
    //    Near 평면 기준의 보수적인 탐색 반경. 상한은 필터 상한보다 넓게 잡아야
    //    반그림자 바깥 끝 픽셀이 Blocker를 놓치고 1.0으로 끊기지 않는다.
    float SearchWorld = Info.LightSize * (dR - Info.Near) / dR;
    float SearchUV = SearchWorld * Info.ProjScale / Info.Near * TileToAtlas;
    SearchUV = min(SearchUV, MaxFilterUV * ShadowSearchScale);

    float dB = 0.0f;
    float BlockerCount = 0.0f;
    uint Count = GetShadowTapCount();

    [loop]
    for (uint i = 0; i < Count; ++i)
    {
        // Blocker의 깊이 값 자체가 필요하므로 Point 샘플러로 읽는다
        float2 SampleUV = clamp(UV + GetFilterTap(i, Rot) * SearchUV, TileMin, TileMax);
        float BlockerDepth = ShadowTextureArray.SampleLevel(ShadowPointSampler, float3(SampleUV, TextureIndex), 0);

        // Receiver보다 가까운 텍셀(Blocker)만 선형화해서 평균에 넣는다
        if (BlockerDepth < Ref)
        {
            dB += LinearizeShadowDepth(BlockerDepth, Info.Near, Info.Far);
            BlockerCount += 1.0f;
        }
    }

    // 주변에 Blocker가 없으면 빛을 받는다
    if (BlockerCount == 0.0f)
        return 1.0f;

    dB /= BlockerCount;

    // 2) 반그림자 반경 (월드): P = LightSize * (dR - dB) / dB  → 깊이 dR에서의 아틀라스 UV로
    float P = Info.LightSize * (dR - dB) / dB;
    float FilterUV = P * Info.ProjScale / dR * TileToAtlas;
    FilterUV = clamp(FilterUV, TexelSize.x * 0.5f, MaxFilterUV);

    // 3) 가변 반경 필터
    return FilterShadow(UV, TextureIndex, Ref, FilterUV, TileMin, TileMax, Rot);
}

// 반환값: 1 = 빛 받음, 0 = 그림자. 필터 방식은 Settings의 ShadowFilterMode(b6)로 고른다.
float ComputeShadowFactor(float ShadowIndex, float3 WorldPosition, float2 PixelPos)
{    
    //-1이면 그림자 없는 라이트
    if (ShadowIndex < 0)
        return 1.0f;
    
#if (RECEIVE_SHADOW == 0)
        return 1.0f;
#endif
    FShadowInfo Info = StructuredShadowData[ShadowIndex];

    // light Clip Space 클립 공간
    float4 LightClip = mul(float4(WorldPosition, 1.0f), Info.LightViewProjection);
    // 광원 뒤쪽
    if (LightClip.w <= 0.0f)
        return 1.0f; 

    //원근 나눗셈은 PS에서 (VS에서 나누고 보간하면 원근 보정이 깨짐)
    //Receiver의 NDC 좌표 = Light 기준의 Depth를 구할 수 있다.
    float3 Ndc = LightClip.xyz / LightClip.w;

    //NDC -> UV. D3D 텍스처는 v가 아래로 증가하므로 y를 뒤집는다
    float2 UV = float2(Ndc.x * 0.5f + 0.5f, 1.0f - (Ndc.y * 0.5f + 0.5f));

    //범위 검사: 섀도맵 밖이거나 Far 너머면 그림자 판정 불가 : 빛 받음
    if (any(UV < 0.0f) || any(UV > 1.0f) || Ndc.z < 0.0f || Ndc.z > 1.0f)
        return 1.0f;
    
    // 아틀라스 UV 구하는 로직
    float AtlasWidth, AtlasHeight, NumPages;
    ShadowTextureArray.GetDimensions(AtlasWidth, AtlasHeight, NumPages);
    
    float2 AtlasUV = (float2(Info.AtlasStartX, Info.AtlasStartY) + UV * Info.AtlasSize) / float2(AtlasWidth, AtlasHeight);
    
    float2 TexelSize = 1.0 / AtlasWidth;
    float2 TileMin = (float2(Info.AtlasStartX, Info.AtlasStartY) + 0.5f) * TexelSize;
    float2 TileMax = (float2(Info.AtlasStartX, Info.AtlasStartY) + Info.AtlasSize - 0.5f) * TexelSize;

    const float Ref = Ndc.z - Info.Bias;

    [branch]
    switch (ShadowFilterMode)
    {
    case SHADOW_FILTER_HARD:
        // Point 샘플러 1탭
        return BasicShadow(AtlasUV, Info.AtlasIndex, Ref, TileMin, TileMax);

    case SHADOW_FILTER_HW_PCF:
        // Comparison LINEAR 1탭 (하드웨어 2x2)
        return PCF_CmpLinear(AtlasUV, Info.AtlasIndex, Ref, TileMin, TileMax);

    case SHADOW_FILTER_PCF:
        // 고정 반경. 격자/Poisson, Gaussian, 회전은 체크박스로
        return FilterShadow(AtlasUV, Info.AtlasIndex, Ref, ShadowPCFRadiusTexels * TexelSize.x,
                            TileMin, TileMax, GetShadowRotation(PixelPos));

    default:
        // PCSS
        return PCSS(TexelSize, AtlasUV, Info.AtlasIndex, Ndc.z, Info, TileMin, TileMax, GetShadowRotation(PixelPos));
    }
}

int GetCascadeIndex(float3 WorldPosition)
{
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float3 ToCenter = WorldPosition - CascadeSplitSpheres[i].xyz;
        float DistSq = dot(ToCenter, ToCenter);
        float RadiusSq = CascadeSplitSpheres[i].w * CascadeSplitSpheres[i].w;
        if (DistSq < RadiusSq)
        {
            return i;
        }
    }
    return 4; // Bounding Sphere를 벗어남 (그림자 최대 거리 초과)
}

// 단일 캐스케이드의 그림자 판정 (0.0: 그림자, 1.0: 밝음)
// Directional Light (Texture2D Atlas) 필터링 함수
float Directional_BasicShadow(float2 UV, float CompareValue, float2 TileMin, float2 TileMax)
{
    float2 SampleUV = clamp(UV, TileMin, TileMax);
    float StoredDepth = DirectionalLightShadowmapAtlas.SampleLevel(ShadowPointSampler, SampleUV, 0);
    return CompareValue > StoredDepth ? 0.0f : 1.0f;
}

float Directional_HW_PCF(float2 UV, float CompareValue, float2 TileMin, float2 TileMax)
{
    float2 SampleUV = clamp(UV, TileMin, TileMax);
    return DirectionalLightShadowmapAtlas.SampleCmpLevelZero(ShadowCmpSampler, SampleUV, CompareValue);
}

float Directional_FilterShadow(float2 UV, float CompareValue, float RadiusUV, float2 TileMin, float2 TileMax, float2x2 Rot)
{
    float Sum = 0.0f;
    float WeightSum = 0.0f;
    uint Count = GetShadowTapCount();

    [loop]
    for (uint i = 0; i < Count; ++i)
    {
        float2 Tap = GetFilterTap(i, Rot);
        float Weight = bShadowGaussian != 0u ? exp(-dot(Tap, Tap) * ShadowGaussK) : 1.0f;
        float2 TapUV = clamp(UV + Tap * RadiusUV, TileMin, TileMax);
        Sum += Weight * DirectionalLightShadowmapAtlas.SampleCmpLevelZero(ShadowCmpSampler, TapUV, CompareValue);
        WeightSum += Weight;
    }
    return Sum / max(WeightSum, 1e-5f);
}

// Directional Light (CSM Orthographic) PCSS:
// Blocker Search -> Contact-Hardening Penumbra -> Variable Radius Filter
float Directional_PCSS(float2 TexelSize, float2 AtlasUV, float ReceiverNdcZ, float CompareValue, float2 TileMin, float2 TileMax, float2x2 Rot)
{
    float MaxFilterUV = TexelSize.x * ShadowMaxFilterTexels;
    float SearchUV = min(MaxFilterUV * ShadowSearchScale, (TileMax.x - TileMin.x) * 0.5f);

    // 1) Blocker Search
    float dB = 0.0f;
    float BlockerCount = 0.0f;
    uint Count = GetShadowTapCount();

    [loop]
    for (uint i = 0; i < Count; ++i)
    {
        float2 SampleUV = clamp(AtlasUV + GetFilterTap(i, Rot) * SearchUV, TileMin, TileMax);
        float BlockerDepth = DirectionalLightShadowmapAtlas.SampleLevel(ShadowPointSampler, SampleUV, 0);

        if (BlockerDepth < CompareValue)
        {
            dB += BlockerDepth;
            BlockerCount += 1.0f;
        }
    }

    if (BlockerCount == 0.0f)
        return 1.0f;

    dB /= BlockerCount;

    // 2) Penumbra Size 계산 (Contact Hardening)
    // 직교 투영(평행광)에서 차폐체(dB)와 수신기(ReceiverNdcZ) 깊이 차이에 비례하여 반그림자 확장
    float DepthDiff = max(ReceiverNdcZ - dB, 0.0f);
    float PenumbraRatio = saturate(DepthDiff / max(CompareValue, 1e-4f));
    float FilterUV = clamp(PenumbraRatio * MaxFilterUV, TexelSize.x * 0.5f, MaxFilterUV);

    // 3) 가변 반경 필터링
    return Directional_FilterShadow(AtlasUV, CompareValue, FilterUV, TileMin, TileMax, Rot);
}

float SampleSingleCascadeShadow(float3 WorldPosition, float3 Normal, int CascadeIndex, float2 PixelPos)
{
    float3 L = -DirectionalLight.Direction;

    // 1. 빛과 표면 사이의 각도 계산 (sin(theta) 기반 경사각 스케일링, 수직 입사 시에도 최소 0.5 텍셀 오프셋을 보장하여 평면 아크네 방지)
    float NdotL = saturate(dot(Normal, L));
    float NormalOffsetScale = clamp(sqrt(1.0f - NdotL * NdotL), 0.5f, 1.5f);

    float AtlasWidth, AtlasHeight;
    DirectionalLightShadowmapAtlas.GetDimensions(AtlasWidth, AtlasHeight);
    float CascadeRes = AtlasWidth * 0.5f;

    // 텍셀의 월드 공간 크기 = (2 * Radius) / CascadeResolution
    float WorldUnitsPerTexel = (2.0f * CascadeSplitSpheres[CascadeIndex].w) / CascadeRes;

    // 2. 표면 노멀 방향으로 월드 좌표를 이동 (NormalBias는 텍셀 배수 단위로 작동하여 캐스케이드별 최적 바이어스 자동 적용)
    float EffectiveNormalBias = NormalBias * WorldUnitsPerTexel;
    float3 ShadowWorldPos = WorldPosition + Normal * (EffectiveNormalBias * NormalOffsetScale);

    // 3. 선택된 캐스케이드의 VP 행렬로 Light Clip Space 계산
    float4 LightClip = mul(float4(ShadowWorldPos, 1.0f), CascadeViewProjection[CascadeIndex]);
    if (LightClip.w <= 0.0f)
        return 1.0f;

    float3 Ndc = LightClip.xyz / LightClip.w;
    float2 BaseUV = float2(Ndc.x * 0.5f + 0.5f, 1.0f - (Ndc.y * 0.5f + 0.5f));

    if (any(BaseUV < 0.0f) || any(BaseUV > 1.0f) || Ndc.z < 0.0f || Ndc.z > 1.0f)
        return 1.0f;

    // 4. 4096 Atlas의 2x2 쿼드런트 오프셋 적용
    float4 Offset = CascadeOffsets[CascadeIndex];
    float2 AtlasUV = BaseUV * Offset.xy + Offset.zw;

    float2 TexelSize = 1.0f / float2(AtlasWidth, AtlasHeight);

    // 쿼드런트 아틀라스 경계 클램핑 (인접 캐스케이드 사분면 침범 방지)
    float2 TileMin = Offset.zw + 0.5f * TexelSize;
    float2 TileMax = Offset.zw + Offset.xy - 0.5f * TexelSize;

    float DepthBias = CascadeBiases[CascadeIndex];
    float CompareValue = Ndc.z - DepthBias;

    // 5. 3x3 PCF 필터링 (하드웨어 비교 샘플러 ShadowCmpSampler 사용)
    // 5. 그림자 필터링 (Hard / HW_PCF / PCF / PCSS)
    [branch]
    switch (ShadowFilterMode)
    {
    case SHADOW_FILTER_HARD:
        return Directional_BasicShadow(AtlasUV, CompareValue, TileMin, TileMax);

    case SHADOW_FILTER_HW_PCF:
        return Directional_HW_PCF(AtlasUV, CompareValue, TileMin, TileMax);

    case SHADOW_FILTER_PCF:
        return Directional_FilterShadow(AtlasUV, CompareValue, ShadowPCFRadiusTexels * TexelSize.x,
                                        TileMin, TileMax, GetShadowRotation(PixelPos));

    default:
        // PCSS
        return Directional_PCSS(TexelSize, AtlasUV, Ndc.z, CompareValue, TileMin, TileMax, GetShadowRotation(PixelPos));
    }
}

// 캐스케이드 인덱스 및 경계면 블렌딩 정보 계산
void GetCascadeBlendInfo(float3 WorldPosition, out int CascadeIndex, out int NextCascadeIndex, out float BlendFactor)
{
    CascadeIndex = GetCascadeIndex(WorldPosition);
    NextCascadeIndex = min(CascadeIndex + 1, NumCascades);
    BlendFactor = 0.0f;

    // 마지막 캐스케이드가 아닌 경우 경계면 15% 구간에서 다음 캐스케이드와 블렌딩 계수 계산
    if (CascadeIndex < NumCascades - 1)
    {
        float3 ToCenter = WorldPosition - CascadeSplitSpheres[CascadeIndex].xyz;
        float Dist = length(ToCenter);
        float Radius = CascadeSplitSpheres[CascadeIndex].w;

        const float BlendRange = 0.15f; // 외곽 15% 영역을 블렌딩 구간으로 설정
        float BlendStart = Radius * (1.0f - BlendRange);

        if (Dist > BlendStart)
        {
            BlendFactor = saturate((Dist - BlendStart) / (Radius - BlendStart));
        }
    }
}

// Directional Light 전용 그림자 판정
float ComputeDirectionalShadowFactor(float3 WorldPosition, float3 Normal, float2 PixelPos)
{
    if (NumCascades <= 0)
        return 1.0f;
    
    #if (RECEIVE_SHADOW == 0)
        return 1.0f;
    #endif
        int CascadeIndex;
        int NextCascadeIndex;
        float BlendFactor;
        GetCascadeBlendInfo(WorldPosition, CascadeIndex, NextCascadeIndex, BlendFactor);

        if (CascadeIndex >= NumCascades)
            return 1.0f;

        float Shadow = SampleSingleCascadeShadow(WorldPosition, Normal, CascadeIndex, PixelPos);

        // 블렌딩 영역에 걸쳐 있다면 다음 캐스케이드와 lerp
        if (BlendFactor > 0.0f && NextCascadeIndex < NumCascades)
        {
            float NextShadow = SampleSingleCascadeShadow(WorldPosition, Normal, NextCascadeIndex, PixelPos);
            Shadow = lerp(Shadow, NextShadow, BlendFactor);
        }

        return Shadow;
}

void ComputeTiledLighting(float3 N, float3 GeoNormal, float3 WorldPosition, float3 V,
    float n, float2 PixelPosition, out float3 DiffuseLight, out float3 SpecularLight){
    DiffuseLight = 0.0f;
    SpecularLight = 0.0f;

    // 화면 전체에 영향을 주는 Directional Light는 타일 목록에 넣지 않음
    {
        float3 Il = DirectionalLight.Color;
        float3 L = -DirectionalLight.Direction;
#if USE_NORMAL_MAP
        float GeoFactor = saturate(dot(GeoNormal, L) * 4.0f);
#else
        float GeoFactor = 1.0f;
#endif
        // 데칼 요철과 분리된 기하 노멀 기준 그림자 바이어스
        float3 ShadowNormal = GeoNormal;
        float Shadow = ComputeDirectionalShadowFactor(WorldPosition, ShadowNormal, PixelPosition);
        
        AccumulateLight(Il * GeoFactor * Shadow, N, L, V, n, DiffuseLight, SpecularLight);
    }

    uint2 TileCoord = min(uint2(PixelPosition) / 16u, uint2(TileCountX - 1u, TileCountY - 1u));
    uint TileIndex = TileCoord.y * TileCountX + TileCoord.x;
    
    // Poisson 픽셀 회전은 ComputeShadowFactor 안에서 Settings(b6)에 따라 PixelPosition으로 계산한다.

    // Point Light
    uint PointListStart = TileIndex * PointTileStride;
    uint PointCount = min(PointTileList[PointListStart], 64u);
    for (uint ListIndex = 0; ListIndex < PointCount; ++ListIndex)
    {
        uint LightIndex = PointTileList[PointListStart + 1u + ListIndex];
        if (LightIndex >= StructuredPointLightCount)
            continue;

        FPointLightInfo Light = StructuredPointLights[LightIndex];
        float3 ToLight = Light.Position - WorldPosition;
        float Distance = length(ToLight);
        float Radius = Light.AttenuationRadius;
        if (Distance <= 0.0f || Distance >= Radius)
            continue;

        float3 L = ToLight / Distance;
#if USE_NORMAL_MAP
        float GeoFactor = saturate(dot(GeoNormal, L) * 4.0f);
        if (GeoFactor <= 0.0f)
            continue;
#else
        float GeoFactor = 1.0f;
#endif

        float DistanceAttenuation = ComputeDistanceAttenuation(Distance, Radius);
        
        //선택할 ShadowIndex
        uint PointShadowIndex = SelectCubeFace(-ToLight);
        //spot light는 ComputeShadowFactor()내부에서 검사로 충분하지만 PointLight는 PointShadowIndex를 더해버리면서
        //ShadowIndex -1이 양수가 될 수 있다. 그래서 여기서도 한번 검사해줌.
        float Shadow = Light.ShadowIndex < 0 ? 1.0 : 
        ComputeShadowFactor(Light.ShadowIndex + PointShadowIndex, WorldPosition, PixelPosition);
        
        float3 Il = Light.Color * DistanceAttenuation * GeoFactor * Shadow;
        AccumulateLight(Il, N, L, V, n, DiffuseLight, SpecularLight);
    }

    // Spot Light
    uint SpotListStart = TileIndex * SpotTileStride;
    uint SpotCount = min(SpotTileList[SpotListStart], 64u);
    for (uint SpotListIndex = 0; SpotListIndex < SpotCount; ++SpotListIndex)
    {
        uint LightIndex = SpotTileList[SpotListStart + 1u + SpotListIndex];
        if (LightIndex >= StructuredSpotLightCount)
            continue;

        FSpotLightInfo Light = StructuredSpotLights[LightIndex];
        float3 ToLight = Light.Position - WorldPosition;
        float Distance = length(ToLight);
        float Radius = Light.AttenuationRadius;
        if (Distance <= 0.0f || Distance >= Radius)
            continue;

        float3 L = ToLight / Distance;
#if USE_NORMAL_MAP
        float GeoFactor = saturate(dot(GeoNormal, L) * 4.0f);
        if (GeoFactor <= 0.0f)
            continue;
#else
        float GeoFactor = 1.0f;
#endif
        //그림자 판단
        float Shadow = ComputeShadowFactor(Light.ShadowIndex, WorldPosition, PixelPosition);
        
        float DistanceAttenuation = ComputeDistanceAttenuation(Distance, Radius);
        float CosAngle = dot(-L, normalize(Light.Direction));
        float SpotAttenuation = Light.CosInnerCone == Light.CosOuterCone
            ? (CosAngle > Light.CosOuterCone ? 1.0f : 0.0f)
            : saturate((CosAngle - Light.CosOuterCone) / (Light.CosInnerCone - Light.CosOuterCone));

        float3 Il = Light.Color * DistanceAttenuation * SpotAttenuation * GeoFactor * Shadow;
        AccumulateLight(Il, N, L, V, n, DiffuseLight, SpecularLight);
    }
}

// 스키닝을 마친 로컬 공간 정점. 스키닝이 없으면 입력 그대로다
struct FLocalVertex
{
    float3 Position;
    float3 Normal;
    float3 Tangent;
};

FLocalVertex GetLocalVertex(FVSInput Input)
{
    FLocalVertex Vertex;
    Vertex.Position = Input.Position;
    Vertex.Normal = Input.Normal;
    Vertex.Tangent = Input.Tangent.xyz;

#if USE_SKINNING
    if (Input.BoneInfluenceCount == 0)
    {
        return Vertex;
    }

    // 노멀·탄젠트도 위치와 같은 본 행렬(회전·스케일 부분)로 섞어야 굽힌 자세에서 음영이 따라온다
    float3 SkinnedPosition = float3(0, 0, 0);
    float3 SkinnedNormal = float3(0, 0, 0);
    float3 SkinnedTangent = float3(0, 0, 0);
    for (int i = 0; i < Input.BoneInfluenceCount; i++)
    {
        int BoneIndex = Input.BoneIndices[i];
        float BoneWeight = Input.BoneWeights[i];
        float4x4 BoneMatrix = transpose(BoneMatrices[BoneIndex]);
        SkinnedPosition += mul(float4(Input.Position, 1.0), BoneMatrix).xyz * BoneWeight;
        SkinnedNormal += mul(Input.Normal, (float3x3) BoneMatrix) * BoneWeight;
        SkinnedTangent += mul(Input.Tangent.xyz, (float3x3) BoneMatrix) * BoneWeight;
    }
    Vertex.Position = SkinnedPosition;
    Vertex.Normal = normalize(SkinnedNormal);
    Vertex.Tangent = normalize(SkinnedTangent);
#endif
    return Vertex;
}

float3 GetLocalVertexPosition(FVSInput Input)
{
    return GetLocalVertex(Input).Position;
}

float3 ApplyDecalNormal(float3 GeoNormal, float3 SurfaceNormal, int2 PixelCoord)
{
    float4 DecalData = DecalNormalTexture.Load(int3(PixelCoord, 0));
    float3 TargetRaw = GeoNormal * (1.0f - saturate(DecalData.a)) + DecalData.rgb;
    float TargetLengthSquared = dot(TargetRaw, TargetRaw);
    if (DecalData.a <= 0.0f || TargetLengthSquared <= 1.0e-8f)
    {
        return SurfaceNormal;
    }

    float3 TargetNormal = TargetRaw * rsqrt(TargetLengthSquared);
    float CosAngle = clamp(dot(GeoNormal, TargetNormal), -1.0f, 1.0f);
    if (CosAngle < -0.9999f)
    {
        // 반대 방향의 안정적인 반회전 축 선택
        float3 ReferenceAxis = abs(GeoNormal.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(0.0f, 1.0f, 0.0f);
        float3 RotationAxis = normalize(cross(GeoNormal, ReferenceAxis));
        return normalize(2.0f * RotationAxis * dot(RotationAxis, SurfaceNormal) - SurfaceNormal);
    }

    // 기하 노멀에서 데칼 방향으로 향하는 최소 회전
    float3 RotationAxis = cross(GeoNormal, TargetNormal);
    float3 FirstCross = cross(RotationAxis, SurfaceNormal);
    float3 RotatedNormal = SurfaceNormal + FirstCross + cross(RotationAxis, FirstCross) / (1.0f + CosAngle);
    return normalize(RotatedNormal);
}

// 표면 진입점을 제외하는 라이브러리 사용 경계
#ifndef UBER_LIT_LIBRARY_ONLY

FPSInput mainVS(FVSInput Input)
{
    FPSInput Output;

    const FLocalVertex Local = GetLocalVertex(Input);
    float4 WorldPosition = mul(float4(Local.Position, 1.0f), World);

    Output.Position = mul(WorldPosition, ViewProjection);
    Output.WorldPosition = WorldPosition.xyz;
    Output.Normal = mul(Local.Normal, (float3x3) NormalMatrix);
    Output.UV = Input.UV;
#if USE_NORMAL_MAP
    Output.Tangent = mul(Local.Tangent, (float3x3) World);
    Output.BitangentSign = Input.Tangent.w;
#endif
#if GOURAUD
    float3 N = normalize(Output.Normal);
    float3 V = normalize(CameraPosition - Output.WorldPosition);
    // TODO
    ComputeLighting(N, Output.WorldPosition, V, Shininess, Output.DiffuseLight, Output.SpecularLight);
    
#endif
    return Output;
}

FPSOutput mainPS(FPSInput Input)
{
    FPSOutput Output;
    
    float4 FinalColor = float4(0.0f, 0.0f, 0.0f, 1.0f);

    float4 TextureColor = AlbedoTexture.Sample(AlbedoSampler, Input.UV + UVScrollOffset);
    float4 DecalColor = DecalTexture.Load(int3(Input.Position.xy, 0));
    float4 SurfaceColor = TextureColor * BaseColor;
    SurfaceColor.rgb = BlendPremultipliedColor(SurfaceColor.rgb, DecalColor.rgb, DecalColor.a, DecalColorBlendMode);
    
    float3 Albedo = SurfaceColor.rgb;
    float Alpha = bOpaque ? 1.0f : SurfaceColor.a;

    float3 Kd = Albedo;
    float3 Ka = Kd; // 나중에 Ka 를 개별로 설정할 수 있음
    float3 Ks = SpecularColor;
    
    float3 Normal = normalize(Input.Normal);
    
#if USE_NORMAL_MAP
    // 그람 슈미트 직교화
    float3 Tangent = normalize(Input.Tangent - dot(Input.Tangent, Normal) * Normal);
    float3 BiTangent = cross(Normal, Tangent) * Input.BitangentSign;
    float3x3 TBN = float3x3(Tangent, BiTangent, Normal);
    // Texture Normal 매핑
    float3 NormalSample = NormalTexture.Sample(AlbedoSampler, Input.UV + UVScrollOffset).xyz;
    float3 TextureNormal = normalize(NormalSample * 2.0f - 1.0f);
    TextureNormal.y = bFlipNormalGreen ? -TextureNormal.y : TextureNormal.y;
    // TBN 행렬을 이용하여 월드 공간으로 변환
    float3 WorldNormal = mul(TextureNormal, TBN);
#else
    float3 WorldNormal = Normal;
#endif
#if SHADING_MODEL == SHADING_MODEL_PHONG || VIEWMODE == VIEWMODE_WORLDNORMAL
    // 픽셀 조명과 노멀 시각화를 위한 데칼 요철 합성
    WorldNormal = ApplyDecalNormal(Normal, WorldNormal, int2(Input.Position.xy));
#endif
#if VIEWMODE == VIEWMODE_UNLIT
    FinalColor = float4(Albedo, Alpha);
#elif VIEWMODE == VIEWMODE_WORLDNORMAL
    // 색이 아니라 데이터이므로 Gamma Encode Pass에서 인코딩하지 않는다.
    FinalColor = float4(normalize(WorldNormal) * 0.5f + 0.5f, 1.0f);
#else // Lit
    if (bVisualizeLightCount != 0u)
    {
        uint2 PixelCoord = uint2(Input.Position.xy);
        uint2 TileCoord = min(PixelCoord / 16u, uint2(TileCountX - 1u, TileCountY - 1u));
        uint TileIndex = TileCoord.y * TileCountX + TileCoord.x;
        
        uint PointListStart = TileIndex * PointTileStride;
        uint SpotListStart = TileIndex * SpotTileStride;
        
        uint PointCount = min(PointTileList[PointListStart], 64u);
        uint SpotCount = min(SpotTileList[SpotListStart], 64u);
        
        float3 Color = GetLightCountColor(PointCount + SpotCount);
        
        uint2 LocalCoord = PixelCoord % 16u;

        if (LocalCoord.x == 0u || LocalCoord.y == 0u)
        {
            Color *= 0.2f;
        }
        
        Output.Color = float4(Color, 1.0f);
        Output.Normal = float4(WorldNormal, 0.f);
        Output.ObjectMask = ObjectMask;

        return Output;
    }
    
    if (bVisualizeCascades != 0u)
    {
        // Cascade 0: Red, Cascade 1: Green, Cascade 2: Blue, Cascade 3: Yellow, Beyond: Gray
        float3 CascadeColors[5] = {
            float3(1.0f, 0.25f, 0.25f), // 0: Red
            float3(0.25f, 1.0f, 0.25f), // 1: Green
            float3(0.25f, 0.45f, 1.0f), // 2: Blue
            float3(1.0f, 1.0f, 0.25f),  // 3: Yellow
            float3(0.35f, 0.35f, 0.35f) // 4: Beyond
        };

        int CascadeIdx;
        int NextCascadeIdx;
        float BlendFactor;
        GetCascadeBlendInfo(Input.WorldPosition, CascadeIdx, NextCascadeIdx, BlendFactor);

#if USE_NORMAL_MAP
        float3 ShadowNormal = Normal;
#else
        float3 ShadowNormal = WorldNormal;
#endif
        float Shadow = ComputeDirectionalShadowFactor(Input.WorldPosition, ShadowNormal, Input.Position.xy);

        float3 BaseColor = CascadeColors[min(CascadeIdx, 4)];
        if (BlendFactor > 0.0f && CascadeIdx < NumCascades - 1)
        {
            float3 NextColor = CascadeColors[min(NextCascadeIdx, 4)];
            BaseColor = lerp(BaseColor, NextColor, BlendFactor);
        }

        float3 Color = BaseColor * (0.35f + 0.65f * Shadow);
        
        Output.Color = float4(Color, 1.0f);
        Output.Normal = float4(WorldNormal, 0.f);
        Output.ObjectMask = ObjectMask;
        
        return Output;
    }
    
    float3 DiffuseLight;
    float3 SpecularLight;
#if SHADING_MODEL == SHADING_MODEL_GOURAUD
    DiffuseLight = Input.DiffuseLight;
    SpecularLight = Input.SpecularLight;
#else // Phong
    float3 N = normalize(WorldNormal);
    float3 V = normalize(CameraPosition - Input.WorldPosition);
    
    ComputeTiledLighting(N, Normal, Input.WorldPosition, V, Shininess, Input.Position.xy, DiffuseLight, SpecularLight);
    
#endif
    FinalColor = float4(Ka * AmbientLight.Color + Kd * DiffuseLight + Ks * SpecularLight, Alpha);
#endif
    
    Output.Color = FinalColor;
    Output.Normal = float4(normalize(WorldNormal), 0.f);
    Output.ObjectMask = ObjectMask;
    
    return Output;
}
#endif
#endif
