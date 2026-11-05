#include "EnginePCH.h"
#include "Asset/FbxImporter/FbxImporter.h"

#include "Asset/ModelBake.h"
#include "Core/PathUtils.h"
#include <algorithm>
#include <stdexcept>

namespace
{
	FMatrix ConvertMatrix(const FbxAMatrix& Matrix)
	{
		FMatrix EngineMatrix;

		for (int32 i = 0; i < 4; ++i)
		{
			const FbxVector4 Row = Matrix.GetRow(i);
			if (i == 1)
			{
				EngineMatrix.M[i][0] = -Row[0];
				EngineMatrix.M[i][1] = Row[1];
				EngineMatrix.M[i][2] = -Row[2];
				EngineMatrix.M[i][3] = -Row[3];
			}
			else
			{
				EngineMatrix.M[i][0] = Row[0];
				EngineMatrix.M[i][1] = -Row[1];
				EngineMatrix.M[i][2] = Row[2];
				EngineMatrix.M[i][3] = Row[3];
			}
		}

		return EngineMatrix;
	}
}

bool FFbxImporter::LoadFromFile(const FString& Path, FModelData& OutModelData)
{
	const FString BinPath = Path + ".bin";

	// .bin 파일 읽기
	if (FModelBake::ReadBaked(BinPath, OutModelData))
	{
		return true;
	}

	std::filesystem::path FbxPath = PathUtils::FromUtf8(Path);

	// FBX sdk 준비
	fbxsdk::FbxManager* MG = fbxsdk::FbxManager::Create();
	if (!MG)
	{
		return false;
	}

	fbxsdk::FbxIOSettings* IOSettings = fbxsdk::FbxIOSettings::Create(MG, IOSROOT);
	MG->SetIOSettings(IOSettings);

	fbxsdk::FbxGeometryConverter* Converter = new fbxsdk::FbxGeometryConverter(MG);

	fbxsdk::FbxImporter* Importer = fbxsdk::FbxImporter::Create(MG, "");
	bool bResult = Importer->Initialize(Path.c_str());
	if (!bResult)
	{
		HTR_LOG(Warning, "[Fbx] : {}", Importer->GetStatus().GetErrorString());
		MG->Destroy();
		return false;
	}

	fbxsdk::FbxScene* Scene = fbxsdk::FbxScene::Create(MG, "");

	bResult = Importer->Import(Scene);
	if (!bResult)
	{
		HTR_LOG(Warning, "[Fbx] : {}", Importer->GetStatus().GetErrorString());
		MG->Destroy();
		return false;
	}

	bResult = Converter->Triangulate(Scene, true);
	if (!bResult)
	{
		HTR_LOG(Warning, "[Fbx] : Failed triangulate");
		MG->Destroy();
		return false;
	}
	if (Scene->GetGlobalSettings().GetSystemUnit() != FbxSystemUnit::m)
		FbxSystemUnit::m.ConvertScene(Scene);

	// 일단 오른손 좌표계로 계산하고 후에 반전 시킨다.
	// eParityEven = X축, eParityOdd = Y축
	fbxsdk::FbxAxisSystem::ECoordSystem CoordSystem = fbxsdk::FbxAxisSystem::eRightHanded;
	fbxsdk::FbxAxisSystem::EUpVector UpVector = fbxsdk::FbxAxisSystem::eZAxis;
	fbxsdk::FbxAxisSystem::EFrontVector FrontVector = (fbxsdk::FbxAxisSystem::EFrontVector)-fbxsdk::FbxAxisSystem::eParityOdd;

	fbxsdk::FbxGlobalSettings& GlobalSettings = Scene->GetGlobalSettings();

	fbxsdk::FbxAxisSystem EngineImportAxis(UpVector, FrontVector, CoordSystem);
	fbxsdk::FbxAxisSystem SourceAxis = GlobalSettings.GetAxisSystem();

	if (SourceAxis != EngineImportAxis)
	{
		fbxsdk::FbxRootNodeUtility::RemoveAllFbxRoots(Scene);
		EngineImportAxis.ConvertScene(Scene);
	}



	// 파싱
	TMap<uint64, int32> NodeToIndexMap;
	TMap<int32, int32> NodeIndexToBoneIndexMap;
	TArray<FMeshNode> MeshNodes;
	BuildNodeHierarchy(-1, Scene->GetRootNode(), NodeToIndexMap, NodeIndexToBoneIndexMap, OutModelData.Nodes, OutModelData.Bones, MeshNodes);

	for (FMeshNode& MeshNode : MeshNodes)
	{
		ProcessMeshNode(FbxPath, NodeToIndexMap, NodeIndexToBoneIndexMap, OutModelData.Bones, MeshNode, OutModelData.MeshData, OutModelData.VertexInfluencesList, OutModelData.Meshs);
	}

	OutModelData.IsSkeletal = OutModelData.Bones.Num();
	if (!OutModelData.IsSkeletal)
	{
		// 스켈레탈이 아니면 GlobalTransform을 계산해서 Vertex를 변환해 버린다.
		TArray<FMatrix> GlobalTransforms;
		GlobalTransforms.SetNum(OutModelData.Nodes.Num());
		for (int32 i = 0; i < OutModelData.Nodes.Num(); i++)
		{
			const FModelNode& ModelNode = OutModelData.Nodes[i];

			FMatrix ParentTranform = FMatrix::Identity;
			if (ModelNode.ParentNodeIndex != -1)
			{
				ParentTranform = GlobalTransforms[ModelNode.ParentNodeIndex];
			}

			GlobalTransforms[i] = ModelNode.LocalTransform * ParentTranform;
		}

		for (const FModelMesh& ModelMesh : OutModelData.Meshs)
		{
			const FMatrix& GlobalTransform = GlobalTransforms[ModelMesh.NodeIndex];
			const FMatrix GlobalTransformForNormal = GlobalTransform.Inverse().GetTransposed();

			for (int32 i = 0; i < ModelMesh.SectionCount; i++)
			{
				FMeshSection& Section = OutModelData.MeshData.Sections[ModelMesh.FirstSection + i];

				for (int32 j = 0; j < Section.IndexCount; j++)
				{
					int32 VertexIndex = OutModelData.MeshData.Indices[Section.StartIndex + j];

					FVertexPNCT& Vertex = OutModelData.MeshData.Vertices[VertexIndex];

					FVector4 Transformed = FVector4(Vertex.Position, 1.f) * GlobalTransform;
					FVector4 TransformedNormal = FVector4(Vertex.Normal, 0.f) * GlobalTransformForNormal;

					Vertex.Position = FVector(Transformed.X, Transformed.Y, Transformed.Z);
					Vertex.Normal = FVector(TransformedNormal.X, TransformedNormal.Y, TransformedNormal.Z);
				}
			}
		}
	}

	OutModelData.MeshData.BuildAABB();
	OutModelData.MeshData.BuildTriangleBVH();

	FModelBake::WriteBaked(BinPath, OutModelData);

	// 리소스 정리
	delete Converter;
	MG->Destroy();

	return true;
}

