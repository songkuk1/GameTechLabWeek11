#pragma once

#include "Texture2D.h"


struct FSceneRenderTargets
{
	TUniquePtr<FTexture2D> SceneColor; // R16G16B16A16_FLOAT, Linear
	TUniquePtr<FTexture2D> DecalTexture; // R16G16B16A16_FLOAT, Linear 색상 + 알파
	TUniquePtr<FTexture2D> DecalNormal; // R16G16B16A16_FLOAT
	TUniquePtr<FTexture2D> Normal; // R16G16B16A16_FLOAT
	TUniquePtr<FTexture2D> ObjectMask; // R8_UINT
	TUniquePtr<FTexture2D> ViewColor; // R8G8B8A8_UNORM
	TUniquePtr<FTexture2D> Depth; // R24G8_TYPELESS
	TUniquePtr<FTexture2D> FxaaTemp;
	uint32 Width = 0;
	uint32 Height = 0;

	void Resize(uint32 InWidth, uint32 InHeight);
	void Release();
};
