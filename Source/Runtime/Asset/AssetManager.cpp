#include "EnginePCH.h"
#include "AssetManager.h"
#include "Render/Buffer.h"
#include "Render/Material.h"

#include "Render/Vertex.h"
#include "Render/Texture2D.h"
#include "Render/RenderCommand.h"

#include "Render/RenderResourceManager.h"

#include "Asset/ObjImporter/ObjImporter.h"

#include "Render/GeometryGenerator.h"
#include "UObject/UObjectGlobals.h"

#include "Render/ImageLoader.h"
#include "Core/ResourcePaths.h"

#include "Sequence/LevelSequence.h"
#include "Serialization/JsonArchive.h"

#include "Asset/FbxImporter/FbxImporter.h"
#include "Asset/ModelBake.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AnimationGraph.h"
#include "Render/SkeletalMesh.h"
#include "Core/PathUtils.h"

namespace
{
	void SetMaterialTexture(int32 SlotIndex, UMaterial* Material,
		const FString& TexturePath, bool bGenerateMipmaps = true, bool bSRGB = true)
	{
		while (static_cast<int32>(Material->Textures.Num()) <= SlotIndex)
		{
			Material->Textures.Add(nullptr);
		}
		if (!TexturePath.empty())
		{
			UTexture2D* Texture = UAssetManager::Get().LoadTexture(TexturePath, bGenerateMipmaps, bSRGB);
			Material->Textures[SlotIndex] = Texture;
		}
	}

	UStaticMesh* CreateStaticMesh(const FModelMeshData* MeshData)
	{
		if (!MeshData || !MeshData->Vertices.Num() || !MeshData->Indices.Num())
			return nullptr;

		FModelMeshData Optimized = *MeshData;

		// Mesh에 MaterialSlot 없을 경우 Default Material 할당
		if (Optimized.MaterialSlots.IsEmpty())
		{
			FMeshMaterialSlot DefaultSlot;
			DefaultSlot.Name = "Default";
			Optimized.MaterialSlots.Add(DefaultSlot);
		}

		// Mesh에 Section이 없을 경우 전체 메쉬을 섹션 하나로 세팅
		if (Optimized.Sections.IsEmpty())
		{
			FMeshSection DefaultSection;
			DefaultSection.StartIndex = 0;
			DefaultSection.IndexCount = static_cast<uint32>(Optimized.Indices.Num());
			DefaultSection.MaterialSlotIndex = 0;

			Optimized.Sections.Add(DefaultSection);
		}

		Optimized.OptimizeTriangleOrderForVertexCache();
		TUniquePtr<FVertexBuffer> VB = RenderCommand::CreateStaticVertexBuffer(Optimized.Vertices.GetData(), sizeof(FVertexPNCT) * static_cast<uint32>(Optimized.Vertices.size()), sizeof(FVertexPNCT));
		TUniquePtr<FIndexBuffer> IB = RenderCommand::CreateStaticIndexBuffer(Optimized.Indices.GetData(), static_cast<uint32>(Optimized.Indices.size()));
		if (VB == nullptr || IB == nullptr) return nullptr;

		UStaticMesh* Mesh = NewObject<UStaticMesh>();
		Mesh->MeshData = std::move(Optimized);

		// Vertex/Index GPU 업로드
		Mesh->VertexBuffer = std::move(VB);
		Mesh->IndexBuffer = std::move(IB);

		UMaterial* DefaultMaterial = UAssetManager::GetAssetByPath<UMaterial>("DefaultMaterial");
		if (!DefaultMaterial)
		{
			return nullptr;
		}

		// MaterialSlots를 실제 UMaterial로 변환
		for (const FMeshMaterialSlot& Slot : Mesh->MeshData.MaterialSlots)
		{
			UMaterial* Material = UMaterial::CreateInstance(DefaultMaterial);
			if (!Material)
			{
				return nullptr;
			}
			Material->BaseColor = Slot.BaseColor;
			Material->Shininess = Slot.Shininess;
			Material->SpecularColor = Slot.SpecularColor;
			if (Material->BaseColor.A < 1.0f)
			{
				Material->BlendState = EBlendState::AlphaBlend;
				Material->DepthStencilState = EDepthStencilState::ReadOnly;
			}

			if (!Slot.DiffuseTexturePath.empty())
			{
				UTexture2D* Texture = UAssetManager::Get().LoadTexture(Slot.DiffuseTexturePath);

				if (Texture)
				{
					Material->Textures[0] = Texture;
				}
			}
			SetMaterialTexture(1, Material, Slot.NormalTexturePath, true, false);
			Mesh->Materials.Add(Material);
		}

		Mesh->MeshData.BuildTriangleBVH();
		Mesh->RebuildRenderData();

		return Mesh;
	}

	USkeleton* CreateSkeleton(const FModelData& ModelData)
	{
		const TArray<FModelNode>& Nodes = ModelData.Nodes;
		const TArray<FModelBone>& Bones = ModelData.Bones;

		USkeleton* Skeleton = NewObject<USkeleton>();
		Skeleton->Initialize(Nodes, Bones);

		return Skeleton;
	}

	USkeletalMesh* CreateSkeletalMesh(const FModelData& ModelData)
	{
		const FModelMeshData& MeshData = ModelData.MeshData;
		const TArray<FModelVertexInfluences>& VertexInfluencesList = ModelData.VertexInfluencesList;
		const TArray<FModelMesh>& Meshs = ModelData.Meshs;

		USkeletalMesh* Mesh = NewObject<USkeletalMesh>();
		Mesh->Initialize(MeshData, VertexInfluencesList, Meshs);

		UMaterial* DefaultMaterial = UAssetManager::GetAssetByPath<UMaterial>("DefaultMaterial");

		// MaterialSlots를 실제 UMaterial로 변환
		TArray<UMaterial*> Materials;
		for (const FMeshMaterialSlot& Slot : MeshData.MaterialSlots)
		{
			UMaterial* Material = UMaterial::CreateInstance(DefaultMaterial);
			Material->BaseColor = Slot.BaseColor;
			Material->Shininess = Slot.Shininess;
			Material->SpecularColor = Slot.SpecularColor;
			if (Material->BaseColor.A < 1.0f)
			{
				Material->BlendState = EBlendState::AlphaBlend;
				Material->DepthStencilState = EDepthStencilState::ReadOnly;
			}

			if (!Slot.DiffuseTexturePath.empty())
			{
				UTexture2D* Texture = UAssetManager::Get().LoadTexture(Slot.DiffuseTexturePath);
				if (Texture)
				{
					Material->Textures[0] = Texture;
				}
			}

			SetMaterialTexture(1, Material, Slot.NormalTexturePath, true, false);

			Materials.Add(Material);
		}
		Mesh->SetMaterials(Materials);

		return Mesh;
	}