void FFbxImporter::BuildNodeHierarchy(int32 ParentNodeIndex, fbxsdk::FbxNode* Current, TMap<uint64, int32>& NodeToIndexMap, TMap<int32, int32>& NodeIndexToBoneIndexMap, TArray<FModelNode>& OutNodes, TArray<FModelBone>& OutBones, TArray<FMeshNode>& OutMeshNodes)
{
	if (!Current)
	{
		return;
	}

	int32 NodeIndex = OutNodes.Num();

	NodeToIndexMap[Current->GetUniqueID()] = NodeIndex;

	FModelNode& ModelNode = OutNodes.AddDefaulted_GetRef();
	ModelNode.Name = Current->GetName();
	ModelNode.ParentNodeIndex = ParentNodeIndex;

	fbxsdk::FbxAMatrix& LocalTransform = Current->EvaluateLocalTransform();
	ModelNode.LocalTransform = ConvertMatrix(LocalTransform);

	if (Current->GetMesh())
	{
		FMeshNode& MeshNode = OutMeshNodes.AddDefaulted_GetRef();
		MeshNode.FbxNode = Current;
		MeshNode.NodeIndex = NodeIndex;
	}

	if (Current->GetSkeleton())
	{
		FModelBone& ModelBone = OutBones.AddDefaulted_GetRef();
		ModelBone.NodeIndex = NodeIndex;

		int32 CurrentParentNodeIndex = ParentNodeIndex;
		while (CurrentParentNodeIndex != -1 && !NodeIndexToBoneIndexMap.Contains(CurrentParentNodeIndex))
		{
			CurrentParentNodeIndex = OutNodes[CurrentParentNodeIndex].ParentNodeIndex;
		}

		ModelBone.ParentBoneIndex = -1;
		if (NodeIndexToBoneIndexMap.Contains(CurrentParentNodeIndex))
		{
			ModelBone.ParentBoneIndex = NodeIndexToBoneIndexMap[CurrentParentNodeIndex];
		}

		NodeIndexToBoneIndexMap[NodeIndex] = OutBones.Num() - 1;
	}

	for (int32 i = 0; i < Current->GetChildCount(); i++)
	{
		BuildNodeHierarchy(NodeIndex, Current->GetChild(i), NodeToIndexMap, NodeIndexToBoneIndexMap, OutNodes, OutBones, OutMeshNodes);
	}
}

