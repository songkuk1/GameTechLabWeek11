#include "EnginePCH.h"
#include "SceneRenderTargets.h"

#include "RenderCommand.h"

void FSceneRenderTargets::Resize(uint32 InWidth, uint32 InHeight)
{
	if (InWidth == Width && InHeight == Height)
	{
		return;
	}

	// 에디터 View 슬롯과 게임 실행 경로에서 공통으로 사용하는 타깃 구성이다.
	D3D11_TEXTURE2D_DESC Desc{
		.Width = InWidth,
		.Height = InHeight,
		.MipLevels = 1,
		.ArraySize = 1,
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		.SampleDesc = {
			.Count = 1,
		},
		.Usage = D3D11_USAGE_DEFAULT,
		.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
	};

	ViewColor = RenderCommand::CreateTexture2D(Desc);
	FxaaTemp = RenderCommand::CreateTexture2D(Desc);

	Desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	SceneColor = RenderCommand::CreateTexture2D(Desc);
	DecalTexture = RenderCommand::CreateTexture2D(Desc);
	DecalNormal = RenderCommand::CreateTexture2D(Desc);
	Normal = RenderCommand::CreateTexture2D(Desc);

	Desc.Format = DXGI_FORMAT_R8_UINT;
	ObjectMask = RenderCommand::CreateTexture2D(Desc);

	Desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	Depth = RenderCommand::CreateTexture2D(Desc);

	Width = InWidth;
	Height = InHeight;
}

void FSceneRenderTargets::Release()
{
	SceneColor.reset();
	DecalTexture.reset();
	DecalNormal.reset();
	Normal.reset();
	ObjectMask.reset();
	ViewColor.reset();
	Depth.reset();
	FxaaTemp.reset();
	Width = 0;
	Height = 0;
}
