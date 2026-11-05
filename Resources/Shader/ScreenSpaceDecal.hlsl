// 기존 표면 셰이더의 조명 및 그림자 함수 재사용
#define UBER_LIT_LIBRARY_ONLY
#define VIEWMODE 0
#define LIGHTING_MODEL 2
#define SHADING_MODEL 1
#define HAS_NORMAL_MAP 0
#define USE_SKINNING 0
#define RECEIVE_SHADOW 1
#include "UberLit.hlsl"

// 카메라 깊이와 불투명 표면 정보
Texture2D<float> SceneDepth : register(t4);
Texture2D<float4> NormalReceiverTexture : register(t5);

// 화면 위치 복원과 데칼 배치의 전용 상수
cbuffer DecalConstants : register(b7)
{
    matrix InverseViewProjection;
    matrix WorldToDecal;
    matrix DecalToWorld;
    float4 DecalColor;
    float2 ViewportOrigin;
    float2 ViewportSize;
    uint bUnlit;
    float3 DecalPadding;
};
struct FDecalPSInput
{
    float4 Position : SV_POSITION;
};

// 단위 박스 위치의 정점 입력
struct FDecalVSInput
{
    float3 Position : POSITION;
};

// 데칼 볼륨의 화면 투영
FDecalPSInput mainVS(FDecalVSInput Input)
{
    FDecalPSInput Output;
    float4 WorldPosition = mul(float4(Input.Position, 1.0f), DecalToWorld);
    Output.Position = mul(WorldPosition, ViewProjection);
    return Output;
}

// 화면 깊이 기반 데칼 투영 및 자체 조명
float4 mainPS(FDecalPSInput Input) : SV_TARGET
{
    int2 PixelCoord = int2(Input.Position.xy);
    float4 Receiver = NormalReceiverTexture.Load(int3(PixelCoord, 0)); // 리시버의 월드 노멀과 데칼 리시브 여부
    float Depth = SceneDepth.Load(int3(PixelCoord, 0));

    // 리시버의 알파값<0.5 는 ReceiveDecals가 False. Depth>=1은 배경.
    if (Receiver.a < 0.5f || Depth >= 1.0f)
    {
        discard;
    }

    // 뷰포트 좌표와 D3D 깊이를 이용한 월드 위치 복원
    float2 LocalPixel = Input.Position.xy - ViewportOrigin;
    float2 ScreenUV = LocalPixel / ViewportSize;
    float2 NDC = ScreenUV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
    float4 WorldPositionH = mul(float4(NDC, Depth, 1.0f), InverseViewProjection);
    float3 WorldPosition = WorldPositionH.xyz / WorldPositionH.w;

    // 중심 원점의 단위 데칼 볼륨 내부 제한
    float3 DecalPosition = mul(float4(WorldPosition, 1.0f), WorldToDecal).xyz;
    clip(0.5f - abs(DecalPosition));

    // +X 투영과 YZ 평면의 텍스처 좌표 생성
    float2 DecalUV = float2(DecalPosition.y + 0.5f, 0.5f - DecalPosition.z);

    float4 SurfaceColor = AlbedoTexture.Sample(AlbedoSampler, DecalUV + UVScrollOffset) * BaseColor * DecalColor;
    float Alpha = saturate(SurfaceColor.a);

    if (Alpha <= 0.0f)
    {
        discard;
    }

    // Unlit 뷰의 색상 출력
    if (bUnlit != 0u)
    {
        return float4(SurfaceColor.rgb, Alpha);
    }

    // 수신 표면의 기하 노멀과 기존 광원·그림자 계산
    float3 N = normalize(Receiver.xyz);
    float3 V = normalize(CameraPosition - WorldPosition);
    float3 DiffuseLight;
    float3 SpecularLight;

    ComputeTiledLighting(N, WorldPosition, V, Shininess, LocalPixel, DiffuseLight, SpecularLight);

    float3 LitColor = SurfaceColor.rgb * (AmbientLight.Color + DiffuseLight) + SpecularColor * SpecularLight;

    return float4(LitColor, Alpha);
}