void FFbxImporter::ProcessMeshNode(const std::filesystem::path& FbxPath, const TMap<uint64, int32>& NodeToIndexMap, const TMap<int32, int32>& NodeIndexToBoneIndexMap, TArray<FModelBone>& Bones, const FMeshNode& MeshNode, FModelMeshData& OutModelMeshData, TArray<FModelVertexInfluences>& OutVertexInfluencesList, TArray<FModelMesh>& OutMeshs)
{
	bool IsSkeletal = Bones.Num();

	fbxsdk::FbxMesh* Mesh = MeshNode.FbxNode->GetMesh();

	TArray<FVertexPNCT> Vertices;
	TArray<int32> VertexToControlPoint;
	TArray<FMeshMaterialSlot> Materials;
	TArray<FTriangleFace> Faces;
	ParseMesh(FbxPath, Mesh, Vertices, VertexToControlPoint, Materials, Faces);

	std::sort(Faces.begin(), Faces.end(), [](const FTriangleFace& A, const FTriangleFace& B) { return A.MaterialSlotIndex < B.MaterialSlotIndex; });

	uint32 VertexOffset = OutModelMeshData.Vertices.Num();
	uint32 IndexOffset = OutModelMeshData.Indices.Num();
	uint32 MaterialOffset = OutModelMeshData.MaterialSlots.Num();

	FModelMesh& ModelMesh = OutMeshs.AddDefaulted_GetRef();
	ModelMesh.NodeIndex = MeshNode.NodeIndex;
	;		ModelMesh.FirstSection = OutModelMeshData.Sections.Num();
	ModelMesh.SectionCount = 0;

	FMatrix BindGlobalTransform;
	ParseSkin(VertexToControlPoint, NodeToIndexMap, NodeIndexToBoneIndexMap, MeshNode, Bones, OutVertexInfluencesList, BindGlobalTransform);

	FMatrix BindGlobalTransformForNormal = BindGlobalTransform.Inverse().GetTransposed();

	TArray<uint32> Indices;
	FMeshSection* CurrentSection = nullptr;
	for (int32 i = 0; i < Faces.Num(); i++)
	{
		const FTriangleFace& Face = Faces[i];
		if (CurrentSection == nullptr || CurrentSection->MaterialSlotIndex != MaterialOffset + Face.MaterialSlotIndex)
		{
			CurrentSection = &OutModelMeshData.Sections.AddDefaulted_GetRef();
			CurrentSection->StartIndex = IndexOffset + (i * 3);
			CurrentSection->IndexCount = 0;
			CurrentSection->MaterialSlotIndex = MaterialOffset + Face.MaterialSlotIndex;

			ModelMesh.SectionCount++;
		}

		CurrentSection->IndexCount += 3;

		Indices.Add(Face.VertexIndices[0]);
		Indices.Add(Face.VertexIndices[1]);
		Indices.Add(Face.VertexIndices[2]);
	}

	const bool bNeedsNormals = Mesh->GetElementNormalCount() == 0;
	const bool bNeedsTangents = bNeedsNormals || Mesh->GetElementTangentCount() == 0;
	if (bNeedsNormals)
	{
		FMath::CalculateNormals(Vertices.GetData(), Vertices.Num(), Indices.GetData(), Indices.Num());
	}

	if (bNeedsTangents)
	{
		FMath::CalculateTangents(Vertices.GetData(), Vertices.Num(), Indices.GetData(), Indices.Num());
	}

	for (int32 i = 0; i < Vertices.Num(); i++)
	{
		FVertexPNCT Vertex = Vertices[i];

		if (IsSkeletal)
		{
			FVector4 Position = FVector4(Vertex.Position, 1.f) * BindGlobalTransform;
			Vertex.Position = FVector(Position.X, Position.Y, Position.Z);

			FVector4 Normal = FVector4(Vertex.Normal, 0.f) * BindGlobalTransformForNormal;
			Vertex.Normal = FVector(Normal.X, Normal.Y, Normal.Z).Normalized();

			FVector4 Tangent = FVector4(Vertex.Tangent.XYZ, 0.f) * BindGlobalTransformForNormal;
			const FVector TangentDirection = FVector(Tangent.X, Tangent.Y, Tangent.Z).Normalized();
			Vertex.Tangent = FVector4(TangentDirection, Vertex.Tangent.W);
		}

		OutModelMeshData.Vertices.Add(Vertex);
	}

	for (uint32 Index : Indices)
	{
		OutModelMeshData.Indices.Add(VertexOffset + Index);
	}

	for (int32 i = 0; i < Materials.Num(); i++)
	{
		OutModelMeshData.MaterialSlots.Add(Materials[i]);
	}
}