	FString MakeAssetKey(const FString& Path)
	{
		return fs::relative(Path, "Assets").generic_string();
	}

	UAnimSequence* CreateAnimationSequence(const FModelAnimation& ModelAnimation, const TArray<FModelAnimationNotify>& ModelNotifies, USkeleton* Skeleton)
	{
		if (!Skeleton)
		{
			return nullptr;
		}

		UAnimSequence* Animation = NewObject<UAnimSequence>();
		Animation->SetName(ModelAnimation.Name);
		Animation->SetSkeleton(Skeleton);

		UAnimDataModel* DataModel = Animation->GetDataModel();
		if (!DataModel)
		{
			delete Animation;
			return nullptr;
		}

		DataModel->PlayLength = ModelAnimation.PlayLength;
		DataModel->FrameRate.Numerator = ModelAnimation.FrameRateNumerator;
		DataModel->FrameRate.Denominator = ModelAnimation.FrameRateDenominator;
		DataModel->NumberOfFrames = ModelAnimation.NumberOfFrames;
		DataModel->NumberOfKeys = ModelAnimation.NumberOfKeys;
		DataModel->CurveData.Curves = ModelAnimation.CurveData.Curves;

		auto NormalizeBoneName = [](const FString& InName) -> FString
		{
			FString Out;
			Out.reserve(InName.size());
			for (char c : InName)
			{
				Out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
			}
			size_t UnderscorePos = 0;
			while ((UnderscorePos = Out.find('_', UnderscorePos)) != FString::npos)
			{
				size_t DigitStart = UnderscorePos + 1;
				if (DigitStart < Out.size() && Out[DigitStart] == '0')
				{
					size_t NonZero = Out.find_first_not_of('0', DigitStart);
					if (NonZero != FString::npos && std::isdigit(static_cast<unsigned char>(Out[NonZero])))
					{
						Out.erase(DigitStart, NonZero - DigitStart);
					}
				}
				UnderscorePos++;
			}
			return Out;
		};

		const TArray<FModelNode>& SkeletonNodes = Skeleton->GetNodes();
		TMap<FString, int32> NameToNodeMap;
		TMap<FString, int32> NormNameToNodeMap;
		for (int32 i = 0; i < SkeletonNodes.Num(); ++i)
		{
			NameToNodeMap.Add(SkeletonNodes[i].Name, i);
			NormNameToNodeMap.Add(NormalizeBoneName(SkeletonNodes[i].Name), i);
		}

		for (const FModelAnimationTrack& ModelTrack : ModelAnimation.Tracks)
		{
			FBoneAnimationTrack BoneTrack;
			BoneTrack.Name = ModelTrack.Name;
			const FString RawTrackName = ModelTrack.Name.ToString();
			int32 MappedNodeIndex = ModelTrack.NodeIndex;

			if (const int32* FoundIndex = NameToNodeMap.Find(RawTrackName))
			{
				MappedNodeIndex = *FoundIndex;
			}
			else if (const int32* FoundNormIndex = NormNameToNodeMap.Find(NormalizeBoneName(RawTrackName)))
			{
				MappedNodeIndex = *FoundNormIndex;
			}
			BoneTrack.NodeIndex = MappedNodeIndex;
			BoneTrack.InternalTrack.PosKeys = ModelTrack.PosKeys;
			BoneTrack.InternalTrack.RotKeys = ModelTrack.RotKeys;
			BoneTrack.InternalTrack.ScaleKeys = ModelTrack.ScaleKeys;

			// 스케일 불일치 보정 (예: 순수 애니메이션 FBX에서 root 스케일이 0.01인데 스켈레톤 root 스케일은 1.0인 경우)
			if (MappedNodeIndex >= 0 && MappedNodeIndex < SkeletonNodes.Num() && !BoneTrack.InternalTrack.ScaleKeys.IsEmpty())
			{
				const FMatrix& NodeTransform = SkeletonNodes[MappedNodeIndex].LocalTransform;
				const float NodeScaleX = FVector(NodeTransform.M[0][0], NodeTransform.M[0][1], NodeTransform.M[0][2]).Size();
				const float TrackScaleX = BoneTrack.InternalTrack.ScaleKeys[0].X;

				if (TrackScaleX > 0.0f && NodeScaleX > 0.0f)
				{
					const float Ratio = NodeScaleX / TrackScaleX;
					if (Ratio > 50.0f && Ratio < 150.0f)
					{
						for (FVector& SKey : BoneTrack.InternalTrack.ScaleKeys)
						{
							SKey *= Ratio;
						}
						for (FVector& PKey : BoneTrack.InternalTrack.PosKeys)
						{
							PKey *= Ratio;
						}
					}
				}
			}

			DataModel->BoneAnimationTracks.Add(std::move(BoneTrack));
		}

		for (const FModelAnimationNotify& Notify : ModelNotifies)
		{
			Animation->AddNotify(Notify.TriggerTime, Notify.Duration, Notify.NotifyName);
		}

		return Animation;
	}
}

UAssetManager& UAssetManager::Get()
{
	static UAssetManager* Instance = NewObject<UAssetManager>();
	return *Instance;
}

void UAssetManager::Init(const FAssetLoadProgress& OnProgress)
{
	FGeometryGenerator::CreateDefaultMeshDatas();
	// 머티리얼이 참조하므로 반드시 먼저 만든다
	Get().CreateDefaultTextures();
	Get().CreateDefaultMaterial();
	Get().LoadFontAtlas(ResourcePaths::DefaultFont, ResourcePaths::DefaultFontAtlas);
	Get().ScanAssets("Assets", OnProgress);
	Get().CreateDefaultMeshes();
	Get().CreateParticleMaterial();
	Get().CreateBillboardMaterial();
	Get().CreateDecalMaterial();
}

