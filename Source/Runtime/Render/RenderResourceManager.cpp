#include "EnginePCH.h"
#include "RenderResourceManager.h"

#include "RenderCommand.h"

#include "RenderUtil.h"
#include "Asset/ModelData.h"
#include <cstddef>

void FRenderResourceManager::ScanShaders(const fs::path& ShaderRoot)
{
	std::error_code ErrorCode;

	if (!fs::exists(ShaderRoot, ErrorCode))
	{
		HTR_LOG(Error, "[Shader] Scan Root Not Found : {}", ShaderRoot.generic_string());
		return;
	}

	for (const fs::directory_entry& Entry : fs::recursive_directory_iterator(ShaderRoot))
	{
		if (!Entry.is_regular_file()) continue;
		if (Entry.path().extension() != ".hlsl") continue;
		if (Entry.path().filename() == "UberLit.hlsl") continue;
		if (Entry.path().filename() == "TileLightCulling.hlsl") continue;
		if (Entry.path().filename() == "SurfaceShader.hlsl") continue;
		// 현재 DBuffer 경로와 함께 실행하지 않는 이전 자체 조명 셰이더 제외
		if (Entry.path().filename() == "ScreenSpaceDecal.hlsl")
		{
			continue;
		}

		FString Path = Entry.path().generic_string();
		LoadOrCompileShader(Path);
	}
}

void FRenderResourceManager::Shutdown()
{
	Get().ShaderProgramMap.Empty();
}

FShaderProgram* FRenderResourceManager::GetShaderProgram(FName Key)
{
	if (FShaderProgramEntry* Found = Get().ShaderProgramMap.Find(Key))
		return Found->Program.get();

	HTR_LOG(Error, "[Shader] not found: {}", Key.ToString());

	if (FShaderProgramEntry* Fallback =
		Get().ShaderProgramMap.Find("Resources/Shader/DefaultShader.hlsl"))
		return Fallback->Program.get();

	return nullptr;
}

FShaderProgram* FRenderResourceManager::RegisterProgram(FName Key, const FShaderProgramDesc& Desc)
{
	FRenderResourceManager& Manager = Get();

	if (auto* Existing = Manager.ShaderProgramMap.Find(Key))
	{
		return Existing->Program.get();
	}

	TArray<D3D_SHADER_MACRO> Macros;
	for (const FShaderDefine& Define : Desc.Defines)
	{
		Macros.Add({ Define.Name.c_str(), Define.Value.c_str() });
	}
	Macros.Add({ nullptr, nullptr });

	const FShaderByteCode VSCode = RenderUtil::CompileShader(Desc.Path, Desc.VertexEntry.c_str(), EShaderType::Vertex, Macros.GetData());
	const FShaderByteCode PSCode = RenderUtil::CompileShader(Desc.Path, Desc.PixelEntry.c_str(), EShaderType::Pixel, Macros.GetData());
	if (!VSCode.IsValid() || !PSCode.IsValid())
	{
		HTR_LOG(Error, "[Shader] compile failed: {}", Key.ToString());
		return nullptr;
	}

	TUniquePtr<FVertexShader> VS = Desc.InputLayout.Num() ? RenderCommand::CreateVertexShader(VSCode, Desc.InputLayout) : RenderCommand::CreateVertexShader(VSCode);
	TUniquePtr<FPixelShader> PS = RenderCommand::CreatePixelShader(PSCode);
	if (!VS || !VS->IsValid() || !PS || !PS->IsValid())
	{
		HTR_LOG(Error, "[Shader] device create failed: {}", Key.ToString());
		return nullptr;
	}

	FShaderProgramEntry Entry;
	Entry.VertexShader = std::move(VS);
	Entry.PixelShader = std::move(PS);
	Entry.Program = MakeUnique<FShaderProgram>(Entry.VertexShader.get(), Entry.PixelShader.get());
	Manager.ShaderProgramMap[Key] = std::move(Entry);

	return Manager.ShaderProgramMap[Key].Program.get();
}

void FRenderResourceManager::LoadOrCompileShader(const FString& Path, const FString& PermutationSuffix, const D3D_SHADER_MACRO* Defines, const TArray<D3D11_INPUT_ELEMENT_DESC>& InputLayoutDescs)
{
	const FString Key = Path + PermutationSuffix;
	if (ShaderProgramMap.Find(Key))
		return;

	FString VSCSOPath;
	FString PSCSOPath;
	FShaderByteCode VSCode = RenderUtil::GetOrCompile(Path, "mainVS", EShaderType::Vertex, VSCSOPath, PermutationSuffix, Defines);
	FShaderByteCode PSCode = RenderUtil::GetOrCompile(Path, "mainPS", EShaderType::Pixel, PSCSOPath, PermutationSuffix, Defines);

	if (!VSCode.IsValid() || !PSCode.IsValid())
	{
		HTR_LOG(Error, "[Shader] compile failed: {}", Key);
		return;
	}

	TUniquePtr<FVertexShader> Vs;
	if (InputLayoutDescs.Num())
	{
		Vs = RenderCommand::CreateVertexShader(VSCode, InputLayoutDescs);
	}
	else
	{
		Vs = RenderCommand::CreateVertexShader(VSCode);
	}

	TUniquePtr<FPixelShader>  Ps = RenderCommand::CreatePixelShader(PSCode);

	if (!Vs || !Vs->IsValid() || !Ps || !Ps->IsValid())
	{
		HTR_LOG(Error, "[Shader] device create failed: {}", Key);
		return;
	}

	FShaderProgramEntry Entry;
	Entry.VertexShader = std::move(Vs);
	Entry.PixelShader = std::move(Ps);
	Entry.Program = MakeUnique<FShaderProgram>(Entry.VertexShader.get(), Entry.PixelShader.get());
	ShaderProgramMap[Key] = std::move(Entry);

	HTR_LOG(Info, "[Shader] loaded: {}", Key);
}

