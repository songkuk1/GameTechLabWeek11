#pragma pack_matrix(row_major)

struct FVSInput
{
    float3 Position : POSITION;
#if USE_SKINNING
    int4 BoneIndices : BLENDINDICES;
    float4 BoneWeights : BLENDWEIGHT;
    int BoneInfluenceCount : BONECOUNT;
#endif
};

cbuffer ShadowViewConstants : register(b0)
{
    matrix LightViewProjection;
};

cbuffer PerObjectConstants : register(b2)
{
    matrix World;
    matrix NormalMatrix;
};

#if USE_SKINNING
StructuredBuffer<float4x4> BoneMatrices : register(t13);
#endif

float4 mainVS(FVSInput Input) : SV_POSITION
{
    float3 LocalPosition = Input.Position;

#if USE_SKINNING
    if (Input.BoneInfluenceCount > 0)
    {
        float3 SkinnedPosition = float3(0, 0, 0);

        for (int i = 0; i < Input.BoneInfluenceCount; i++)
        {
            int BoneIndex = Input.BoneIndices[i];
            float BoneWeight = Input.BoneWeights[i];
            float4x4 BoneMatrix = transpose(BoneMatrices[BoneIndex]);
            SkinnedPosition += mul(float4(Input.Position, 1.0f), BoneMatrix).xyz * BoneWeight;
        }

        LocalPosition = SkinnedPosition;
    }
#endif

    return mul(mul(float4(LocalPosition, 1.0f), World), LightViewProjection);
}

void mainPS()
{
}