void UAssetManager::ScanAssets(const fs::path& AssetRoot, const FAssetLoadProgress& OnProgress)
{
	if (!fs::exists(AssetRoot))
	{
		HTR_LOG(Warning, "Asset root not found: {}", AssetRoot.generic_string());
		return;
	}

	TArray<fs::path> Files;

	for (const fs::directory_entry& Entry : fs::recursive_directory_iterator(AssetRoot))
	{
		if (Entry.is_regular_file())
		{
			Files.Add(Entry.path());
		}
	}

	const int32 Total = Files.Num();
	int32 Completed = 0;

	TArray<fs::path> ModelBakeFiles;
	TArray<fs::path> AnimationFiles;
	TArray<fs::path> BlendSpaceFiles;
	TArray<fs::path> AnimationGraphFiles;

	auto ReportProgress = [&](const fs::path& FilePath)
		{
			++Completed;

			if (OnProgress)
			{
				const FString Key = PathUtils::ToUtf8(fs::relative(FilePath, AssetRoot));
				OnProgress(Completed, Total, Key);
			}
		};

	for (const fs::path& FilePath : Files)
	{
		const FString Key = PathUtils::ToUtf8(fs::relative(FilePath, AssetRoot));
		const FString Path = PathUtils::ToUtf8(FilePath);
		const FString Extension = PathUtils::ToUtf8(FilePath.extension());

		AssetPathMap.Add(Key, Path);

		// OBJ / FBX Bake 파일은 원본과 분리해서 나중에 로드한다.
		if (_stricmp(Extension.c_str(), ".bin") == 0)
		{
			fs::path SourcePath = FilePath;
			SourcePath.replace_extension();

			const FString SourceExtension = PathUtils::ToUtf8(SourcePath.extension());

			if (_stricmp(SourceExtension.c_str(), ".obj") == 0 || _stricmp(SourceExtension.c_str(), ".fbx") == 0)
			{
				ModelBakeFiles.Add(FilePath);
			}

			ReportProgress(FilePath);
			continue;
		}

		// 원본 OBJ / FBX는 시작 시 자동 임포트하지 않는다.
		if (_stricmp(Extension.c_str(), ".obj") == 0 || _stricmp(Extension.c_str(), ".fbx") == 0)
		{
			ReportProgress(FilePath);
			continue;
		}

		// 이미지 파일은 시작 시 일괄 로드하지 않고, 머티리얼/뷰어에서 필요로 할 때 GetAssetByPath로 자동 로드 (Lazy Loading)
		if (_stricmp(Extension.c_str(), ".png") == 0 || _stricmp(Extension.c_str(), ".jpg") == 0 ||
			_stricmp(Extension.c_str(), ".jpeg") == 0 || _stricmp(Extension.c_str(), ".hdr") == 0)
		{
			ReportProgress(FilePath);
			continue;
		}

		if (_stricmp(Extension.c_str(), ".anim") == 0)
		{
			AnimationFiles.Add(FilePath);
			ReportProgress(FilePath);
			continue;
		}

		if (_stricmp(Extension.c_str(), ".blendspace") == 0)
		{
			BlendSpaceFiles.Add(FilePath);
			continue;
		}

		if (_stricmp(Extension.c_str(), ".animgraph") == 0)
		{
			AnimationGraphFiles.Add(FilePath);
			continue;
		}

		LoadAsset(Key, Path);
		ReportProgress(FilePath);
	}

	// 1. 모델 Bake 로드
	for (const fs::path& FilePath : ModelBakeFiles)
	{
		fs::path SourcePath = FilePath;
		SourcePath.replace_extension();

		const FString SourcePathString = PathUtils::ToUtf8(SourcePath);
		const FString SourceExtension = PathUtils::ToUtf8(SourcePath.extension());

		if (_stricmp(SourceExtension.c_str(), ".obj") == 0)
		{
			LoadObjStaticMesh(SourcePathString, false);
		}
		else if (_stricmp(SourceExtension.c_str(), ".fbx") == 0)
		{
			LoadFbxModel(SourcePathString, false, false, false);
		}
	}

	for (const fs::path& FilePath : AnimationFiles)
	{
		const FString Path = PathUtils::ToUtf8(FilePath);
		LoadAnimSequence(Path);
	}

	// 2. BlendSpace 로드
	for (const fs::path& FilePath : BlendSpaceFiles)
	{
		const FString Key = PathUtils::ToUtf8(fs::relative(FilePath, AssetRoot));
		const FString Path = PathUtils::ToUtf8(FilePath);

		LoadAsset(Key, Path);
		ReportProgress(FilePath);
	}

	// 3. Animation Graph 로드
	for (const fs::path& FilePath : AnimationGraphFiles)
	{
		const FString Key = PathUtils::ToUtf8(fs::relative(FilePath, AssetRoot));
		const FString Path = PathUtils::ToUtf8(FilePath);

		LoadAsset(Key, Path);
		ReportProgress(FilePath);
	}
}

void UAssetManager::LoadAsset(const FString& Key, const FString& Path)
{
	FString Extension = PathUtils::ToUtf8(PathUtils::FromUtf8(Path).extension());
	if (_stricmp(Extension.c_str(), ".png") == 0 || _stricmp(Extension.c_str(), ".jpg") == 0 || _stricmp(Extension.c_str(), ".jpeg") == 0)
	{
		if (UTexture2D* Texture = LoadTexture(Path))
		{
			AssetMap[Key] = Texture;
		}
	}
	else if (Extension == ".json")
	{
		FString TexturePath = PathUtils::ToUtf8(PathUtils::FromUtf8(Path).replace_extension(".png"));
		if (UFont* Font = LoadFontAtlas(Path, TexturePath))
		{
			AssetMap[Key] = Font;
		}
	}
	else if (Extension == ".sequence")
	{
		LoadLevelSequence(Path);
	}
	else if (Extension == ".blendspace")
	{
		LoadBlendSpace(Path);
	}
	else if (Extension == ".animgraph")
	{
		LoadAnimationGraph(Path);
	}
}

