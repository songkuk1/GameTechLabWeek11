#include "EnginePCH.h"
#include "RenderUtil.h"
#include <algorithm>
#include <fstream>

namespace
{
#ifdef _DEBUG
	constexpr uint32 ShaderCompileFlags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
	constexpr const char* ShaderCacheDir = "Intermediate/Shaders/Debug";
#else
	constexpr uint32 ShaderCompileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
	constexpr const char* ShaderCacheDir = "Intermediate/Shaders/Release";
#endif
}

FShaderByteCode RenderUtil::CompileShader(FString Path, const char* EntryPoint, EShaderType ShaderType, const D3D_SHADER_MACRO* Defines)
{
	const char* Target;
	switch (ShaderType)
	{
	case EShaderType::Vertex:
		Target = "vs_5_0";
		break;
	case EShaderType::Pixel:
		Target = "ps_5_0";
		break;
	case EShaderType::Compute:
		Target = "cs_5_0";
		break;
	}

	FShaderByteCode ByteCode;

	std::wstring WPath(Path.begin(), Path.end());

	ComPtr<ID3DBlob> ErrorBlob;
	HRESULT hr = D3DCompileFromFile(WPath.c_str(), Defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, EntryPoint, Target,
		ShaderCompileFlags, 0, ByteCode.Blob.GetAddressOf(), ErrorBlob.GetAddressOf());

	if (FAILED(hr))
	{
		if (ErrorBlob)
			OutputDebugStringA((const char*)ErrorBlob->GetBufferPointer());
		else
			OutputDebugStringA(("[Shader] file not found: " + Path + "\n").c_str());

		ByteCode.Blob.Reset();
	}
	return ByteCode;
}

void RenderUtil::SaveByteCode(FString CsoPath, const FShaderByteCode& ByteCode)
{
	fs::create_directories(fs::path(CsoPath).parent_path());
	std::ofstream Out(CsoPath, std::ios::binary);
	if (!Out) { fs::remove(CsoPath); return; }
	Out.write(reinterpret_cast<const char*>(ByteCode.GetData()), ByteCode.GetSize());
}

FShaderByteCode RenderUtil::LoadByteCode(const FString& CsoPath)
{
	FShaderByteCode Result;

	std::ifstream In(CsoPath, std::ios::binary | std::ios::ate);
	if (!In) return Result;

	const std::streamsize Size = In.tellg();
	In.seekg(0);
	if (Size <= 0) return Result;

	ComPtr<ID3DBlob> Blob;
	if (FAILED(D3DCreateBlob(static_cast<SIZE_T>(Size), &Blob))) return Result;

	In.read(reinterpret_cast<char*>(Blob->GetBufferPointer()), Size);
	if (In) Result.Blob = std::move(Blob);
	return Result;
}

FString RenderUtil::GetCsoPath(const FString& ShaderPath, EShaderType ShaderType, const FString& PermutationSuffix)
{
	const fs::path Rel = fs::relative(ShaderPath, "Resources/Shader");
	const char* Suffix;
	switch (ShaderType)
	{
	case EShaderType::Vertex:
		Suffix = "_VS.cso";
		break;
	case EShaderType::Pixel:
		Suffix = "_PS.cso";
		break;
	case EShaderType::Compute:
		Suffix = "_CS.cso";
		break;
	}

	fs::path Out = fs::path(ShaderCacheDir) / Rel.parent_path();
	Out /= Rel.stem().generic_string() + PermutationSuffix + Suffix;
	return Out.generic_string();
}

// .cso 파일이 최신 컴파일인지 비교한다.
bool RenderUtil::IsCsoUpToDate(const FString& ShaderPath, const FString& CsoPath)
{
	std::error_code Ec;
	const auto CsoTime = fs::last_write_time(CsoPath, Ec);
	if (Ec){ return false; }

	// 중복 검사와 순환 참조 방지를 위한 방문 경로
	TSet<FString> Visited;
	const auto IsSourceUpToDate = [&](auto&& Self, const fs::path& SourcePath) -> bool
		{
			const fs::path Path = SourcePath.lexically_normal();
			if (!Visited.Add(Path.generic_string())){ return true; }

			const auto SourceTime = fs::last_write_time(Path, Ec);
			if (Ec || SourceTime >= CsoTime){ return false; }

			std::ifstream Input(Path);
			if (!Input){ return false; }

			// 프로젝트의 문자열 리터럴 기반 로컬 include 추적
			FString Line;
			while (std::getline(Input, Line))
			{
				const size_t Prefix = Line.find_first_not_of(" \t");
				if (Prefix == FString::npos || Line[Prefix] != '#'){ continue; }

				const size_t Directive = Line.find_first_not_of(" \t", Prefix + 1);
				if (Directive == FString::npos || Line.compare(Directive, 7, "include") != 0){ continue; }

				const size_t Begin = Line.find_first_not_of(" \t", Directive + 7);
				const size_t End = Begin == FString::npos ? FString::npos : Line.find('"', Begin + 1);
				if (Begin == FString::npos || Line[Begin] != '"' || End == FString::npos){ return false; }

				const fs::path IncludePath = Path.parent_path() / Line.substr(Begin + 1, End - Begin - 1);
				if (!Self(Self, IncludePath)){ return false; }
			}
			return !Input.bad();
		};
	return IsSourceUpToDate(IsSourceUpToDate, fs::path(ShaderPath));
}

FShaderByteCode RenderUtil::GetOrCompile(const FString& ShaderPath, const char* EntryPoint, EShaderType ShaderType, FString& OutCSOPath,
	const FString& PermutationSuffix, const D3D_SHADER_MACRO* Defines)
{
	OutCSOPath = GetCsoPath(ShaderPath, ShaderType, PermutationSuffix);

	if (IsCsoUpToDate(ShaderPath, OutCSOPath))
	{
		FShaderByteCode Cached = LoadByteCode(OutCSOPath);
		if (Cached.IsValid()) return Cached;
	}

	FShaderByteCode Compiled = CompileShader(ShaderPath, EntryPoint, ShaderType, Defines);
	if (Compiled.IsValid())
		SaveByteCode(OutCSOPath, Compiled);

	return Compiled;

}
