#include "EnginePCH.h"
#include "ShadowMapManager.h"
#include "RenderCommand.h"
#include "RenderUtil.h"
#include "Shader.h"
#include "Render/RenderResourceManager.h"
#include "Engine/LightSceneProxy.h"

void FShadowMapManager::UploadShadowInfos(const TArray<FShadowInfo>& ShadowInfos)
{
    NumShadowInfos = ShadowInfos.Num();

    if (!EnsureShadowInfoCapacity(NumShadowInfos))
    {
        assert(false);
        return;
    }

    // 일단 아틀라스 해상도 4096으로 하드코딩
    EnsureShadowMapAtlas(4096, NumAtlas);


    ID3D11DeviceContext* Context = RenderCommand::GetContext();
    D3D11_MAPPED_SUBRESOURCE Mapped{};

    if (NumShadowInfos > 0 && SUCCEEDED(Context->Map(ShadowInfoBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
    {
        std::memcpy(Mapped.pData, ShadowInfos.GetData(), NumShadowInfos * sizeof(FShadowInfo));
        Context->Unmap(ShadowInfoBuffer.Get(), 0);
    }
}

ID3D11DepthStencilView* FShadowMapManager::GetSliceDSV(uint32 Slice) const
{
    if (Slice >= static_cast<uint32>(ShadowDepthView.Num())) return nullptr;

    return ShadowDepthView[Slice].Get();
}

ID3D11ShaderResourceView* FShadowMapManager::GetShadowMapArraySRV() const
{
    return ShadowMapArraySRV.Get();
}

ID3D11ShaderResourceView* FShadowMapManager::GetShadowInfoSRV() const
{
    return ShadowInfoSRV.Get();
}

bool FShadowMapManager::EnsureShadowMaps(uint32 InResolution, uint32 InNumSlices)
{
    //개수가 바뀐다면, 해상도가 바뀐다면
    if (ShadowMapArrayBuffer && ShadowMapArraySRV && 
        InNumSlices == static_cast<uint32>(ShadowDepthView.Num()) && Resolution == InResolution)
    {
        return true;
    }

    NumSlices = InNumSlices;
    NumSlices = std::max(NumSlices, 1u);
    Resolution = InResolution;

    ShadowMapArrayBuffer.Reset();
    for (int32 i = 0; i < ShadowDepthView.Num(); ++i)
    {
        ShadowDepthView[i].Reset();
    }
    ShadowDepthView.Empty();
    ShadowDepthView.SetNum(NumSlices);

    ShadowMapArraySRV.Reset();


    ID3D11Device* Device = RenderCommand::GetDevice();

    

    // 리소스: 같은 크기의 깊이 이미지 N장
    D3D11_TEXTURE2D_DESC Desc{};
    // 정사각형
    Desc.Width = Resolution;          
    Desc.Height = Resolution;          
    Desc.MipLevels = 1;
    // 층 수 (Spot 수 + Point 수 × 6 ...)
    Desc.ArraySize = NumSlices;                    
    Desc.Format = DXGI_FORMAT_R32_TYPELESS;     
    Desc.SampleDesc.Count = 1;
    Desc.Usage = D3D11_USAGE_DEFAULT;
    Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    HRESULT hr = Device->CreateTexture2D(&Desc, nullptr, ShadowMapArrayBuffer.GetAddressOf());
    if (FAILED(hr))
    {
        assert(false);
    }

    


    // DSV: 층마다 하나씩 (그릴 때는 한 층만 대상)
    for (uint32 i = 0; i < NumSlices; ++i)
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC Dsv{};
        Dsv.Format = DXGI_FORMAT_D32_FLOAT;
        Dsv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        Dsv.Texture2DArray.MipSlice = 0;
        Dsv.Texture2DArray.FirstArraySlice = i;
        Dsv.Texture2DArray.ArraySize = 1;
        hr = Device->CreateDepthStencilView(ShadowMapArrayBuffer.Get(), &Dsv, ShadowDepthView[i].GetAddressOf());
        if (FAILED(hr))
        {
            assert(false);
        }
    }

    // SRV: 전체 층을 한 번에 (셰이더는 이걸 Texture2DArray로 본다)
    D3D11_SHADER_RESOURCE_VIEW_DESC Srv{};
    Srv.Format = DXGI_FORMAT_R32_FLOAT;
    Srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    Srv.Texture2DArray.MostDetailedMip = 0;
    Srv.Texture2DArray.MipLevels = 1;
    Srv.Texture2DArray.FirstArraySlice = 0;
    Srv.Texture2DArray.ArraySize = NumSlices;
    hr = Device->CreateShaderResourceView(ShadowMapArrayBuffer.Get(), &Srv, ShadowMapArraySRV.GetAddressOf());
    if (FAILED(hr))
    {
        assert(false);
    }

    return true;
}

bool FShadowMapManager::EnsureShadowMapAtlas(uint32 InNewAtlasResolution, uint32 InNewNumAtlas)
{
    //개수가 바뀐다면, 해상도가 바뀐다면
    if (ShadowMapAtlasBuffer && ShadowMapAtlasArraySRV &&
        InNewNumAtlas == static_cast<uint32>(ShadowAtlasDepthView.Num()) && AtlasResolution == InNewAtlasResolution)
    {
        return true;
    }

    NumAtlas = InNewNumAtlas;
    NumAtlas = std::max(NumAtlas, 1u);
    AtlasResolution = InNewAtlasResolution;

    ShadowMapAtlasBuffer.Reset();
    for (uint32 i = 0; i < ShadowAtlasDepthView.Num(); i++)
    {
        ShadowAtlasDepthView[i].Reset();
    }
    ShadowAtlasDepthView.Empty();
    ShadowAtlasDepthView.SetNum(NumAtlas);

    ShadowMapAtlasArraySRV.Reset();


    ID3D11Device* Device = RenderCommand::GetDevice();

    D3D11_TEXTURE2D_DESC Desc{};
    // 정사각형
    Desc.Width = AtlasResolution;
    Desc.Height = AtlasResolution;
    Desc.MipLevels = 1;
    // 층 수 (Spot 수 + Point 수 × 6 ...)
    Desc.ArraySize = NumAtlas;   // TODO : 아틀라스 배열로 수정
    Desc.Format = DXGI_FORMAT_R32_TYPELESS;
    Desc.SampleDesc.Count = 1;
    Desc.Usage = D3D11_USAGE_DEFAULT;
    Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = Device->CreateTexture2D(&Desc, nullptr, ShadowMapAtlasBuffer.GetAddressOf());
    if (FAILED(hr))
    {
        assert(false);
    }


    // DSV: 층마다 하나씩 (그릴 때는 한 층만 대상)
    for (uint32 i = 0; i < NumAtlas; ++i)
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC Dsv{};
        Dsv.Format = DXGI_FORMAT_D32_FLOAT;
        Dsv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        Dsv.Texture2DArray.MipSlice = 0;
        Dsv.Texture2DArray.FirstArraySlice = i;
        Dsv.Texture2DArray.ArraySize = 1;
        // TODO: RenderCommand 에 DSV 생성 함수 만들기
        hr = Device->CreateDepthStencilView(ShadowMapAtlasBuffer.Get(), &Dsv, ShadowAtlasDepthView[i].GetAddressOf());
        if (FAILED(hr))
        {
            assert(false);
        }
    }

    // SRV: 전체 층을 한 번에 (셰이더는 이걸 Texture2DArray로 본다)
    D3D11_SHADER_RESOURCE_VIEW_DESC Srv{};
    Srv.Format = DXGI_FORMAT_R32_FLOAT;
    Srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    Srv.Texture2DArray.MostDetailedMip = 0;
    Srv.Texture2DArray.MipLevels = 1;
    Srv.Texture2DArray.FirstArraySlice = 0;
    Srv.Texture2DArray.ArraySize = NumAtlas;  // TODO : 아틀라스 배열로 수정
    // TODO: RenderCommand 에 SRV 생성 함수 만들기
    hr = Device->CreateShaderResourceView(ShadowMapAtlasBuffer.Get(), &Srv, ShadowMapAtlasArraySRV.GetAddressOf());
    if (FAILED(hr))
    {
        assert(false);
    }

    return true;
}

void FShadowMapManager::BeginShadowRender(uint32 Slice)
{
    //DSV, Viewport 바인딩
    //Viewport는 나중에 다른 해상도로 바뀔 것이다.

    ID3D11DeviceContext* Context = RenderCommand::GetContext();

    if (Slice >= static_cast<uint32>(ShadowDepthView.Num())) return;

    //해당하는 DSV 있다면 초기화
    ID3D11DepthStencilView* DSV = ShadowDepthView[Slice].Get();

    // RTV 없이 DSV만 : Depth-only
    Context->OMSetRenderTargets(0, nullptr, DSV);
    Context->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);


    // 섀도맵 해상도로 Viewport (뷰포트 크기와 무관)
    //나중에 맵마다 해상도를 다르게 한다면 좀 더 구조적으로 잡혀야한다.
    //지금은 모든 텍스처가 같다.
    RenderCommand::SetViewport(0, 0, Resolution, Resolution);
}