void UAssetManager::CreateDefaultTextures()
{
	// 텍스처가 지정되지 않은 머티리얼이 검게 나오지 않도록 하는 1x1 흰색 텍스처.
	// 셰이더에서 곱해도 결과가 변하지 않으므로 "텍스처 없음"의 기본값으로 쓴다.
	const uint32 WhitePixel = 0xFFFFFFFF;

	D3D11_TEXTURE2D_DESC Desc{};
	Desc.Width = 1;
	Desc.Height = 1;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_IMMUTABLE;
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	TUniquePtr<FTexture2D> Resource = RenderCommand::CreateTexture2D(Desc, &WhitePixel);
	if (!Resource)
	{
		return;
	}

	UTexture2D* WhiteTexture = NewObject<UTexture2D>();
	WhiteTexture->SetResource(std::move(Resource));
	WhiteTexture->SetPath("WhiteTexture");

	AssetMap["WhiteTexture"] = WhiteTexture;

	// 노멀맵이 지정되지 않은 머티리얼을 위한 1x1 평면 노멀맵 텍스처.
	// uint32 리터럴로 쓰면 리틀 엔디안 때문에 RGBA 순서가 뒤집히므로 바이트 순서대로 적는다
	const uint8 FlatNormalPixel[4] = { 128, 128, 255, 255 }; // RGBA => (0.5, 0.5, 1.0)

	D3D11_TEXTURE2D_DESC NormalDesc{};
	NormalDesc.Width = 1;
	NormalDesc.Height = 1;
	NormalDesc.MipLevels = 1;
	NormalDesc.ArraySize = 1;
	NormalDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	NormalDesc.SampleDesc.Count = 1;
	NormalDesc.Usage = D3D11_USAGE_IMMUTABLE;
	NormalDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	TUniquePtr<FTexture2D> NormalResource = RenderCommand::CreateTexture2D(NormalDesc, FlatNormalPixel);
	if (!NormalResource)
	{
		return;
	}

	UTexture2D* FlatNormalTexture = NewObject<UTexture2D>();
	FlatNormalTexture->SetResource(std::move(NormalResource));
	FlatNormalTexture->SetPath("FlatNormalTexture");

	AssetMap["FlatNormalTexture"] = FlatNormalTexture;

	struct FColorPixel {
		uint8 R;
		uint8 G;
		uint8 B;
		uint8 A;
	};

	FColorPixel HeatMapLookUpPixels[256];

	const FVector MagentaColor(160.0f / 255.0f, 32.0f / 255.0f, 240.0f / 255.0f); // 가중치 0 (보라/마젠타)
	const FVector BlueColor(0.0f, 0.05f, 0.95f);                                 // 미세 가중치 (파랑)
	const FVector CyanColor(0.0f, 0.95f, 0.95f);                                 // 저가중치 (하늘)
	const FVector GreenColor(0.1f, 0.95f, 0.1f);                                 // 중간 (초록)
	const FVector YellowColor(1.0f, 0.90f, 0.0f);                                // 중상 (노랑)
	const FVector RedColor(1.0f, 0.05f, 0.05f);                                  // 고가중치 (빨강)

	for (int32 i = 0; i < 256; ++i)
	{
		float t = static_cast<float>(i) / 255.0f;
		FVector Color;

		// 1. 0.0 부근의 절벽 제거: 보라 -> 파랑으로 부드럽게 Lerp 전환 (삼각형 톱니 현상 제거)
		if (t < 0.08f)
		{
			float u = t / 0.08f;
			// 3차 Hermite 커브(Smoothstep)로 가중치 0 경계의 기울기를 부드럽게 시작
			u = u * u * (3.0f - 2.0f * u);
			Color = FMath::Lerp(MagentaColor, BlueColor, u);
		}
		// 2. 파랑 -> 하늘 (0.08 ~ 0.22)
		else if (t < 0.22f)
		{
			float u = (t - 0.08f) / (0.22f - 0.08f);
			Color = FMath::Lerp(BlueColor, CyanColor, u);
		}
		// 3. 하늘 -> 초록 (0.22 ~ 0.40)
		else if (t < 0.40f)
		{
			float u = (t - 0.22f) / (0.40f - 0.22f);
			Color = FMath::Lerp(CyanColor, GreenColor, u);
		}
		// 4. 초록 -> 노랑 (0.40 ~ 0.55)
		else if (t < 0.55f)
		{
			float u = (t - 0.40f) / (0.55f - 0.40f);
			Color = FMath::Lerp(GreenColor, YellowColor, u);
		}
		// 5. 노랑 -> 빨강 (0.55 ~ 0.72): 트위스트 본 가중치(0.6)에서도 언리얼처럼 붉은색 도달
		else if (t < 0.72f)
		{
			float u = (t - 0.55f) / (0.72f - 0.55f);
			Color = FMath::Lerp(YellowColor, RedColor, u);
		}
		// 6. 0.72 ~ 1.0: 순수 레드
		else
		{
			Color = RedColor;
		}

		HeatMapLookUpPixels[i].R = static_cast<uint8>(FMath::Clamp(Color.X * 255.0f + 0.5f, 0.0f, 255.0f));
		HeatMapLookUpPixels[i].G = static_cast<uint8>(FMath::Clamp(Color.Y * 255.0f + 0.5f, 0.0f, 255.0f));
		HeatMapLookUpPixels[i].B = static_cast<uint8>(FMath::Clamp(Color.Z * 255.0f + 0.5f, 0.0f, 255.0f));
		HeatMapLookUpPixels[i].A = 255;
	}

	D3D11_TEXTURE2D_DESC HeatMapLookUpDesc{};
	HeatMapLookUpDesc.Width = 256;
	HeatMapLookUpDesc.Height = 1;
	HeatMapLookUpDesc.MipLevels = 1;
	HeatMapLookUpDesc.ArraySize = 1;
	HeatMapLookUpDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	HeatMapLookUpDesc.SampleDesc.Count = 1;
	HeatMapLookUpDesc.Usage = D3D11_USAGE_IMMUTABLE;
	HeatMapLookUpDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	TUniquePtr<FTexture2D> HeatMapLookUpResource = RenderCommand::CreateTexture2D(HeatMapLookUpDesc, HeatMapLookUpPixels);
	if (!HeatMapLookUpResource)
	{
		return;
	}

	UTexture2D* HeatMapLookUpTexture = NewObject<UTexture2D>();
	HeatMapLookUpTexture->SetResource(std::move(HeatMapLookUpResource));
	HeatMapLookUpTexture->SetPath("HeatMapLookUpTexture");

	AssetMap["HeatMapLookUpTexture"] = HeatMapLookUpTexture;
}