void FFbxImporter::ParseMaterial(const std::filesystem::path& ParentPath, fbxsdk::FbxSurfaceMaterial* Material, FMeshMaterialSlot& OutMaterial)
{
	fbxsdk::FbxProperty DiffuseProperty = Material->FindProperty(fbxsdk::FbxSurfaceMaterial::sDiffuse);
	if (DiffuseProperty.IsValid())
	{
		fbxsdk::FbxDouble3 FbxColor3 = DiffuseProperty.Get<FbxDouble3>();
		OutMaterial.BaseColor = FLinearColor(FbxColor3[0], FbxColor3[1], FbxColor3[2]);

		for (int32 i = 0; i < DiffuseProperty.GetSrcObjectCount<FbxFileTexture>(); i++)
		{
			fbxsdk::FbxFileTexture* Texture = DiffuseProperty.GetSrcObject<fbxsdk::FbxFileTexture>(i);
			if (Texture->GetRelativeFileName())
			{
				OutMaterial.DiffuseTexturePath = PathUtils::ToUtf8(ParentPath / PathUtils::FromUtf8(Texture->GetRelativeFileName()));
			}
			else if (Texture->GetFileName())
			{
				OutMaterial.DiffuseTexturePath = PathUtils::ToUtf8(ParentPath / PathUtils::FromUtf8(Texture->GetFileName()));
			}
		}
	}

	fbxsdk::FbxProperty SpecularProperty = Material->FindProperty(fbxsdk::FbxSurfaceMaterial::sSpecular);
	if (SpecularProperty.IsValid())
	{
		fbxsdk::FbxDouble3 FbxColor3 = SpecularProperty.Get<FbxDouble3>();
		OutMaterial.SpecularColor = FLinearColor(FbxColor3[0], FbxColor3[1], FbxColor3[2]);
	}

	fbxsdk::FbxProperty ShininessProperty = Material->FindProperty(fbxsdk::FbxSurfaceMaterial::sShininess);
	if (ShininessProperty.IsValid())
	{
		fbxsdk::FbxDouble FbxDouble1 = ShininessProperty.Get<FbxDouble>();
		OutMaterial.Shininess = FbxDouble1;
	}

	fbxsdk::FbxProperty NormalProperty = Material->FindProperty(fbxsdk::FbxSurfaceMaterial::sNormalMap);
	if (DiffuseProperty.IsValid())
	{
		for (int32 i = 0; i < NormalProperty.GetSrcObjectCount<FbxFileTexture>(); i++)
		{
			fbxsdk::FbxFileTexture* Texture = NormalProperty.GetSrcObject<fbxsdk::FbxFileTexture>(i);
			if (Texture->GetRelativeFileName())
			{
				OutMaterial.NormalTexturePath = PathUtils::ToUtf8(ParentPath / PathUtils::FromUtf8(Texture->GetRelativeFileName()));
			}
			else if (Texture->GetFileName())
			{
				OutMaterial.NormalTexturePath = PathUtils::ToUtf8(ParentPath / PathUtils::FromUtf8(Texture->GetFileName()));
			}
		}
	}
}

