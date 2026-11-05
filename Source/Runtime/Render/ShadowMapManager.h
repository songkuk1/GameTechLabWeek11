#pragma once

#include "Render/LightInfo.h"
#include "Containers/Array.h"
#include "Buffer.h"
#include "ShadowFilterSettings.h"

class FLightSceneProxy;



class FShadowMapManager
{
public:
	uint32 GetNumShadowInfos() const { return NumShadowInfos; }

	void UploadShadowInfos(const TArray<FShadowInfo>& ShadowInfos);

	// 섀도 패스
	ID3D11DepthStencilView* GetSliceDSV(uint32 Slice) const;
	ID3D11ShaderResourceView* GetShadowMapArraySRV() const;
	ID3D11ShaderResourceView* GetShadowInfoSRV() const;
	uint32 GetResolution() const { return Resolution; }
	uint32 GetNumSlices() const { return NumSlices; }
	uint32 GetDirectionalResolution() const { return DirectionalResolution; }
	ID3D11ShaderResourceView* GetDirectionalShadowSRV() const { return DirectionalShadowSRV.Get(); }
		
	//그리기 전 바인딩
	void BeginShadowRender(uint32 Slice);

	void ClearAtlas();

	//그리고 나서 해제
	void EndShadowPass();

	//바인딩 해제
	void UnbindForShading();
	//그리고 나서 바인딩
	void BindForShading();

	// 그림자 필터 옵션(b6). 값이 바뀌었을 때만 다음 BindForShading에서 GPU에 올린다.
	void SetFilterSettings(const FShadowFilterSettings& Settings);

	//void SetupDirectionalShadow(const FShadowInfo& DirectionalShadow);

	void ComputeCascadeSpheres(
		const FVector& CameraPosition,
		const FVector& CameraForward,
		float FovDegrees,
		float AspectRatio,
		float NearClip,
		float ShadowMaxDist,
		FVector4 OutCascadeSpheres[4]);
	void ComputeCascadeLightMatrices(
		const FVector& LightDirection,
		FVector4 InOutCascadeSpheres[4],
		uint32 CascadeResolution,
		FMatrix OutCascadeMatrices[4],
		float OutCascadeRanges[4] = nullptr);
	void SetupDirectionalShadow(
		const FShadowInfo& DirectionalShadow,
		const FVector& LightDirection,
		const FVector& CameraPosition,
		const FVector& CameraForward,
		float FovDegrees,
		float AspectRatio,
		bool bVisualizeCascades = false);
	void BeginDirectionalCascadeRender(uint32 CascadeIndex);

	void BeginDirectionalShadowRender();
	void EndDirectionalShadowRender();

	const FMatrix& GetCascadeMatrix(uint32 CascadeIndex) const { return CachedCascadeMatrices[CascadeIndex]; }

	//Imgui
	void UpdatePreview(FLightSceneProxy* PreviewShadowMapProxy, const TArray<FShadowInfo>& ShadowInfos);

	// Atlas
	void BuildAtlas(TArray<FShadowInfo>& ShadowInfos);

	void BeginShadowRenderWithAtlas(const FShadowInfo& ShadowInfo);

	uint32 GetAtlasResolution() { return AtlasResolution; }
	uint32 GetNumAtlas() { return NumAtlas; }

private:
	// 해상도, 층 수가 바뀔 때만 리소스와 뷰를 다시 만든다
	bool EnsureShadowMaps(uint32 InResolution, uint32 InNumSlices);
	
	bool EnsureShadowInfoCapacity(uint32 RequiredCount);	
	bool EnsureDirectionalShadowMap(uint32 InResolution);

	// 아틀라스 섀도우 맵 만들기
	bool EnsureShadowMapAtlas(uint32 InResolution, uint32 InNumSlices);

	ComPtr<ID3D11Buffer> ShadowInfoBuffer;
	ComPtr<ID3D11ShaderResourceView> ShadowInfoSRV;

	ComPtr<ID3D11Texture2D> ShadowMapArrayBuffer;
	ComPtr<ID3D11ShaderResourceView> ShadowMapArraySRV;
	TArray<ComPtr<ID3D11DepthStencilView>> ShadowDepthView;

	ComPtr<ID3D11Texture2D> ShadowMapAtlasBuffer;
	ComPtr<ID3D11ShaderResourceView> ShadowMapAtlasArraySRV;
	TArray<ComPtr<ID3D11DepthStencilView>> ShadowAtlasDepthView;
	
	ComPtr<ID3D11Texture2D> DirectionalShadowTexture;
	ComPtr<ID3D11ShaderResourceView> DirectionalShadowSRV;
	ComPtr<ID3D11DepthStencilView> DirectionalShadowDSV;
	TUniquePtr<FConstantBuffer> DirectionalShadowCB;


	// 그림자 필터 옵션 (b6)
	TUniquePtr<FConstantBuffer> FilterCB;
	FShadowFilterConstants FilterConstants{};
	bool bFilterDirty = true;
	//uint32 DirectionalResolution = 2048;

	FMatrix CachedCascadeMatrices[4] = {};
	float CachedCascadeRanges[4] = {};
	uint32 DirectionalResolution = 4096;


	uint32 ShadowInfoCapacity = 0;
	uint32 NumShadowInfos = 0;	

	uint32 ShadowMapCapacity = 0;
	uint32 NumShadowMaps = 0;

	uint32 Resolution = 1024;
	uint32 NumSlices = 0;

	uint32 AtlasResolution = 4096;
	uint32 NumAtlas = 1;

	// 프리뷰는 섀도우 매니저가 들고 있는다.
	TArray<ComPtr<ID3D11Texture2D>> PreviewShadowTextures;
	TArray<ComPtr<ID3D11ShaderResourceView>> PreviewShadowSRVs;
	// Point Light 는 섀도우 맵이 6개임. 아틀라스 여러개에 걸쳐있을 수 있기 때문에 Array로 관리.
};