void UAssetManager::CreateDefaultMeshes()
{
	// 메시 데이터 업로드
	FModelMeshData* CubeData = FGeometryGenerator::GetMeshData("Cube");
	RegisterAsset("Cube", CreateStaticMesh(CubeData));

	FModelMeshData* ConeData = FGeometryGenerator::GetMeshData("Cone");
	RegisterAsset("Cone", CreateStaticMesh(ConeData));

	FModelMeshData* SphereData = FGeometryGenerator::GetMeshData("Sphere");
	RegisterAsset("Sphere", CreateStaticMesh(SphereData));

	FModelMeshData* CylinderData = FGeometryGenerator::GetMeshData("Cylinder");
	RegisterAsset("Cylinder", CreateStaticMesh(CylinderData));

	FModelMeshData* PlaneData = FGeometryGenerator::GetMeshData("Plane");
	RegisterAsset("Plane", CreateStaticMesh(PlaneData));

	FModelMeshData* ArrowMeshData = FGeometryGenerator::GetMeshData("Arrow");
	RegisterAsset("Arrow", CreateStaticMesh(ArrowMeshData));

	FModelMeshData* RingMeshData = FGeometryGenerator::GetMeshData("Ring");
	RegisterAsset("Ring", CreateStaticMesh(RingMeshData));

	FModelMeshData* ScaleBarData = FGeometryGenerator::GetMeshData("ScaleBar");
	RegisterAsset("ScaleBar", CreateStaticMesh(ScaleBarData));

	FModelMeshData* GizmoSphereData = FGeometryGenerator::GetMeshData("GizmoSphere");
	RegisterAsset("GizmoSphere", CreateStaticMesh(GizmoSphereData));

	const FParticleVertex Vertices[] =
	{
		{ FVector(0.0f, -0.5f,  0.5f), FVector2(0.0f, 0.0f) },
		{ FVector(0.0f, 0.5f,  0.5f), FVector2(1.0f, 0.0f) },
		{ FVector(0.0f, 0.5f, -0.5f), FVector2(1.0f, 1.0f) },
		{ FVector(0.0f, -0.5f, -0.5f), FVector2(0.0f, 1.0f) }
	};

	const uint32 Indices[] =
	{
		0, 1, 2,
		0, 2, 3
	};

	UStaticMesh* Mesh = NewObject<UStaticMesh>();
	Mesh->VertexBuffer = RenderCommand::CreateStaticVertexBuffer(
		Vertices,
		sizeof(Vertices), sizeof(FParticleVertex));
	Mesh->IndexBuffer = RenderCommand::CreateStaticIndexBuffer(
		Indices,
		ARRAYSIZE(Indices));

	FVertexPNCT Vertex{};
	Vertex.Position = FVector(0.0f, -0.5f, 0.5f);
	Mesh->MeshData.Vertices.Add(Vertex);
	Vertex.Position = FVector(0.0f, 0.5f, 0.5f);
	Mesh->MeshData.Vertices.Add(Vertex);
	Vertex.Position = FVector(0.0f, 0.5f, -0.5f);
	Mesh->MeshData.Vertices.Add(Vertex);
	Vertex.Position = FVector(0.0f, -0.5f, -0.5f);
	Mesh->MeshData.Vertices.Add(Vertex);

	Mesh->MeshData.Indices = { 0, 1, 2, 0, 2, 3, 0,2,1, 0,3,2 };

	Mesh->MeshData.AABB.Min = FVector(-0.001f, -0.5, -0.5);
	Mesh->MeshData.AABB.Max = FVector(0.001f, 0.5, 0.5);
	Mesh->RebuildRenderData();

	RegisterAsset("Quad", Mesh);
}

void UAssetManager::CreateDefaultMaterial()
{
	UMaterial* DefaultMat = NewObject<UMaterial>();
	DefaultMat->Shader = FRenderResourceManager::GetUberLitProgram(FViewRenderMode{}, false);
	DefaultMat->Textures.Add(GetAssetByPath<UTexture2D>("WhiteTexture"));
	 //DefaultMat->Textures.Add(GetAssetByPath<UTexture2D>("FlatNormalTexture"));

	DefaultMat->ParamLayout = EMaterialParamLayout::StaticMesh;
	DefaultMat->ParamBuffer = RenderCommand::CreateConstantBuffer(sizeof(FStaticMeshMaterialParams));
	RegisterAsset("DefaultMaterial", DefaultMat);
}

void UAssetManager::CreateParticleMaterial()
{
	UMaterial* ParticleMat = NewObject<UMaterial>();
	ParticleMat->Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/ParticleSubUVShader.hlsl");
	ParticleMat->Textures.Add(LoadTexture(ResourcePaths::DefaultParticleTexture));
	ParticleMat->BlendState = EBlendState::AlphaBlend;
	ParticleMat->DepthStencilState = EDepthStencilState::ReadOnly;
	ParticleMat->ParamLayout = EMaterialParamLayout::ParticleSubUV;
	ParticleMat->ParamBuffer = RenderCommand::CreateConstantBuffer(256);
	RegisterAsset("SubUVMaterial", ParticleMat);
}

void UAssetManager::CreateBillboardMaterial()
{
	UMaterial* BillboardMat = NewObject<UMaterial>();
	BillboardMat->Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/BillboardShader.hlsl");
	BillboardMat->Textures.Add(LoadTexture(ResourcePaths::DefaultParticleTexture));
	BillboardMat->BlendState = EBlendState::AlphaBlend;
	BillboardMat->DepthStencilState = EDepthStencilState::ReadOnly;
	BillboardMat->ParamLayout = EMaterialParamLayout::Billboard;
	BillboardMat->ParamBuffer = RenderCommand::CreateConstantBuffer(sizeof(FBillboardMaterialParams));
	RegisterAsset("BillboardMaterial", BillboardMat);
}

void UAssetManager::CreateDecalMaterial()
{
	UMaterial* Material = NewObject<UMaterial>();
	Material->MaterialDomain = EMaterialDomain::Decal;
	Material->ParamLayout = EMaterialParamLayout::None;
	Material->Textures.Add(GetAssetByPath<UTexture2D>("WhiteTexture"));
	Material->Textures.Add(GetAssetByPath<UTexture2D>("FlatNormalTexture"));
	Material->BaseColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	Material->BlendState = EBlendState::AlphaBlend;
	Material->DepthStencilState = EDepthStencilState::Disabled;
	Material->SamplerState = ESamplerState::LinearClamp;
	RegisterAsset("DefaultDecalMaterial", Material);
}

void UAssetManager::Shutdown()
{
	Get().AssetMap.Empty();
}


void UAssetManager::RegisterAsset(const FString& Key, UAsset* Asset)
{
	Asset->SetPath(Key);
	AssetMap[Key] = Asset;
}

UTexture2D* UAssetManager::LoadTexture(const FString& InPath, bool bGenerateMips, bool bSRGB)
{
	if (UAsset** Found = AssetMap.Find(InPath))
	{
		return Cast<UTexture2D>(*Found);
	}

	FImageData Data = ImageLoader::LoadAuto(InPath);
	if (!Data.IsValid()) return nullptr;

	D3D11_TEXTURE2D_DESC Desc{};
	Desc.Width = Data.Width;
	Desc.Height = Data.Height;
	Desc.ArraySize = 1;
	Desc.Format = Data.Format;              // 로더가 정한 포맷 (.hdr이면 float)
	// 8비트 이미지 파일은 sRGB로 저장돼 있다. .hdr(float)은 이미 Linear라 그대로 둔다.
	if (bSRGB && Desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM)
		Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	Desc.SampleDesc.Count = 1;
	Desc.MipLevels = bGenerateMips ? 0 : 1;
	Desc.Usage = bGenerateMips ? D3D11_USAGE_DEFAULT : D3D11_USAGE_IMMUTABLE;
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | (bGenerateMips ? D3D11_BIND_RENDER_TARGET : 0);
	Desc.MiscFlags = bGenerateMips ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;

	TUniquePtr<FTexture2D> Resource = RenderCommand::CreateTexture2D(Desc, Data);

	if (!Resource) return nullptr;

	UTexture2D* Asset = NewObject<UTexture2D>();
	Asset->SetResource(std::move(Resource));
	RegisterAsset(InPath, Asset);

	return Asset;
}