void FFbxImporter::ParseMesh(const std::filesystem::path& FbxPath, fbxsdk::FbxMesh* Mesh, TArray<FVertexPNCT>& OutVertices, TArray<int32>& OutVertexToControlPoints, TArray<FMeshMaterialSlot>& OutMaterials, TArray<FTriangleFace>& OutFaces)
{
	fbxsdk::FbxNode* Node = Mesh->GetNode();

	fbxsdk::FbxVector4 Translation = Node->GetGeometricTranslation(fbxsdk::FbxNode::eSourcePivot);
	fbxsdk::FbxVector4 Rotation = Node->GetGeometricRotation(fbxsdk::FbxNode::eSourcePivot);
	fbxsdk::FbxVector4 Scale = Node->GetGeometricScaling(fbxsdk::FbxNode::eSourcePivot);

	fbxsdk::FbxAMatrix GeometryTransform;
	GeometryTransform.SetTRS(Translation, Rotation, Scale);

	fbxsdk::FbxAMatrix GeometryTransformForNormal = GeometryTransform.Inverse().Transpose();

	auto* TangentElement = Mesh->GetElementTangent();
	auto TangentMappingMode = TangentElement ? TangentElement->GetMappingMode() : fbxsdk::FbxLayerElement::eNone;
	auto TangentReferenceMode = TangentElement ? TangentElement->GetReferenceMode() : fbxsdk::FbxLayerElement::eDirect;

	auto* BinormalElement = Mesh->GetElementBinormal();
	auto BinormalMappingMode = BinormalElement ? BinormalElement->GetMappingMode() : fbxsdk::FbxLayerElement::eNone;
	auto BinormalReferenceMode = BinormalElement ? BinormalElement->GetReferenceMode() : fbxsdk::FbxLayerElement::eDirect;

	TMap<uint64, int32> MaterialIDToIndexMap;
	for (int32 PolygonIndex = 0; PolygonIndex < Mesh->GetPolygonCount(); PolygonIndex++)
	{
		int32 PolygonSize = Mesh->GetPolygonSize(PolygonIndex);
		if (PolygonSize != 3)
		{
			// 왜 여길 온 거지? Triangulate에서 삼각화를 수행했는데...?
			throw std::runtime_error("Undefined behavior");
		}

		FTriangleFace& Face = OutFaces.AddDefaulted_GetRef();

		// Vertex 정보 파싱
		for (int32 i = 0; i < PolygonSize; i++)
		{
			int32 VertexIndex = Mesh->GetPolygonVertex(PolygonIndex, i);

			FVertexPNCT& Vertex = OutVertices.AddDefaulted_GetRef();

			// Position 정보 파싱
			fbxsdk::FbxVector4 Position = Mesh->GetControlPointAt(VertexIndex);
			fbxsdk::FbxVector4 FinalPosition = GeometryTransform.MultT(Position);
			FinalPosition[1] *= -1;

			Vertex.Position = FVector(FinalPosition[0], FinalPosition[1], FinalPosition[2]);

			// Normal 정보 파싱
			fbxsdk::FbxVector4 Normal;
			if (Mesh->GetPolygonVertexNormal(PolygonIndex, i, Normal))
			{
				fbxsdk::FbxVector4 FinalNormal = GeometryTransformForNormal.MultT(Normal);
				FinalNormal[1] *= -1;
				FinalNormal.Normalize();

				Vertex.Normal = FVector(FinalNormal[0], FinalNormal[1], FinalNormal[2]);

				// Tangent 정보 파싱
				if (TangentElement && BinormalElement)
				{
					int32 TangentMappingIndex = -1;
					if (TangentMappingMode == fbxsdk::FbxLayerElement::eByPolygonVertex)
					{
						TangentMappingIndex = Mesh->GetPolygonVertexIndex(PolygonIndex) + i;
					}
					else if (TangentMappingMode == fbxsdk::FbxLayerElement::eByControlPoint)
					{
						TangentMappingIndex = VertexIndex;
					}

					int32 TangentIndex = -1;
					if (TangentReferenceMode == fbxsdk::FbxLayerElement::eDirect)
					{
						TangentIndex = TangentMappingIndex;
					}
					else if (TangentReferenceMode == fbxsdk::FbxLayerElement::eIndexToDirect)
					{
						TangentIndex = TangentElement->GetIndexArray().GetAt(TangentMappingIndex);
					}

					int32 BinormalMappingIndex = -1;
					if (BinormalMappingMode == fbxsdk::FbxLayerElement::eByPolygonVertex)
					{
						BinormalMappingIndex = Mesh->GetPolygonVertexIndex(PolygonIndex) + i;
					}
					else if (BinormalMappingMode == fbxsdk::FbxLayerElement::eByControlPoint)
					{
						BinormalMappingIndex = VertexIndex;
					}

					int32 BinormalIndex = -1;
					if (BinormalReferenceMode == fbxsdk::FbxLayerElement::eDirect)
					{
						BinormalIndex = BinormalMappingIndex;
					}
					else if (BinormalReferenceMode == fbxsdk::FbxLayerElement::eIndexToDirect)
					{
						BinormalIndex = BinormalElement->GetIndexArray().GetAt(BinormalMappingIndex);
					}

					fbxsdk::FbxVector4 Tangent = TangentElement->GetDirectArray().GetAt(TangentIndex);
					fbxsdk::FbxVector4 FinalTangent = GeometryTransformForNormal.MultT(fbxsdk::FbxVector4(Tangent[0], Tangent[1], Tangent[2], 0.f));
					FinalTangent[1] *= -1;
					FinalTangent.Normalize();

					fbxsdk::FbxVector4 Bitangent = BinormalElement->GetDirectArray().GetAt(BinormalIndex);
					fbxsdk::FbxVector4 FinalBitangent = GeometryTransformForNormal.MultT(fbxsdk::FbxVector4(Bitangent[0], Bitangent[1], Bitangent[2], 0.f));
					FinalBitangent[1] *= -1;
					FinalBitangent.Normalize();

					float Sign = FinalNormal.CrossProduct(FinalTangent).DotProduct(FinalBitangent) < 0 ? -1 : +1;

					Vertex.Tangent = FVector4(FinalTangent[0], FinalTangent[1], FinalTangent[2], Sign);
				}
			}

			// UV 정보 파싱
			fbxsdk::FbxStringList UVNames;
			Mesh->GetUVSetNames(UVNames);

			for (int32 j = 0; j < UVNames.GetCount(); j++)
			{
				fbxsdk::FbxString UVName = UVNames.GetStringAt(j);

				fbxsdk::FbxVector2 UV;
				bool Unmapped;
				Mesh->GetPolygonVertexUV(PolygonIndex, i, UVName, UV, Unmapped);

				Vertex.UV = FVector2(UV[0], 1.f - UV[1]);
			}

			Vertex.Color = FVector(1.f);

			OutVertexToControlPoints.Add(VertexIndex);

			Face.VertexIndices[i] = OutVertices.Num() - 1;

			// NOTE: 거울 반전으로 인해 인덱스를 뒤집는다. (A -> B -> C) => (A -> C -> B)
			if ((i + 1) % 3 == 0)
			{
				std::swap(Face.VertexIndices[i], Face.VertexIndices[i - 1]);
			}
		}

		// Material 정보 파싱
		uint32 MaterialSlotIndex = UINT32_MAX;

		fbxsdk::FbxGeometryElementMaterial* MaterialElement = Mesh->GetElementMaterial();
		if (MaterialElement)
		{
			fbxsdk::FbxLayerElement::EMappingMode MappingMode = MaterialElement->GetMappingMode();

			int32 ArrayIndex = -1;
			switch (MappingMode)
			{
				case fbxsdk::FbxLayerElement::EMappingMode::eAllSame: ArrayIndex = 0; break;
				case fbxsdk::FbxLayerElement::EMappingMode::eByPolygon: ArrayIndex = PolygonIndex; break;
			}

			const auto& MaterialIndices = MaterialElement->GetIndexArray();
			if (ArrayIndex >= 0 && ArrayIndex < MaterialIndices.GetCount())
			{
				int32 MaterialIndex = MaterialIndices.GetAt(ArrayIndex);

				fbxsdk::FbxSurfaceMaterial* SurfaceMaterial = Node->GetMaterial(MaterialIndex);
				fbxsdk::FbxUInt64 SurfaceMaterialID = SurfaceMaterial->GetUniqueID();

				if (MaterialIDToIndexMap.Contains(SurfaceMaterialID))
				{
					MaterialSlotIndex = MaterialIDToIndexMap[SurfaceMaterialID];
				}
				else
				{
					MaterialSlotIndex = OutMaterials.Num();

					FMeshMaterialSlot& MaterialSlot = OutMaterials.AddDefaulted_GetRef();
					ParseMaterial(FbxPath.parent_path(), SurfaceMaterial, MaterialSlot);

					MaterialIDToIndexMap[SurfaceMaterialID] = MaterialSlotIndex;
				}
			}
		}

		Face.MaterialSlotIndex = MaterialSlotIndex;
	}
}