namespace
{
	const FString UberLitPath = "Resources/Shader/UberLit.hlsl";
	const char* const Digits[] = { "0", "1", "2", "3"};
}

FString FRenderResourceManager::GetUberLitSuffix(const FViewRenderMode& Mode, bool bHasNormalMap, bool bReceiveShadow, bool bUseSkinning)
{
	const FString SkinSuffix = bUseSkinning ? "_SKINNED" : "_STATIC";
	switch (Mode.ViewMode)
	{
	case EViewMode::Unlit:
	case EViewMode::Wireframe:
		return FString("_UNLIT") + SkinSuffix;
	case EViewMode::WorldNormal:
		return FString("_WORLDNORMAL")+ "_NM" + Digits[static_cast<int32>(bHasNormalMap)] + SkinSuffix;
	default:
		break;
	}

	const bool bUseNormalMap = (Mode.ShadingModel == EShadingModel::Phong && bHasNormalMap);

	return FString("_LM") + Digits[static_cast<int32>(Mode.LightingModel)]
		+ "_SM" + Digits[static_cast<int32>(Mode.ShadingModel)]
		+ "_NM" + Digits[static_cast<int32>(bUseNormalMap)]
		+ "_RS" + Digits[static_cast<int32>(bReceiveShadow)] + SkinSuffix;
}

FShaderProgram* FRenderResourceManager::GetUberLitProgram(const FViewRenderMode& Mode, bool bHasNormalMap, bool bReceiveShadow, bool bUseSkinning)
{
	return GetShaderProgram(UberLitPath + GetUberLitSuffix(Mode, bHasNormalMap, bReceiveShadow, bUseSkinning));
}

void FRenderResourceManager::LoadUberLitPermutations()
{
	// Slot 0: shared PNCT geometry. Slot 1: per-vertex bone indices, weights and count.
	const TArray<D3D11_INPUT_ELEMENT_DESC> SkinLayout = {
		{ "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, static_cast<UINT>(offsetof(FVertexPNCT, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, static_cast<UINT>(offsetof(FVertexPNCT, Normal)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT",      0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(FVertexPNCT, Tangent)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",        0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(FVertexPNCT, Color)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, static_cast<UINT>(offsetof(FVertexPNCT, UV)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_SINT,  1, static_cast<UINT>(offsetof(FModelVertexInfluences, BoneIndices)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, static_cast<UINT>(offsetof(FModelVertexInfluences, Weights)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BONECOUNT",    0, DXGI_FORMAT_R32_SINT,           1, static_cast<UINT>(offsetof(FModelVertexInfluences, InfluenceCount)), D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	const TArray<D3D11_INPUT_ELEMENT_DESC> EmptyLayout{};

	for (int32 Skin = 0; Skin < 2; ++Skin)
	{
		const bool bUseSkinning = Skin != 0;
		const auto& Layout = bUseSkinning ? SkinLayout : EmptyLayout;
		const D3D_SHADER_MACRO UnlitDefines[] = {
			{ "USE_SKINNING", Digits[Skin] },
			{ "VIEWMODE", Digits[static_cast<int32>(EViewMode::Unlit)] },
			{ nullptr, nullptr }          // 배열 끝
		};
		LoadOrCompileShader(UberLitPath, GetUberLitSuffix({ EViewMode::Unlit }, false, true, bUseSkinning), UnlitDefines, Layout);

		for (int32 NM = 0; NM < 2; ++NM)
		{
			const D3D_SHADER_MACRO WorldNormalDefines[] = {
				{ "USE_SKINNING", Digits[Skin] },
				{ "VIEWMODE",       Digits[static_cast<int32>(EViewMode::WorldNormal)] },
				{ "HAS_NORMAL_MAP", Digits[NM] },
				{ nullptr, nullptr }
			};
			LoadOrCompileShader(UberLitPath, GetUberLitSuffix({ EViewMode::WorldNormal }, NM, true, bUseSkinning), WorldNormalDefines, Layout);
		}

		for (int32 LM = 0; LM < 3; ++LM)          // Lambert, Phong, Blinn-Phong
		{
			for (int32 SM = 0; SM < 2; ++SM)      // Gouraud, Phong
			{
				for (int32 NM = 0; NM < 2; ++NM)
				{
					for (int32 RS = 0; RS < 2; ++RS) {
						if (SM == 0 && NM == 1) continue; // Gouraud shading + Normal Map 스킵
						const D3D_SHADER_MACRO Defines[] = {
							{ "USE_SKINNING", Digits[Skin] },
							{ "VIEWMODE",       Digits[static_cast<int32>(EViewMode::Lit)] },
							{ "LIGHTING_MODEL", Digits[LM] },
							{ "SHADING_MODEL",  Digits[SM] },
							{ "HAS_NORMAL_MAP", Digits[NM] },
							{ "RECEIVE_SHADOW", Digits[RS] },
							{ nullptr, nullptr }
						};
						const FViewRenderMode Mode{ EViewMode::Lit, static_cast<ELightingModel>(LM), static_cast<EShadingModel>(SM) };
						LoadOrCompileShader(UberLitPath, GetUberLitSuffix(Mode, NM, RS, bUseSkinning), Defines, Layout);
					}
				}
			}
		}
	}
}