UFont* UAssetManager::LoadFontAtlas(const FString& JsonPath, const FString& AtlasTexturePath)
{
	if (UAsset** Found = AssetMap.Find(JsonPath))
	{
		return Cast<UFont>(*Found);
	}

	UFont* Font = NewObject<UFont>();

	if (!Font->LoadFontAtlasJson(JsonPath))
	{
		return nullptr;
	}

	// 폰트 아틀라스는 글자 커버리지 데이터라 감마 디코딩하지 않는다
	Font->AtlasTexture = LoadTexture(AtlasTexturePath, false, false);
	if (!Font->AtlasTexture)
	{
		return nullptr;
	}

	RegisterAsset(JsonPath, Font);
	return Font;
}

UStaticMesh* UAssetManager::LoadObjStaticMesh(const FString& Path, bool bAllowSourceImport, bool bForceSourceImport)
{
	const FString Key = MakeAssetKey(Path);
	if (UStaticMesh* Cached = GetAssetByPath<UStaticMesh>(Key))
	{
		if (!bForceSourceImport)
		{
			return Cached;
		}
	}

	TUniquePtr<FModelMeshData> Data = FObjImporter::LoadStaticMeshData(Path, bAllowSourceImport, bForceSourceImport);
	UStaticMesh* Mesh = CreateStaticMesh(Data.get());   // Data가 nullptr이면 nullptr 반환
	if (!Mesh)
	{
		return nullptr;
	}

	Get().RegisterAsset(Key, Mesh);
	return Mesh;
}

void UAssetManager::LoadFbxModel(const FString& Path, bool bAllowSourceImport, bool bSaveAnimationAssets, bool bForceSourceImport)
{
	const FString MeshKey = MakeAssetKey(Path);
	USkeletalMesh* CachedSkeletalMesh = GetAssetByPath<USkeletalMesh>(MeshKey);
	UStaticMesh* CachedStaticMesh = GetAssetByPath<UStaticMesh>(MeshKey);

	if ((CachedSkeletalMesh || CachedStaticMesh) && !bForceSourceImport)
	{
		return;
	}

	FModelData ModelData;
	if (!FFbxImporter::LoadFromFile(Path, ModelData, bAllowSourceImport, bForceSourceImport))
	{
		return;
	}

	if (ModelData.IsSkeletal)
	{
		const bool bHasMeshGeometry = !ModelData.Meshs.IsEmpty() && ModelData.MeshData.Vertices.Num() > 0;
		USkeleton* Skeleton = !bForceSourceImport && CachedSkeletalMesh ? CachedSkeletalMesh->GetSkeleton() : nullptr;

		// 메쉬가 없는 순수 애니메이션 FBX인 경우, 호환 스켈레톤 탐색하여 공유!
		if (!Skeleton && !bHasMeshGeometry)
		{
			const FString PathLower = Path;
			// 마네킹 경로 하위일 때만 마네킹 스켈레톤을 우선 공유
			if (PathLower.find("Mannequin") != FString::npos || PathLower.find("UnrealCharac") != FString::npos)
			{
				Skeleton = GetAssetByPath<USkeleton>("Models/Mannequin/UnrealCharac/SKM_Manny_Simple_Skeleton");

				if (!Skeleton)
				{
					std::filesystem::path ParentDir = PathUtils::FromUtf8(Path).parent_path();
					while (!ParentDir.empty() && ParentDir != ParentDir.parent_path())
					{
						std::filesystem::path CandidateMeshPath = ParentDir / "SKM_Manny_Simple.FBX";
						if (fs::exists(CandidateMeshPath))
						{
							LoadFbxModel(PathUtils::ToUtf8(CandidateMeshPath), true, true, false);
							Skeleton = GetAssetByPath<USkeleton>("Models/Mannequin/UnrealCharac/SKM_Manny_Simple_Skeleton");
							if (Skeleton)
							{
								break;
							}
						}
						ParentDir = ParentDir.parent_path();
					}
				}
			}
		}

		if (!Skeleton)
		{
			Skeleton = CreateSkeleton(ModelData);
			if (!Skeleton)
			{
				return;
			}

			Skeleton->LoadDefaultPose(Path);

			std::filesystem::path SkeletonPath = PathUtils::FromUtf8(Path);
			SkeletonPath.replace_filename(std::format("{}_Skeleton", SkeletonPath.stem().generic_string()));

			RegisterAsset(MakeAssetKey(PathUtils::ToUtf8(SkeletonPath)), Skeleton);
		}

		USkeletalMesh* SkeletalMesh = !bForceSourceImport ? CachedSkeletalMesh : nullptr;

		if (bHasMeshGeometry)
		{
			if (!SkeletalMesh)
			{
				SkeletalMesh = CreateSkeletalMesh(ModelData);
				if (!SkeletalMesh)
				{
					return;
				}

				SkeletalMesh->SetSkeleton(Skeleton);
				RegisterAsset(MeshKey, SkeletalMesh);
			}
			else if (SkeletalMesh->GetSkeleton() != Skeleton)
			{
				SkeletalMesh->SetSkeleton(Skeleton);
			}
		}

		if (bSaveAnimationAssets)
		{
			for (FModelAnimation& ModelAnimation : ModelData.Animations)
			{
				std::filesystem::path AnimationPath = PathUtils::FromUtf8(Path);
				AnimationPath.replace_extension();

				FString AnimationFileName = ModelAnimation.Name.ToString();
				const FString InvalidCharacters = "\\/:*?\"<>|";

				for (char& Character : AnimationFileName)
				{
					if (InvalidCharacters.find(Character) != FString::npos)
					{
						Character = '_';
					}
				}

				AnimationPath += "_" + AnimationFileName + ".anim";

				const FString AnimationPathString = PathUtils::ToUtf8(AnimationPath);
				const bool bAnimationFileExists = fs::exists(AnimationPath);

				TArray<FModelAnimationNotify> AnimationNotifies;
				const FString AnimationKey = MeshKey + "::" + ModelAnimation.Name.ToString();

				if (bAnimationFileExists)
				{
					LoadAnimSequence(AnimationPathString);

					if (UAnimSequence* ExistingSequence = GetAssetByPath<UAnimSequence>(AnimationKey))
					{
						for (const FAnimNotifyEvent& Notify : ExistingSequence->GetNotifies())
						{
							FModelAnimationNotify ModelNotify;
							ModelNotify.TriggerTime = Notify.TriggerTime;
							ModelNotify.Duration = Notify.Duration;
							ModelNotify.NotifyName = Notify.NotifyName;
							AnimationNotifies.Add(std::move(ModelNotify));
						}
					}
				}

				if (bForceSourceImport || !bAnimationFileExists)
				{
					if (!FModelBake::WriteAnimationAsset(AnimationPathString, MeshKey, ModelAnimation, AnimationNotifies))
					{
						HTR_LOG(Error, "[ANIMATION] Failed to save animation asset: {}", AnimationPathString);
						continue;
					}
				}
				else if (!LoadAnimSequence(AnimationPathString))
				{
					if (!FModelBake::WriteAnimationAsset(AnimationPathString, MeshKey, ModelAnimation, AnimationNotifies))
					{
						HTR_LOG(Error, "[ANIMATION] Failed to rebuild animation asset: {}", AnimationPathString);
						continue;
					}
				}

				LoadAnimSequence(AnimationPathString);
			}
		}
		else
		{
			for (const FModelAnimation& ModelAnimation : ModelData.Animations)
			{
				const FString AnimationKey = MeshKey + "::" + ModelAnimation.Name.ToString();
				if (GetAssetByPath<UAnimSequence>(AnimationKey))
				{
					continue;
				}
				TArray<FModelAnimationNotify> EmptyNotifies;
				if (UAnimSequence* EmbeddedSequence = CreateAnimationSequence(ModelAnimation, EmptyNotifies, Skeleton))
				{
					RegisterAsset(AnimationKey, EmbeddedSequence);
				}
			}
		}
	}
	else if (bForceSourceImport || !CachedStaticMesh)
	{
		UStaticMesh* StaticMesh = CreateStaticMesh(&ModelData.MeshData);
		if (StaticMesh)
		{
			RegisterAsset(MeshKey, StaticMesh);
		}
	}
}