void FShadowMapManager::ClearAtlas()
{
    ID3D11DeviceContext* Context = RenderCommand::GetContext();
    for (uint32 i = 0; i < NumAtlas; ++i)
        Context->ClearDepthStencilView(ShadowAtlasDepthView[i].Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void FShadowMapManager::BeginShadowRenderWithAtlas(const FShadowInfo& ShadowInfo)
{
    //DSV, Viewport 바인딩
    //Viewport는 나중에 다른 해상도로 바뀔 것이다.

    ID3D11DeviceContext* Context = RenderCommand::GetContext();

    //해당하는 DSV 있다면 초기화
    ID3D11DepthStencilView* DSV = ShadowAtlasDepthView[ShadowInfo.AtlasIndex].Get();

    // RTV 없이 DSV만 : Depth-only
    Context->OMSetRenderTargets(0, nullptr, DSV);

    // 섀도맵 해상도로 Viewport (뷰포트 크기와 무관)
    //나중에 맵마다 해상도를 다르게 한다면 좀 더 구조적으로 잡혀야한다.
    //지금은 모든 텍스처가 같다.
    RenderCommand::SetViewport(ShadowInfo.AtlasStartX, ShadowInfo.AtlasStartY, ShadowInfo.AtlasSize, ShadowInfo.AtlasSize);
}

void FShadowMapManager::EndShadowPass()
{
    // DSV를 풀어야 같은 리소스를 SRV로 읽을 수 있다
    RenderCommand::GetContext()->OMSetRenderTargets(0, nullptr, nullptr);
}

void FShadowMapManager::SetFilterSettings(const FShadowFilterSettings& Settings)
{
    FShadowFilterConstants Constants{};
    Constants.Mode = static_cast<uint32>(Settings.Mode);
    Constants.bPoisson = Settings.bPoisson ? 1u : 0u;
    Constants.bGaussian = Settings.bGaussian ? 1u : 0u;
    Constants.bRotate = Settings.bRotate ? 1u : 0u;
    Constants.PCFRadiusTexels = Settings.PCFRadiusTexels;
    Constants.MaxFilterTexels = Settings.MaxFilterTexels;
    Constants.SearchScale = Settings.SearchScale;

    // 뷰마다 호출되므로 값이 바뀌었을 때만 다시 올린다
    if (std::memcmp(&Constants, &FilterConstants, sizeof(FShadowFilterConstants)) != 0)
    {
        FilterConstants = Constants;
        bFilterDirty = true;
    }
}

void FShadowMapManager::BindForShading()
{
    ID3D11DeviceContext* Context = RenderCommand::GetContext();

    // Texture2DArray
    Context->PSSetShaderResources(2, 1, ShadowMapAtlasArraySRV.GetAddressOf());

    // Texture2D: Directional Light Shadow Atlas
    Context->PSSetShaderResources(3, 1, DirectionalShadowSRV.GetAddressOf());

    // StructuredBuffer
    Context->PSSetShaderResources(12, 1, ShadowInfoSRV.GetAddressOf());       
    // Directional Shadow Constants (b5)
    if (DirectionalShadowCB)
    {
        RenderCommand::BindConstantBuffer(5, DirectionalShadowCB.get(), EShaderBindFlagBits::Pixel);
    }

    // Shadow Filter Constants (b6)
    if (!FilterCB)
    {
        FilterCB = RenderCommand::CreateConstantBuffer(sizeof(FShadowFilterConstants));
        bFilterDirty = true;
    }
    if (bFilterDirty)
    {
        RenderCommand::UpdateBufferData(FilterCB.get(), &FilterConstants, sizeof(FShadowFilterConstants));
        bFilterDirty = false;
    }
    RenderCommand::BindConstantBuffer(6, FilterCB.get(), EShaderBindFlagBits::Pixel);

    // s0은 BindMaterial이 머티리얼마다 덮어쓰므로 섀도 샘플러는 s1
    RenderCommand::BindSamplerState(1, ESamplerState::Point, EShaderBindFlagBits::Pixel);
    RenderCommand::BindSamplerState(2, ESamplerState::Comparison_Linear, EShaderBindFlagBits::Pixel);
}

bool FShadowMapManager::EnsureDirectionalShadowMap(uint32 InResolution)
{
    if (DirectionalShadowTexture && DirectionalShadowDSV && DirectionalShadowSRV && DirectionalResolution == InResolution)
    {
        return true;
    }

    DirectionalResolution = InResolution;
    DirectionalShadowTexture.Reset();
    DirectionalShadowDSV.Reset();
    DirectionalShadowSRV.Reset();

    ID3D11Device* Device = RenderCommand::GetDevice();

    D3D11_TEXTURE2D_DESC Desc{};
    Desc.Width = DirectionalResolution;
    Desc.Height = DirectionalResolution;
    Desc.MipLevels = 1;
    Desc.ArraySize = 1;
    Desc.Format = DXGI_FORMAT_R32_TYPELESS;
    Desc.SampleDesc.Count = 1;
    Desc.Usage = D3D11_USAGE_DEFAULT;
    Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = Device->CreateTexture2D(&Desc, nullptr, DirectionalShadowTexture.GetAddressOf());
    if (FAILED(hr))
    {
        assert(false);
        return false;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC DsvDesc{};
    DsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    DsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    DsvDesc.Texture2D.MipSlice = 0;

    hr = Device->CreateDepthStencilView(DirectionalShadowTexture.Get(), &DsvDesc, DirectionalShadowDSV.GetAddressOf());
    if (FAILED(hr))
    {
        assert(false);
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc{};
    SrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    SrvDesc.Texture2D.MostDetailedMip = 0;
    SrvDesc.Texture2D.MipLevels = 1;

    hr = Device->CreateShaderResourceView(DirectionalShadowTexture.Get(), &SrvDesc, DirectionalShadowSRV.GetAddressOf());
    if (FAILED(hr))
    {
        assert(false);
        return false;
    }

    return true;
}

// 카메라의 절두체(Frustum)를 4구간으로 나누고, 각 구간을 완벽히 감싸는 4개의 최소 외접구(Bounding Sphere)를 계산
void FShadowMapManager::ComputeCascadeSpheres(
    const FVector& CameraPosition,
    const FVector& CameraForward,
    float FovDegrees,
    float AspectRatio,
    float NearClip,
    float ShadowMaxDist,
    FVector4 OutCascadeSpheres[4])
{
    // 1. 파라미터 유효성 검사 및 보정
    if (FovDegrees <= 1.0f || FovDegrees >= 179.0f)
    {
        FovDegrees = 60.0f;
    }
    if (AspectRatio <= 0.01f)
    {
        AspectRatio = 16.0f / 9.0f;
    }
    if (NearClip <= 0.0f)
    {
        NearClip = 0.1f;
    }
    if (ShadowMaxDist <= NearClip)
    {
        ShadowMaxDist = 300.0f;
    }

    // 2. FOV와 종횡비 기반의 대각선 비율 계수(Alpha) 계산
    const float HalfFOVRad = FMath::DegreesToRadians(FovDegrees * 0.5f);
    const float TanHalf = tan(HalfFOVRad);
    const float Alpha = TanHalf * TanHalf * (1.0f + AspectRatio * AspectRatio);

    // 3. 4개 구간 분할 거리 (Splits)
    // 눈앞 초근거리(12) -> 근거리(35) -> 중거리(100) -> 원거리(ShadowMaxDist)
    float Splits[4] = { 12.0f, 35.0f, 100.0f, ShadowMaxDist };
    for (int i = 0; i < 4; ++i)
    {
        if (Splits[i] > ShadowMaxDist)
        {
            Splits[i] = ShadowMaxDist;
        }
    }

    // 4. 구간별 최소 외접구(Center, Radius) 계산
    float PrevFar = NearClip;
    for (int i = 0; i < 4; ++i)
    {
        const float n = PrevFar;
        const float f = Splits[i];
        PrevFar = f;

        // 구 중심까지의 시선 방향 거리 d 계산
        float d = 0.5f * (f + n) * (1.0f + Alpha);
        float Radius = 0.0f;

        // 시야각이 너무 넓어 중심이 뒷면보다 뒤로 넘어가는 경우 뒷면 중심으로 클램핑
        if (d > f)
        {
            d = f;
            Radius = sqrt(Alpha) * f;
        }
        else
        {
            Radius = sqrt((f - d) * (f - d) + Alpha * f * f);
        }

        // 월드 공간 구 중심 좌표
        const FVector Center = CameraPosition + CameraForward * d;
        OutCascadeSpheres[i] = FVector4(Center.X, Center.Y, Center.Z, Radius);
    }
}

// 4개의 구체와 라이트 방향을 받아 텍셀 스내핑이 적용된 4개의 Light VP 행렬 생성
void FShadowMapManager::ComputeCascadeLightMatrices(
    const FVector& LightDirection,
    FVector4 InOutCascadeSpheres[4],
    uint32 CascadeResolution,
    FMatrix OutCascadeMatrices[4],
    float OutCascadeRanges[4])
{
    // 1. Directional Light 카메라의 기저 벡터 정의
    FVector Forward = LightDirection.GetSafeNormal();
    if (Forward.SizeSquared() < 1e-4f)
    {
        Forward = FVector(0.0f, 0.0f, -1.0f);
    }
    const FVector UpGuide = (std::abs(Forward.Z) > 0.99f) ? FVector(0.0f, 1.0f, 0.0f) : FVector(0.0f, 0.0f, 1.0f);
    const FVector Right = FVector::Cross(UpGuide, Forward).GetSafeNormal();
    const FVector Up = FVector::Cross(Forward, Right).GetSafeNormal();

    const FMatrix Rotation(
        Forward.X, Right.X, Up.X, 0.0f,
        Forward.Y, Right.Y, Up.Y, 0.0f,
        Forward.Z, Right.Z, Up.Z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );

    // 2. 각 캐스케이드별 행렬 계산
    for (int i = 0; i < 4; ++i)
    {
        const FVector Center(InOutCascadeSpheres[i].X, InOutCascadeSpheres[i].Y, InOutCascadeSpheres[i].Z);
        const float Radius = InOutCascadeSpheres[i].W;

        // 3. 텍셀 스내핑 (섀도우 지터링 방지)
        FVector SnappedCenter = Center;
        if (CascadeResolution > 0)
        {
            const float WorldUnitsPerTexel = (2.0f * Radius) / static_cast<float>(CascadeResolution);

            // 월드 원점 기준 Light 공간 좌표 (Right, Up 성분)
            const float CenterY = Center.Dot(Right);
            const float CenterZ = Center.Dot(Up);

            // 텍셀 크기 단위로 내림(floor)하여 정수 격자에 맞춤
            const float SnappedY = std::floor(CenterY / WorldUnitsPerTexel) * WorldUnitsPerTexel;
            const float SnappedZ = std::floor(CenterZ / WorldUnitsPerTexel) * WorldUnitsPerTexel;

            const float OffsetY = SnappedY - CenterY;
            const float OffsetZ = SnappedZ - CenterZ;

            // 스냅 오차만큼 Center 보정
            SnappedCenter = Center + Right * OffsetY + Up * OffsetZ;
            InOutCascadeSpheres[i] = FVector4(SnappedCenter.X, SnappedCenter.Y, SnappedCenter.Z, Radius);
        }

        // Shadow Pancaking (DepthClipEnable = FALSE) 적용:
        // 광원 쪽으로 멀리 있는 캐스터는 GPU가 깊이 0.0으로 클램핑하므로 Frustum을 거대하게 밀어낼 필요가 없음.
        // 수신 구체 앞/뒤로 안전 마진(NearMargin)만 살짝 두어 깊이 구간(DepthRange)을 극도로 타이트하게 압축.
        const float NearMargin = 5.0f;
        const float BackDistance = Radius + NearMargin;
        const FVector Eye = SnappedCenter - Forward * BackDistance;

        // Light View 행렬
        const FMatrix View = FMatrix::MakeTranslation(Eye * -1.0f) * Rotation;

        // 4. 직교 투영 (Orthographic Projection) 행렬
        const float Width = 2.0f * Radius;
        const float Height = Width;
        const float NearClip = 0.1f;
        const float FarClip = BackDistance + Radius + NearMargin;
        const float DepthRange = FarClip - NearClip;

        if (OutCascadeRanges)
        {
            OutCascadeRanges[i] = DepthRange;
        }

        const float XScale = 2.0f / Width;
        const float YScale = 2.0f / Height;
        const float ZScale = 1.0f / DepthRange;
        const float ZOffset = -NearClip / DepthRange;

        const FMatrix Proj = FMatrix(
            0.0f, 0.0f, ZScale, 0.0f,
            XScale, 0.0f, 0.0f, 0.0f,
            0.0f, YScale, 0.0f, 0.0f,
            0.0f, 0.0f, ZOffset, 1.0f
        );

        OutCascadeMatrices[i] = View * Proj;
    }
}

void FShadowMapManager::SetupDirectionalShadow(
    const FShadowInfo& DirectionalShadow,
    const FVector& LightDirection,
    const FVector& CameraPosition,
    const FVector& CameraForward,
    float FovDegrees,
    float AspectRatio,
    bool bVisualizeCascades)
{
    EnsureDirectionalShadowMap(DirectionalResolution);

    if (!DirectionalShadowCB)
    {
        DirectionalShadowCB = RenderCommand::CreateConstantBuffer(sizeof(FDirectionalShadowConstants));
    }

    // 1. 카메라 Frustum 기반 4개 Bounding Sphere 계산
    FVector4 CascadeSpheres[4];
    ComputeCascadeSpheres(CameraPosition, CameraForward, FovDegrees, AspectRatio, 0.1f, 300.0f, CascadeSpheres);

    // 2. 4개 구체 기반 텍셀 스내핑된 Light VP 행렬 및 깊이 구간 길이 계산 (4096 Atlas의 2x2 분할이므로 캐스케이드당 2048)
    ComputeCascadeLightMatrices(LightDirection, CascadeSpheres, DirectionalResolution / 2, CachedCascadeMatrices, CachedCascadeRanges);

    FDirectionalShadowConstants Constants{};
    for (int i = 0; i < 4; ++i)
    {
        Constants.CascadeViewProjection[i] = CachedCascadeMatrices[i];
        Constants.CascadeSplitSpheres[i] = CascadeSpheres[i];
    }

    // 3. 4096 Atlas 내 2x2 쿼드런트 오프셋 (Scale: 0.5, 0.5)
    Constants.CascadeOffsets[0] = FVector4(0.5f, 0.5f, 0.0f, 0.0f); // Top-Left
    Constants.CascadeOffsets[1] = FVector4(0.5f, 0.5f, 0.5f, 0.0f); // Top-Right
    Constants.CascadeOffsets[2] = FVector4(0.5f, 0.5f, 0.0f, 0.5f); // Bottom-Left
    Constants.CascadeOffsets[3] = FVector4(0.5f, 0.5f, 0.5f, 0.5f); // Bottom-Right

    // 4. 바이어스 및 캐스케이드 개수
    // 각 캐스케이드의 깊이 구간(FarClip - NearClip) 길이에 비례하여 DepthBias를 동적으로 계산
    const float BaseBias = DirectionalShadow.Bias;
    const float BaseRange = (CachedCascadeRanges[0] > 0.0f) ? CachedCascadeRanges[0] : 1.0f;
    Constants.CascadeBiases = FVector4(
        BaseBias * (CachedCascadeRanges[0] / BaseRange),
        BaseBias * (CachedCascadeRanges[1] / BaseRange),
        BaseBias * (CachedCascadeRanges[2] / BaseRange),
        BaseBias * (CachedCascadeRanges[3] / BaseRange)
    );
    Constants.NormalBias = DirectionalShadow.NormalBias;
    Constants.NumCascades = 4;
    Constants.bVisualizeCascades = bVisualizeCascades ? 1 : 0;

    // 5. GPU 상수 버퍼(DirectionalShadowCB, b5)로 업로드
    RenderCommand::UpdateBufferData(DirectionalShadowCB.get(), &Constants);
}

void FShadowMapManager::BeginDirectionalCascadeRender(uint32 CascadeIndex)
{
    ID3D11DeviceContext* Context = RenderCommand::GetContext();
    if (!DirectionalShadowDSV) return;

    // 이전 패스 셰이딩용 t3 바인딩 해제
    ID3D11ShaderResourceView* NullSRV = nullptr;
    Context->PSSetShaderResources(3, 1, &NullSRV);

    Context->OMSetRenderTargets(0, nullptr, DirectionalShadowDSV.Get());

    // 0번 캐스케이드 렌더 전 4096 Atlas 전체 1회 클리어
    if (CascadeIndex == 0)
    {
        Context->ClearDepthStencilView(DirectionalShadowDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
    }

    // 2x2 그리드 뷰포트 (4096 해상도 기준 각 2048x2048)
    const uint32 HalfRes = DirectionalResolution / 2;
    const uint32 ViewportX = (CascadeIndex % 2) * HalfRes;
    const uint32 ViewportY = (CascadeIndex / 2) * HalfRes;

    D3D11_VIEWPORT Viewport{};
    Viewport.TopLeftX = static_cast<float>(ViewportX);
    Viewport.TopLeftY = static_cast<float>(ViewportY);
    Viewport.Width = static_cast<float>(HalfRes);
    Viewport.Height = static_cast<float>(HalfRes);
    Viewport.MinDepth = 0.0f;
    Viewport.MaxDepth = 1.0f;

    Context->RSSetViewports(1, &Viewport);
}

void FShadowMapManager::BeginDirectionalShadowRender()
{
    BeginDirectionalCascadeRender(0);
}

void FShadowMapManager::EndDirectionalShadowRender()
{
    RenderCommand::GetContext()->OMSetRenderTargets(0, nullptr, nullptr);
}

void FShadowMapManager::UpdatePreview(FLightSceneProxy* PreviewShadowMapProxy, const TArray<FShadowInfo>& ShadowInfos)
{
    if (!PreviewShadowMapProxy)
        return;

    TArray<FShadowPreview>& Previews = PreviewShadowMapProxy->GetShadowPreview();

    // Directional Light의 경우 이미 단일 Texture2D 형태이므로 SRV를 직접 전달
    if (PreviewShadowMapProxy->GetLightType() == ELightType::Directional)
    {
        if (Previews.IsValidIndex(0) == false)
        {
            Previews.Add(FShadowPreview());
        }
        Previews[0].ShadowSRV = DirectionalShadowSRV.Get();
        Previews[0].Resolution = DirectionalResolution;
        Previews[0].PreviewUV = { {0.0f, 0.0f}, {1.0f, 1.0f} };

        return;
    }

    const int32 BaseIndex = PreviewShadowMapProxy->GetShadowIndex();
    const int32 ShadowInfoNum = PreviewShadowMapProxy->GetShadowInfoNum();
    if (BaseIndex < 0 || ShadowInfoNum <= 0 || BaseIndex + ShadowInfoNum > ShadowInfos.Num())
        return;

    // Point Light 6장이어도 왜 아틀라스 개수만큼 Num을 set하는가??
    // 0번 1번 아틀라스에 저장되면 상관 없음
    // 3번 4번 아틀라스에 저장되면 ShadowInfo 가 갖고있는 인덱스는 3혹은 4인데 
    // PrevicewShadowTextures 배열에 접근할 때는 추가적인 인덱스 계산이 필요함.
    // 그래서 번호 유지를 위해서 실제로 사용하는 인덱스에만 저장.
    if (PreviewShadowTextures.Num() < NumAtlas)
    {
        PreviewShadowTextures.SetNum(NumAtlas);
        PreviewShadowSRVs.SetNum(NumAtlas);
    }

    TArray<uint32> CopiedPages;

    const float InvAtlas = 1.0f / static_cast<float>(AtlasResolution);

    // ShadowInfo 개수만큼 
    for (int32 i = 0; i < ShadowInfoNum; ++i)
    {
        if (Previews.IsValidIndex(i) == false)
        {
            Previews.Add(FShadowPreview());
        }

        const FShadowInfo& Info = ShadowInfos[BaseIndex + i];
        const uint32 Page = Info.AtlasIndex;

        if (PreviewShadowTextures[Page] == nullptr)
        {
            ID3D11Device* Device = RenderCommand::GetDevice();

            D3D11_TEXTURE2D_DESC PreviewDesc{};
            // 정사각형
            PreviewDesc.Width = AtlasResolution;
            PreviewDesc.Height = AtlasResolution;
            PreviewDesc.MipLevels = 1;
            // 층 수 (Spot 수 + Point 수 × 6 ...)
            PreviewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            PreviewDesc.SampleDesc.Count = 1;
            PreviewDesc.Usage = D3D11_USAGE_DEFAULT;
            PreviewDesc.ArraySize = 1;
            PreviewDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            HRESULT hr = Device->CreateTexture2D(&PreviewDesc, nullptr, PreviewShadowTextures[Page].ReleaseAndGetAddressOf());
            if (FAILED(hr))
            {
                assert(false);
                return;
            }

            PreviewShadowMapProxy->GetShadowPreview()[i].Resolution = Resolution;

            D3D11_SHADER_RESOURCE_VIEW_DESC PreviewSrv{};
            PreviewSrv.Format = DXGI_FORMAT_R32_FLOAT;
            // ImGui 셰이더가 Texture2D라서
            PreviewSrv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            PreviewSrv.Texture2D.MipLevels = 1;
            hr = Device->CreateShaderResourceView(PreviewShadowTextures[Page].Get(), &PreviewSrv, PreviewShadowSRVs[Page].ReleaseAndGetAddressOf());
            if (FAILED(hr))
            {
                assert(false);
            }
        }

        if (CopiedPages.Find(Page) == -1)
        {
            const UINT SrcSubresource = D3D11CalcSubresource(0, Info.AtlasIndex, 1);

            //해당 Subresource를 복사합니다.
            //    ID3D11Resource * pDstResource, UINT DstSubresource,   // 어디로 (PreviewTexture, 0번)
            //    UINT DstX, UINT DstY, UINT DstZ,                     // 대상 안의 시작 좌표 (0,0,0)
            //    ID3D11Resource * pSrcResource, UINT SrcSubresource,   // 어디서 (ShadowMapArray, 층 k)
            //    const D3D11_BOX * pSrcBox                            // 복사할 영역. nullptr = 서브리소스 전체

            RenderCommand::GetContext()->CopySubresourceRegion(
                PreviewShadowTextures[Page].Get(), 0, 0, 0, 0,
                ShadowMapAtlasBuffer.Get(), SrcSubresource, nullptr);
            CopiedPages.Add(Page);
        }


        Previews[i].ShadowSRV = PreviewShadowSRVs[Page].Get();
        Previews[i].Resolution = Info.AtlasSize;
        Previews[i].PreviewUV.StartUV = { Info.AtlasStartX * InvAtlas, Info.AtlasStartY * InvAtlas };
        Previews[i].PreviewUV.EndUV = { (Info.AtlasStartX + Info.AtlasSize) * InvAtlas, (Info.AtlasStartY + Info.AtlasSize) * InvAtlas };      
    }
   
}

void FShadowMapManager::BuildAtlas(TArray<FShadowInfo>& ShadowInfos)
{
    //constexpr uint32 Cellsize = 128;    // 가장 작은 섀도우 맵 크기
    //const uint32 CellsPerside = AtlasResolution / Cellsize; // 아틀라스 한 변의 셀 개수
    //const uint32 CellsPerPage = CellsPerside * CellsPerside;

    // 해상도 크기 순 - 내림차순으로 정렬
    TArray<uint32> Order;
    Order.SetNum(ShadowInfos.Num());
    std::iota(Order.begin(), Order.end(), 0);

    std::stable_sort(Order.begin(), Order.end(), [&ShadowInfos](int32 a, int32 b)
        {
            return ShadowInfos[a].AtlasSize > ShadowInfos[b].AtlasSize;
        });

    uint32 CurrentAtlasIndex = 0;
    uint32 CurrentX = 0;
    uint32 CurrentY = 0;
    NumAtlas = 1;   // 아틀라스 빌드할 때마다 1장으로 초기화

    for (int32 Index : Order)
    {
        FShadowInfo& Info = ShadowInfos[Index];
        const uint32 Size = Info.AtlasSize;

        Info.AtlasIndex = CurrentAtlasIndex;
        Info.AtlasStartX = CurrentX;
        Info.AtlasStartY = CurrentY;

        // 루프 안에서 설정해주어야 빈 맵이 안생김 (실제로 그릴때만 업데이트 하겠다)
        NumAtlas = CurrentAtlasIndex + 1;

        // 다음에 그릴 위치 계산
        uint32 Step = Size;
        while (Step < AtlasResolution)
        {
            const uint32 ParentSize = Step * 2;
            const bool bRight = CurrentX % ParentSize >= Step;
            const bool bBottom = CurrentY % ParentSize >= Step;

            if (!bRight)
            {
                CurrentX += Step;
                break;
            }
            if (!bBottom)
            {
                CurrentX -= Step;
                CurrentY += Step;
                break;
            }

            CurrentX -= Step;
            CurrentY -= Step;
            Step = ParentSize;
        }

        if (Step >= AtlasResolution)
        {
            ++CurrentAtlasIndex;
            CurrentX = 0;
            CurrentY = 0;
        }
    }
}

void FShadowMapManager::UnbindForShading()
{
    ID3D11ShaderResourceView* NullSRVs[2] = { nullptr, nullptr };
    ID3D11DeviceContext* Context = RenderCommand::GetContext();

    // t2: ShadowMapArray, t3: DirectionalLightShadowmapAtlas
    Context->PSSetShaderResources(2, 2, NullSRVs);
    // t12: StructuredShadowData
    Context->PSSetShaderResources(12, 1, &NullSRVs[0]);
}

bool FShadowMapManager::EnsureShadowInfoCapacity(uint32 RequiredCount)
{
    RequiredCount = std::max(RequiredCount, 1u);

    if (ShadowInfoBuffer && ShadowInfoSRV && RequiredCount <= ShadowInfoCapacity)
    {
        return true;
    }

    uint32 NewCapacity = std::max(ShadowInfoCapacity * 2, 64u);
    while (NewCapacity < RequiredCount)
    {
        NewCapacity *= 2;
    }

    ShadowInfoBuffer.Reset();
    ShadowInfoSRV.Reset();
    ShadowInfoCapacity = 0;

    D3D11_BUFFER_DESC BufferDesc{};
    BufferDesc.ByteWidth = NewCapacity * sizeof(FShadowInfo);
    BufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    BufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    BufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    BufferDesc.StructureByteStride = sizeof(FShadowInfo);

    ID3D11Device* Device = RenderCommand::GetDevice();

    if (FAILED(Device->CreateBuffer(&BufferDesc, nullptr, ShadowInfoBuffer.GetAddressOf())))
    {
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc{};
    SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    SRVDesc.Buffer.FirstElement = 0;
    SRVDesc.Buffer.NumElements = NewCapacity;

    if (FAILED(Device->CreateShaderResourceView(ShadowInfoBuffer.Get(), &SRVDesc, ShadowInfoSRV.GetAddressOf())))
    {
        ShadowInfoBuffer.Reset();
        return false;
    }

    ShadowInfoCapacity = NewCapacity;
    return true;
}
