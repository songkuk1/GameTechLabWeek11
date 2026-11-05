#pragma pack_matrix(row_major)

cbuffer ShadowViewConstants : register(b0)
{
    matrix LightViewProjection;
};

cbuffer PerObjectConstants : register(b2) // FRenderer의 PerObjectCB와 같은 레이아웃
{
    matrix World;
    matrix NormalMatrix;
};

// FVertexPNCT의 첫 요소만 받는다 : 입력 레이아웃 오프셋이 정확히 맞는다
float4 mainVS(float3 Position : POSITION) : SV_POSITION
{
    return mul(mul(float4(Position, 1.0f), World), LightViewProjection);
}

// 로더(LoadOrCompileShader)가 mainPS를 요구하므로 둔다. 실제로는 PS를 null로 바인딩한다.
void mainPS()
{
}