ULevelSequence* UAssetManager::LoadLevelSequence(const FString& Path)
{
	const FString Key = MakeAssetKey(Path);
	if (ULevelSequence* Cached = GetAssetByPath<ULevelSequence>(Key))
	{
		return Cached;
	}

	ULevelSequence* LevelSequence = NewObject<ULevelSequence>();
	if (!FJsonArchive::LoadSequence(LevelSequence, Path))
	{
		HTR_LOG(Warning, "Can't load level sequence");
		delete LevelSequence;
		return nullptr;
	}

	Get().RegisterAsset(Key, LevelSequence);

	return LevelSequence;
}

UBlendSpace1D* UAssetManager::LoadBlendSpace(const FString& Path)
{
	const FString Key = MakeAssetKey(Path);
	if (UBlendSpace1D* Cached = GetAssetByPath<UBlendSpace1D>(Key))
	{
		return Cached;
	}
	UBlendSpace1D* BlendSpace = NewObject<UBlendSpace1D>();
	if (!FJsonArchive::LoadBlendSpace(BlendSpace, Path))
	{
		HTR_LOG(Warning, "Can't load BlendSpace: {}", Path);
		delete BlendSpace;
		return nullptr;
	}

	Get().RegisterAsset(Key, BlendSpace);

	return BlendSpace;
}

UAnimationGraph* UAssetManager::LoadAnimationGraph(const FString& Path)
{
	const FString Key = MakeAssetKey(Path);
	if (UAnimationGraph* Cached = GetAssetByPath<UAnimationGraph>(Key))
	{
		return Cached;
	}
	UAnimationGraph* Graph = NewObject<UAnimationGraph>();
	if (!FJsonArchive::LoadAnimationGraph(Graph, Path))
	{
		HTR_LOG(Warning, "Can't load AnimationGraph: {}", Path);
		delete Graph;
		return nullptr;
	}
	Get().RegisterAsset(Key, Graph);
	return Graph;
}

TArray<UAnimSequence*> UAssetManager::GetAnimSequencesForMesh(const FString& MeshKey) const
{
	TArray<UAnimSequence*> Result;
	const FString Prefix = MeshKey + "::";
	for (const auto& Pair : AssetMap)
	{
		if (!Pair.second || !Pair.second->IsA<UAnimSequence>())
		{
			continue;
		}
		if (Pair.first.size() >= Prefix.size() &&
			_strnicmp(Pair.first.c_str(), Prefix.c_str(), Prefix.size()) == 0)
		{
			Result.Add(Cast<UAnimSequence>(Pair.second));
		}
	}
	return Result;
}

UAnimSequence* UAssetManager::LoadAnimSequence(const FString& Path, USkeletalMesh** OutSkeletalMesh)
{
	if (OutSkeletalMesh)
	{
		*OutSkeletalMesh = nullptr;
	}

	FString SourceMeshKey;
	FModelAnimation ModelAnimation;
	TArray<FModelAnimationNotify> ModelNotifies;
	bool bHasEmbeddedNotifies = false;

	if (!FModelBake::ReadAnimationAsset(Path, SourceMeshKey, ModelAnimation, ModelNotifies, bHasEmbeddedNotifies))
	{
		HTR_LOG(Error, "[ANIMATION] Failed to load animation asset: {}", Path);
		return nullptr;
	}

	const std::filesystem::path SourceMeshPath = std::filesystem::path("Assets") / PathUtils::FromUtf8(SourceMeshKey);
	Get().LoadFbxModel(PathUtils::ToUtf8(SourceMeshPath), false);

	USkeletalMesh* SkeletalMesh = GetAssetByPath<USkeletalMesh>(SourceMeshKey);
	USkeleton* TargetSkeleton = SkeletalMesh ? SkeletalMesh->GetSkeleton() : nullptr;

	if (!TargetSkeleton)
	{
		std::filesystem::path SkeletonPath = SourceMeshPath;
		SkeletonPath.replace_filename(std::format("{}_Skeleton", SkeletonPath.stem().generic_string()));
		TargetSkeleton = GetAssetByPath<USkeleton>(MakeAssetKey(PathUtils::ToUtf8(SkeletonPath)));
	}

	if (!TargetSkeleton)
	{
		if (SourceMeshKey.find("Mannequin") != FString::npos || SourceMeshKey.find("UnrealCharac") != FString::npos)
		{
			TargetSkeleton = GetAssetByPath<USkeleton>("Models/Mannequin/UnrealCharac/SKM_Manny_Simple_Skeleton");
		}
	}

	if (!TargetSkeleton)
	{
		HTR_LOG(Error, "[ANIMATION] Target Skeleton is unavailable for: {}", SourceMeshKey);
		return nullptr;
	}

	if (OutSkeletalMesh)
	{
		*OutSkeletalMesh = SkeletalMesh ? SkeletalMesh : TargetSkeleton->GetPreviewMesh();
	}

	const FString AnimationKey = SourceMeshKey + "::" + ModelAnimation.Name.ToString();

	if (UAnimSequence* Cached = GetAssetByPath<UAnimSequence>(AnimationKey))
	{
		if (Cached->GetSkeleton() == TargetSkeleton)
		{
			Cached->GetMutableNotifies().Reset();

			for (const FModelAnimationNotify& Notify : ModelNotifies)
			{
				Cached->AddNotify(Notify.TriggerTime, Notify.Duration, Notify.NotifyName);
			}

			return Cached;
		}
	}

	UAnimSequence* Animation = CreateAnimationSequence(ModelAnimation, ModelNotifies, TargetSkeleton);
	if (!Animation)
	{
		return nullptr;
	}

	Get().RegisterAsset(AnimationKey, Animation);

	if (!bHasEmbeddedNotifies)
	{
		const FString LegacyNotifyPath = FJsonArchive::GetAnimationNotifyPath(AnimationKey);
		const std::filesystem::path LegacyNotifyFilePath = PathUtils::FromUtf8(LegacyNotifyPath);
		const bool bHasLegacyFile = fs::exists(LegacyNotifyFilePath);
		bool bLegacyLoaded = !bHasLegacyFile;

		if (bHasLegacyFile)
		{
			bLegacyLoaded = FJsonArchive::LoadAnimNotifies(Animation, LegacyNotifyPath);
		}

		if (bLegacyLoaded)
		{
			TArray<FModelAnimationNotify> MigratedNotifies;

			for (const FAnimNotifyEvent& Notify : Animation->GetNotifies())
			{
				FModelAnimationNotify ModelNotify;
				ModelNotify.TriggerTime = Notify.TriggerTime;
				ModelNotify.Duration = Notify.Duration;
				ModelNotify.NotifyName = Notify.NotifyName;
				MigratedNotifies.Add(std::move(ModelNotify));
			}

			if (FModelBake::UpdateAnimationNotifies(Path, MigratedNotifies))
			{
				if (bHasLegacyFile)
				{
					std::error_code Error;
					fs::remove(LegacyNotifyFilePath, Error);

					if (Error)
					{
						HTR_LOG(Warning, "[ANIMATION] Failed to remove legacy notify file: {}", LegacyNotifyPath);
					}
				}
			}
			else
			{
				HTR_LOG(Error, "[ANIMATION] Failed to migrate animation notifies: {}", Path);
			}
		}
		else
		{
			HTR_LOG(Error, "[ANIMATION] Failed to load legacy animation notifies: {}", LegacyNotifyPath);
		}
	}

	return Animation;
}