void FFbxImporter::CompactSoftSkin(FSoftModelVertexInfluences& Skin)
{
	if (Skin.BoneIndices.Num() < MaxBoneInfluenceCount)
	{
		return;
	}

	// 같은 본이 중복되면 합친다.
	int32 InfluenceCount = Skin.BoneIndices.Num();
	for (int32 i = 0; i < InfluenceCount; i++)
	{
		for (int32 j = i + 1; j < InfluenceCount; j++)
		{
			if (Skin.BoneIndices[i] != Skin.BoneIndices[j])
			{
				continue;
			}

			Skin.Weights[i] += Skin.Weights[j];
			Skin.BoneIndices[j] = Skin.BoneIndices[InfluenceCount - 1];
			Skin.Weights[j] = Skin.Weights[InfluenceCount - 1];
			InfluenceCount--;
			j--;
		}
	}

	// 내림차순으로 정렬
	for (int32 i = 0; i < InfluenceCount; i++)
	{
		for (int32 j = i + 1; j < InfluenceCount; j++)
		{
			if (Skin.Weights[i] >= Skin.Weights[j])
			{
				continue;
			}

			std::swap(Skin.BoneIndices[i], Skin.BoneIndices[j]);
			std::swap(Skin.Weights[i], Skin.Weights[j]);
		}
	}

	int32 FinalInfluenceCount = FMath::Min(InfluenceCount, MaxBoneInfluenceCount);

	// 상위 Max개 합 계산
	float Sum = 0;
	for (int32 i = 0; i < FinalInfluenceCount; i++)
	{
		Sum += Skin.Weights[i];
	}

	// 합으로 나눠서 합이 1.0이 되게 하기
	if (Sum > 0.f)
	{
		for (int32 i = 0; i < FinalInfluenceCount; i++)
		{
			Skin.Weights[i] /= Sum;
		}
	}

	Skin.BoneIndices.SetNum(FinalInfluenceCount);
	Skin.Weights.SetNum(FinalInfluenceCount);
}

