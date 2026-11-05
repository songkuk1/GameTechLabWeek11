#pragma pack_matrix(row_major)

#define OM_RECIEVEDECALS 0x4

// 박스 투영 및 화면 위치 복원 상수
cbuffer DecalConstants : register(b0)
{
    matrix InvViewProjection;
    matrix InvWorld;
    matrix WorldViewProjection;
    float4 Tint;
    float2 InvTargetSize;
    float2 UVScrollOffset;
    uint bAffectBaseColor;
    uint bAffectNormal;
    uint bFlipNormalGreen;
    uint DecalPadding;
};

// 투영 전용 박스의 위치 입력
struct VS_INPUT
{
    float3 Position : POSITION;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
};

struct PS_OUTPUT
{
    float4 Color : SV_Target0;
    float4 Normal : SV_Target1;
};

Texture2D<float> DepthTexture : register(t0);
Texture2D<uint> ObjectMaskTexture : register(t1);
Texture2D DecalTexture : register(t2);
Texture2D NormalTexture : register(t3);
Texture2D<float4> GeometryNormalTexture : register(t4);

SamplerState DecalSampler : register(s0);

// 뷰포트 좌표와 D3D 깊이를 이용한 월드 위치 복원
float3 ReconstructWorldPosition(float2 PixelPosition, float Depth)
{
    float2 ScreenUV = PixelPosition * InvTargetSize;
    float2 NDC = ScreenUV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
    float4 WorldPositionH = mul(float4(NDC, Depth, 1.0f), InvViewProjection);
    return WorldPositionH.xyz / WorldPositionH.w;
}

// 현재 뷰포트 내부의 유효 깊이 복원
bool TryLoadWorldPosition(float2 PixelPosition, out float3 WorldPosition)
{
    WorldPosition = 0.0f;
    float2 TargetSize = 1.0f / InvTargetSize;
    if (any(PixelPosition < 0.0f) || any(PixelPosition >= TargetSize))
    {
        return false;
    }

    float Depth = DepthTexture.Load(int3(int2(PixelPosition), 0));
    if (Depth >= 1.0f)
    {
        return false;
    }

    WorldPosition = ReconstructWorldPosition(PixelPosition, Depth);
    return true;
}

// 중심 접평면 기준의 한 픽셀 위치 변화량
float3 GetPlaneDerivative(float2 PixelPosition, float2 Axis, float3 WorldPosition, float3 GeoNormal)
{
    float2 NeighborPixel = PixelPosition + Axis;
    float3 RayOrigin = ReconstructWorldPosition(NeighborPixel, 0.0f);
    float3 RayDirection = ReconstructWorldPosition(NeighborPixel, 0.5f) - RayOrigin;
    float Denominator = dot(RayDirection, GeoNormal);
    if (Denominator * Denominator <= 1.0e-8f * dot(RayDirection, RayDirection))
    {
        return 0.0f;
    }

    float RayDistance = dot(WorldPosition - RayOrigin, GeoNormal) / Denominator;
    return RayOrigin + RayDirection * RayDistance - WorldPosition;
}

// 깊이 불연속을 제외한 화면 양의 방향 미분
float3 GetSurfaceDerivative(float2 PixelPosition, float2 Axis, float3 WorldPosition, float3 GeoNormal)
{
    float3 ExpectedDerivative = GetPlaneDerivative(PixelPosition, Axis, WorldPosition, GeoNormal);
    float3 Derivative = ExpectedDerivative;
    float BestError = dot(ExpectedDerivative, ExpectedDerivative);
    if (BestError == 0.0f)
    {
        return Derivative;
    }

    // 한 픽셀 예상 변화량을 기준으로 한 이웃 선택
    [unroll]
    for (int Side = -1; Side <= 1; Side += 2)
    {
        float3 NeighborPosition;
        if (!TryLoadWorldPosition(PixelPosition + Axis * Side, NeighborPosition))
        {
            continue;
        }

        float3 Candidate = (NeighborPosition - WorldPosition) * Side;
        float3 Error = Candidate - ExpectedDerivative;
        float ErrorSquared = dot(Error, Error);
        if (ErrorSquared < BestError)
        {
            Derivative = Candidate;
            BestError = ErrorSquared;
        }
    }
    return Derivative;
}