bool UAssetManager::ReorientStaticMesh(UStaticMesh& Mesh, const std::function<FVector(const FVector&)>& Rotate)
{
	if (Mesh.GetLODCount() > 1 || Mesh.MeshData.Vertices.IsEmpty())
		return false;   // LOD가 이미 옛 방향으로 만들어졌으면 서로 어긋난다

	FModelMeshData& Data = Mesh.MeshData;
	FBox Bounds{ FVector(FLT_MAX, FLT_MAX, FLT_MAX), FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX) };
	for (FVertexPNCT& Vertex : Data.Vertices)
	{
		Vertex.Position = Rotate(Vertex.Position);
		Vertex.Normal = Rotate(Vertex.Normal);
		Bounds.Min = FVector(std::min(Bounds.Min.X, Vertex.Position.X), std::min(Bounds.Min.Y, Vertex.Position.Y), std::min(Bounds.Min.Z, Vertex.Position.Z));
		Bounds.Max = FVector(std::max(Bounds.Max.X, Vertex.Position.X), std::max(Bounds.Max.Y, Vertex.Position.Y), std::max(Bounds.Max.Z, Vertex.Position.Z));
	}
	Data.AABB = Bounds;

	// 인덱스는 그대로다 (회전은 삼각형 감기 방향을 바꾸지 않는다). 정점 버퍼만 다시 올린다.
	TUniquePtr<FVertexBuffer> VB = RenderCommand::CreateStaticVertexBuffer(Data.Vertices.GetData(),
		sizeof(FVertexPNCT) * static_cast<uint32>(Data.Vertices.Num()), sizeof(FVertexPNCT));
	if (!VB)
		return false;
	Mesh.VertexBuffer = std::move(VB);

	Data.BuildTriangleBVH();   // 피킹용 삼각형 BVH도 새 좌표로
	return true;
}

void UAssetManager::ForEachObjStaticMesh(const std::function<void(const FString& Key, UStaticMesh& Mesh)>& Func)
{
	for (auto& [Key, Asset] : Get().AssetMap)
	{
		if (!Asset || !Asset->IsA<UStaticMesh>() || !fs::path(Key).extension().string().ends_with(".obj"))
			continue;
		Func(Key, *Cast<UStaticMesh>(Asset));
	}
}

FLODGenerateResult UAssetManager::GenerateStaticMeshLODs(UStaticMesh& Mesh, const FLODGenerateRequest& Request)
{
	TArray<FModelMeshData> Generated;
	FLODGenerateResult Result = FStaticMeshLODGenerator::Generate(Mesh.GetMeshData(), Generated);

	if (!Result.bSuccess) return Result;

	if (!(Request.ScreenThresholds[0] >
		Request.ScreenThresholds[1] &&
		Request.ScreenThresholds[1] >
		Request.ScreenThresholds[2] &&
		Request.ScreenThresholds[2] > 0.0f))
	{
		Result.bSuccess = false;
		Result.FailureReason = "Screen thresholds must descend";
		return Result;
	}

	TArray<FStaticMeshLODResource> NewResources;

	for (FModelMeshData& Data : Generated)
	{
		FStaticMeshLODResource Resource;
		Resource.Data = std::move(Data);
		Resource.Data.OptimizeTriangleOrderForVertexCache();

		Resource.VertexBuffer = RenderCommand::CreateStaticVertexBuffer(
			Resource.Data.Vertices.GetData(),
			sizeof(FVertexPNCT) *
			static_cast<uint32>(Resource.Data.Vertices.Num()),
			sizeof(FVertexPNCT));

		Resource.IndexBuffer = RenderCommand::CreateStaticIndexBuffer(
			Resource.Data.Indices.GetData(),
			static_cast<uint32>(Resource.Data.Indices.Num()));

		if (!Resource.VertexBuffer || !Resource.IndexBuffer)
		{
			Result.bSuccess = false;
			Result.FailureReason = "LOD GPU buffer creation failed";
			return Result;
		}

		NewResources.Add(std::move(Resource));
	}

	// bSaveToAsset이면 이 지점에서 파일 저장.
	// 저장 실패 시 기존 Mesh.AdditionalLODs는 그대로 둔다.

	Mesh.AdditionalLODs = std::move(NewResources);
	Mesh.ScreenThresholds = Request.ScreenThresholds;
	Mesh.RebuildRenderData();
	return Result;
}