void FFbxImporter::ParseSkin(const TArray<int32>& VertexToControlPoints, const TMap<uint64, int32>& NodeToIndexMap, const TMap<int32, int32>& NodeIndexToBoneIndexMap, const FMeshNode& MeshNode, TArray<FModelBone>& Bones, TArray<FModelVertexInfluences>& OutVertexInfluencesList, FMatrix& OutBindGlobalTransform)
{
	fbxsdk::FbxMesh* Mesh = MeshNode.FbxNode->GetMesh();

	TArray<FSoftModelVertexInfluences> SoftVerteInfluences;
	for (int32 i = 0; i < Mesh->GetDeformerCount(fbxsdk::FbxDeformer::eSkin); i++)
	{
		fbxsdk::FbxSkin* Skin = (fbxsdk::FbxSkin*)Mesh->GetDeformer(i, fbxsdk::FbxDeformer::eSkin);

		for (int32 j = 0; j < Skin->GetClusterCount(); j++)
		{
			fbxsdk::FbxCluster* Cluster = Skin->GetCluster(j);
			fbxsdk::FbxNode* Bone = Cluster->GetLink();

			uint64 BoneID = Bone->GetUniqueID();
			int32 BoneNodeIndex = NodeToIndexMap[BoneID];
			int32 BoneIndex = NodeIndexToBoneIndexMap[BoneNodeIndex];

			int32* CtrlPointIndices = Cluster->GetControlPointIndices();
			double* CtrlPointWeights = Cluster->GetControlPointWeights();
			for (int32 k = 0; k < Cluster->GetControlPointIndicesCount(); k++)
			{
				int32 CtrlPointIndex = CtrlPointIndices[k];
				double CtrlPointWeight = CtrlPointWeights[k];

				if (CtrlPointIndex >= SoftVerteInfluences.Num())
				{
					SoftVerteInfluences.SetNum(CtrlPointIndex + 1);
				}

				FSoftModelVertexInfluences& SoftVertexInflueces = SoftVerteInfluences[CtrlPointIndex];
				SoftVertexInflueces.BoneIndices.Add(BoneIndex);
				SoftVertexInflueces.Weights.Add(CtrlPointWeight);
			}

			if (i == 0 && j == 0)
			{
				fbxsdk::FbxAMatrix BindGlobalTransform;
				Cluster->GetTransformMatrix(BindGlobalTransform);

				OutBindGlobalTransform = ConvertMatrix(BindGlobalTransform);
			}

			fbxsdk::FbxAMatrix BoneTransform;
			Cluster->GetTransformLinkMatrix(BoneTransform);

			Bones[BoneIndex].InverseBindTransform = ConvertMatrix(BoneTransform.Inverse());
		}
	}

	for (int32 i = 0; i < VertexToControlPoints.Num(); i++)
	{
		int32 ControlPointIndex = VertexToControlPoints[i];

		FModelVertexInfluences& VertexInfluences = OutVertexInfluencesList.AddDefaulted_GetRef();
		if (ControlPointIndex >= SoftVerteInfluences.Num())
		{
			continue;
		}

		FSoftModelVertexInfluences& SoftVertexInfluences = SoftVerteInfluences[ControlPointIndex];
		CompactSoftSkin(SoftVertexInfluences);

		for (int32 j = 0; j < SoftVertexInfluences.BoneIndices.Num(); j++)
		{
			VertexInfluences.BoneIndices[j] = SoftVertexInfluences.BoneIndices[j];
			VertexInfluences.Weights[j] = SoftVertexInfluences.Weights[j];
			VertexInfluences.InfluenceCount++;
		}
	}
}