// 투영 UV 미분 기준의 노멀맵 월드 변환
float3 EvaluateDecalNormal(float2 UV, float2 UVdx, float2 UVdy, float3 PositionDx, float3 PositionDy, float3 GeoNormal)
{
    float Determinant = UVdx.x * UVdy.y - UVdx.y * UVdy.x;
    float DerivativeScale = dot(UVdx, UVdx) * dot(UVdy, UVdy);
    if (Determinant * Determinant <= 1.0e-6f * DerivativeScale)
    {
        return GeoNormal;
    }

    float3 TangentRaw = (PositionDx * UVdy.y - PositionDy * UVdx.y) / Determinant;
    float3 BitangentRaw = (PositionDy * UVdx.x - PositionDx * UVdy.x) / Determinant;
    float3 Tangent = TangentRaw - GeoNormal * dot(TangentRaw, GeoNormal);
    float TangentLengthSquared = dot(Tangent, Tangent);
    if (TangentLengthSquared <= 1.0e-8f * dot(TangentRaw, TangentRaw))
    {
        return GeoNormal;
    }

    // 직교화 및 반전 UV의 B축 방향 유지
    Tangent *= rsqrt(TangentLengthSquared);
    float3 Bitangent = cross(GeoNormal, Tangent);
    if (dot(Bitangent, BitangentRaw) < 0.0f)
    {
        Bitangent = -Bitangent;
    }

    // 원시 노멀 데이터 복원 및 기존 Green 반전 적용
    float3 TextureNormal = NormalTexture.SampleGrad(DecalSampler, UV, UVdx, UVdy).xyz * 2.0f - 1.0f;
    TextureNormal.y = bFlipNormalGreen != 0u ? -TextureNormal.y : TextureNormal.y;
    float3x3 TBN = float3x3(Tangent, Bitangent, GeoNormal);
    return normalize(mul(TextureNormal, TBN));
}

// 단위 박스의 화면 투영
PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT Output;
    Output.Position = mul(float4(Input.Position, 1.0f), WorldViewProjection);
    return Output;
}

// 화면 깊이 기반 투영 및 조명 전 데칼 데이터 출력
PS_OUTPUT mainPS(PS_INPUT Input)
{
    int2 PixelCoord = int2(Input.Position.xy);
    float Depth = DepthTexture.Load(int3(PixelCoord, 0));
    uint ObjectMask = ObjectMaskTexture.Load(int3(PixelCoord, 0));
    if ((ObjectMask & OM_RECIEVEDECALS) != OM_RECIEVEDECALS || Depth >= 1.0f)
    {
        discard;
    }

    float3 WorldPosition = ReconstructWorldPosition(Input.Position.xy, Depth);

    // 수신 표면의 실제 데칼 볼륨 내부 제한
    float3 DecalPosition = mul(float4(WorldPosition, 1.0f), InvWorld).xyz;
    clip(0.5f - abs(DecalPosition));

    // 표면 미분과 노멀맵 변환의 기하 노멀
    float3 GeoNormal = normalize(GeometryNormalTexture.Load(int3(PixelCoord, 0)).xyz);

    // +X 투영과 YZ 평면의 텍스처 좌표 생성
    float2 DecalUV = float2(DecalPosition.y + 0.5f, 0.5f - DecalPosition.z);

    // 동일한 표면 미분을 이용한 색상 및 노멀 샘플링
    float3 PositionDx = GetSurfaceDerivative(Input.Position.xy, float2(1.0f, 0.0f), WorldPosition, GeoNormal);
    float3 PositionDy = GetSurfaceDerivative(Input.Position.xy, float2(0.0f, 1.0f), WorldPosition, GeoNormal);
    float3 DecalDx = mul(float4(PositionDx, 0.0f), InvWorld).xyz;
    float3 DecalDy = mul(float4(PositionDy, 0.0f), InvWorld).xyz;
    float2 UVdx = float2(DecalDx.y, -DecalDx.z);
    float2 UVdy = float2(DecalDy.y, -DecalDy.z);
    float2 SampleUV = DecalUV + UVScrollOffset;

    // 텍스처와 기존 데칼·머티리얼 색상의 결합
    float4 SurfaceColor = DecalTexture.SampleGrad(DecalSampler, SampleUV, UVdx, UVdy) * Tint;
    float Alpha = saturate(SurfaceColor.a);
    if (Alpha <= 0.0f)
    {
        discard;
    }

    // 색상과 노멀의 독립적인 적용 비중
    PS_OUTPUT Output;
    Output.Color = float4(SurfaceColor.rgb, bAffectBaseColor != 0u ? Alpha : 0.0f);
    Output.Normal = float4(0.0f, 0.0f, 0.0f, 0.0f);
    if (bAffectNormal != 0u)
    {
        float3 DecalNormal = EvaluateDecalNormal(SampleUV, UVdx, UVdy, PositionDx, PositionDy, GeoNormal);
        Output.Normal = float4(DecalNormal, Alpha);
    }
    return Output;
}