#pragma pack_matrix(row_major)
        
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
    float2 UV : TEXCOORD0;
    float3 Normal : NORMAL;
};

struct FPSOutput
{
    uint ObjectMask : SV_Target0;
    float4 Normal : SV_Target1;
};

cbuffer ViewConstants : register(b0)
{
    matrix ViewProjection;
    float3 CameraPosition;
    float ViewPadding;
};

cbuffer PerObjectConstants : register(b1)
{
    matrix World;
    matrix NormalMatrix; // World의 역전치
    uint ObjectMask;
    uint3 PerObjectConstantsPadding;
};

#if USE_SKINNING
StructuredBuffer<float4x4> BoneMatrices : register(t0);
#endif

FPSInput mainVS(FVSInput Input)
{
    FPSInput Output;
    float3 LocalPosition = Input.Position;
    float3 LocalNormal = Input.Normal;
    
#if USE_SKINNING
    if (Input.BoneInfluenceCount > 0)
    {
        float3 SkinnedPosition = float3(0, 0, 0);
        float3 SkinnedNormal = float3(0, 0, 0);
        for (int i = 0; i < Input.BoneInfluenceCount; i++)
        {
            int BoneIndex = Input.BoneIndices[i];
            float BoneWeight = Input.BoneWeights[i];

            // 불투명 셰이더와 동일한 본 행렬 해석
            float4x4 BoneMatrix = transpose(BoneMatrices[BoneIndex]);
            SkinnedPosition += mul(float4(Input.Position, 1.0f), BoneMatrix).xyz * BoneWeight;
            SkinnedNormal += mul(Input.Normal, (float3x3) BoneMatrix) * BoneWeight;
        }
        LocalPosition = SkinnedPosition;
        LocalNormal = normalize(SkinnedNormal);
    }
#endif

    float4 WorldPosition = mul(float4(LocalPosition, 1.0f), World);
    Output.Position = mul(WorldPosition, ViewProjection);
    Output.UV = Input.UV;
    Output.Normal = mul(LocalNormal, (float3x3) NormalMatrix);
    return Output;

}

FPSOutput mainPS(FPSInput Input)
{
    FPSOutput Output;
    
    Output.ObjectMask = ObjectMask;
    Output.Normal = float4(normalize(Input.Normal), 0.0f);

    return Output;
}
