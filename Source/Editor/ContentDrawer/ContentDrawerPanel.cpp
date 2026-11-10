#include "EnginePCH.h"
#include "Editor/ContentDrawer/ContentDrawerPanel.h"
#include "Asset/AssetManager.h"
#include "Core/PathUtils.h"
#include "Text/Font.h"
#include "Editor/HitoriEd/EditorDragDrop.h"
#include "Editor/HitoriEd/EditorResourcePaths.h"
#include "Editor/Script/LuaScriptEditor.h"
#include "Editor/HitoriEd/EditorFileUtils.h"
#include "Sequence/LevelSequence.h"
#include "Sequence/MovieScene.h"
#include "Serialization/JsonArchive.h"
#include "Animation/BlendSpace1D.h"
#include "Render/Mesh.h"
#include "Render/SkeletalMesh.h"
#include "UObject/UObjectIterator.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationGraph.h"

bool FContentDrawerPanel::Init()
{
	FolderIcon = UAssetManager::Get().LoadTexture(EditorResourcePaths::FolderIcon);
	FileIcon = UAssetManager::Get().LoadTexture(EditorResourcePaths::FileIcon);
	return false;
}

void FContentDrawerPanel::Tick(float DeltaTime)
{
}

void FContentDrawerPanel::OnRender()
{
	ImGui::Begin("Content Drawer");
	std::error_code Error;

	if (!fs::is_directory(CurrentPath, Error))
	{
		CurrentPath = AssetRootPath;
	}

	Error.clear();

	if (!fs::is_directory(AssetRootPath, Error))
	{
		ImGui::TextUnformatted("Asset folder is unavailable.");
		ImGui::End();
		return;
	}

	float treeViewWidth = 200.0f;
	ImGui::BeginChild("FolderTreeView", ImVec2(treeViewWidth, 0));
	{
		// 루트 노드
		if (ImGui::TreeNodeEx(PathUtils::ToUtf8(AssetRootPath).c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
			(AssetRootPath == CurrentPath ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_DrawLinesToNodes))
		{
			if (ImGui::IsItemClicked())
			{
				CurrentPath = AssetRootPath;
			}
			RenderFolderTree(AssetRootPath, CurrentPath);
			ImGui::TreePop();
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("AssetGridView");
	{
		DrawPathBar();

		if (ImGui::Button("Import OBJ"))
		{
			ImportModel(false);
		}

		ImGui::SameLine();

		if (ImGui::Button("Import FBX"))
		{
			ImportModel(true);
		}

		ImGui::Separator();
		if (!CreateAssetError.empty())
		{
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.35f, 1.0f), "%s", CreateAssetError.c_str());
		}

		// 그리드 레이아웃 계산
		float padding = 16.0f;
		float thumbnailSize = 80.0f;
		float cellSize = thumbnailSize + padding;
		float panelWidth = ImGui::GetContentRegionAvail().x;
		int ColumnCount = static_cast<int>(panelWidth / cellSize);
		if (ColumnCount < 1) ColumnCount = 1;

		// 테이블 기반 그리드 시작
		if (ImGui::BeginTable("AssetGridTable", ColumnCount))
		{
			std::error_code IterateError;
			fs::directory_iterator It(CurrentPath, IterateError);
			const fs::directory_iterator End;

			for (; !IterateError && It != End; It.increment(IterateError))
			{
				const fs::path Path = It->path();

				std::error_code TypeError;
				const bool bIsDirectory = fs::is_directory(Path, TypeError);

				if (TypeError)
					continue;

				if (!bIsDirectory && (Path.extension() == ".bin" || Path.extension() == ".animnotify" || Path.extension() == ".pose"))
					continue;

				ImGui::TableNextColumn();

				std::string PathString = PathUtils::ToUtf8(Path);
				std::string FilenameString = PathUtils::ToUtf8(Path.filename());

				ImGui::PushID(PathString.c_str()); // 각 위젯에 고유 ID 부여
				UTexture2D* Thumbnail = nullptr;

				// 실제 에셋일 때만 채워진다.
				// 폴더/파일 아이콘은 끌 수 없어야 하므로 썸네일과 구분해서 들고 있는다.
				UTexture2D* TextureAsset = nullptr;
				UFont* FontAsset = nullptr;

				// 폴더 또는 파일 아이콘 표시
				if (bIsDirectory)
				{
					Thumbnail = FolderIcon;
				}
				else // 파일인 경우
				{
					UAsset* Asset = UAssetManager::GetAssetByPath<UAsset>(PathString);
					if (Asset && Asset->IsA<UTexture2D>())
					{
						TextureAsset = Cast<UTexture2D>(Asset);
						Thumbnail = TextureAsset;
					}
					else if (Asset && Asset->IsA<UFont>())
					{
						FontAsset = Cast<UFont>(Asset);

						// 폰트는 글리프 아틀라스를 그대로 미리보기로 쓴다
						Thumbnail = FontAsset->AtlasTexture;
					}

					if (Thumbnail == nullptr)
					{
						Thumbnail = FileIcon;
					}
				}
				if (Thumbnail)
				{
					ImGui::ImageButton(PathString.c_str(), (ImTextureID)Thumbnail->GetResource()->GetRawSRV(), { thumbnailSize, thumbnailSize });
				}

				// 페이로드에 포인터를 그대로 실어서 받는 쪽이 AssetMap을 다시 뒤지지 않게 한다.
				if (TextureAsset && ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload(EditorDragDrop::Texture, &TextureAsset, sizeof(UTexture2D*));

					// 끌고 다니는 동안 커서를 따라다니는 미리보기
					ImGui::Image((ImTextureID)TextureAsset->GetResource()->GetRawSRV(), { thumbnailSize, thumbnailSize });
					ImGui::TextUnformatted(FilenameString.c_str());

					ImGui::EndDragDropSource();
				}
				if (FontAsset && ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload(EditorDragDrop::Font, &FontAsset, sizeof(UFont*));

					ImGui::TextUnformatted(FilenameString.c_str());

					ImGui::EndDragDropSource();
				}
				if (!bIsDirectory && Path.extension() == ".sequence" && ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload(EditorDragDrop::Sequence, PathString.c_str(), PathString.size() + 1);
					ImGui::TextUnformatted(FilenameString.c_str());
					ImGui::EndDragDropSource();
				}
				// 더블 클릭 이벤트 처리
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					if (bIsDirectory)
					{
						CurrentPath /= Path.filename(); // 하위 폴더로 이동
					}
					else if (Path.extension() == ".lua")
					{
						FLuaScriptEditor::EditScript(PathString);
					}
					else if (Path.extension() == ".sequence" && OpenLevelSequence)
					{
						OpenLevelSequence(PathString);
					}
					else if (Path.extension() == ".fbx")
					{
						UAssetManager::Get().LoadFbxModel(PathString, true, true, false);

						const FString MeshKey = PathUtils::ToUtf8(fs::relative(Path, AssetRootPath));
						USkeletalMesh* SkeletalMesh = UAssetManager::GetAssetByPath<USkeletalMesh>(MeshKey);

						if (SkeletalMesh && OpenSkeletalMeshViewer)
						{
							OpenSkeletalMeshViewer(SkeletalMesh);
						}
						else
						{
							CreateAssetError = "Could not open Skeletal Mesh Viewer. Check whether the FBX contains a skeleton.";
						}
					}
					else if (Path.extension() == ".anim")
					{
						USkeletalMesh* SkeletalMesh = nullptr;
						UAnimSequence* Animation = UAssetManager::LoadAnimSequence(PathString, &SkeletalMesh);

						if (Animation && SkeletalMesh && OpenAnimationSequenceViewer)
						{
							OpenAnimationSequenceViewer(SkeletalMesh, Animation);
						}
						else
						{
							CreateAssetError = "Could not open Animation Sequence Viewer. Check the animation asset and its source FBX.";
						}
					}
					else if (Path.extension() == ".blendspace")
					{
						UBlendSpace1D* BlendSpace = UAssetManager::LoadBlendSpace(PathString);
						if (BlendSpace && OpenBlendSpace)
						{
							OpenBlendSpace(BlendSpace);
						}
						else
						{
							CreateAssetError = "Could not open BlendSpace.";
						}
					}
					else if (Path.extension() == ".animgraph")
					{
						UAnimationGraph* Graph = UAssetManager::LoadAnimationGraph(PathString);

						if (Graph && OpenAnimationGraph)
						{
							OpenAnimationGraph(Graph);
						}
						else
						{
							CreateAssetError = "Could not open Animation Graph.";
						}
					}
				}
				// 파일 이름 표시
				ImGui::TextWrapped("%s", FilenameString.c_str());

				ImGui::PopID();
			}
			ImGui::EndTable();
		}
		if (ImGui::BeginPopupContextWindow("AssetContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::BeginMenu("Create"))
			{
				if (ImGui::MenuItem("Level Sequence"))
				{
					CreateLevelSequenceFile();
				}

				if (ImGui::MenuItem("Blend Space 1D"))
				{
					SelectedCreateMesh = nullptr;
					bOpenCreateBlendSpacePopup = true;
				}

				if (ImGui::MenuItem("Animation Graph"))
				{
					SelectedCreateAnimationGraphMesh = nullptr;
					SelectedCreateAnimationGraphSequence = nullptr;

					bOpenCreateAnimationGraphPopup = true;
				}
				ImGui::EndMenu();
			}
			ImGui::EndPopup();
		}
		if (bOpenCreateBlendSpacePopup)
		{
			ImGui::OpenPopup("Create Blend Space 1D");
			bOpenCreateBlendSpacePopup = false;
		}

		if (ImGui::BeginPopupModal("Create Blend Space 1D", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			TArray<USkeletalMesh*> AvailableMeshes;
			for (TObjectIterator<USkeletalMesh> It; It; ++It)
			{
				USkeletalMesh* Mesh = *It;
				if (Mesh && Mesh->GetSkeleton())
				{
					AvailableMeshes.Add(Mesh);
				}
			}

			std::sort(AvailableMeshes.begin(), AvailableMeshes.end(), [](const USkeletalMesh* A, const USkeletalMesh* B) { return A->GetPath() < B->GetPath(); });
			
			if (SelectedCreateMesh && AvailableMeshes.Find(SelectedCreateMesh) == -1)
			{
				SelectedCreateMesh = nullptr;
			}
			const FString CurrentName = SelectedCreateMesh ? SelectedCreateMesh->GetPath() : "Select Skeletal Mesh";
			ImGui::SetNextItemWidth(350.0f);
			if (ImGui::BeginCombo("Skeletal Mesh", CurrentName.c_str()))
			{
				for (USkeletalMesh* Mesh : AvailableMeshes)
				{
					const bool bSelected = Mesh == SelectedCreateMesh;
					if (ImGui::Selectable(Mesh->GetPath().c_str(), bSelected))
					{
						SelectedCreateMesh = Mesh;
					}

					if (bSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}

				ImGui::EndCombo();
			}
			ImGui::Spacing();
			ImGui::BeginDisabled(SelectedCreateMesh == nullptr);
			if (ImGui::Button("Create"))
			{
				if (CreateBlendSpaceFile(SelectedCreateMesh))
				{
					SelectedCreateMesh = nullptr;
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::EndDisabled();
			ImGui::SameLine();

			if (ImGui::Button("Cancel"))
			{
				SelectedCreateMesh = nullptr;
				ImGui::CloseCurrentPopup();
			}
			if (AvailableMeshes.IsEmpty())
			{
				ImGui::TextDisabled("No compatible Skeletal Mesh is loaded.");
			}
			ImGui::EndPopup();
		}

		if (bOpenCreateAnimationGraphPopup)
		{
			ImGui::OpenPopup("Create Animation Graph");
			bOpenCreateAnimationGraphPopup = false;
		}

		if (ImGui::BeginPopupModal("Create Animation Graph", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			TArray<USkeletalMesh*> AvailableMeshes;

			for (TObjectIterator<USkeletalMesh> It; It; ++It)
			{
				USkeletalMesh* Mesh = *It;

				if (Mesh && Mesh->GetSkeleton())
				{
					AvailableMeshes.Add(Mesh);
				}
			}

			std::sort(AvailableMeshes.begin(), AvailableMeshes.end(), [](const USkeletalMesh* A, const USkeletalMesh* B) { return A->GetPath() < B->GetPath(); });

			if (SelectedCreateAnimationGraphMesh && AvailableMeshes.Find(SelectedCreateAnimationGraphMesh) == -1)
			{
				SelectedCreateAnimationGraphMesh = nullptr;
				SelectedCreateAnimationGraphSequence = nullptr;
			}

			const FString MeshLabel = SelectedCreateAnimationGraphMesh ? SelectedCreateAnimationGraphMesh->GetPath() : "Select Skeletal Mesh";

			ImGui::SetNextItemWidth(350.0f);

			if (ImGui::BeginCombo("Skeletal Mesh", MeshLabel.c_str()))
			{
				for (USkeletalMesh* Mesh : AvailableMeshes)
				{
					const bool bSelected = Mesh == SelectedCreateAnimationGraphMesh;

					if (ImGui::Selectable(Mesh->GetPath().c_str(), bSelected))
					{
						SelectedCreateAnimationGraphMesh = Mesh;
						SelectedCreateAnimationGraphSequence = nullptr;
					}

					if (bSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}

				ImGui::EndCombo();
			}

			TArray<UAnimSequence*> AvailableAnimations;

			if (SelectedCreateAnimationGraphMesh)
			{
				USkeleton* Skeleton = SelectedCreateAnimationGraphMesh->GetSkeleton();

				for (TObjectIterator<UAnimSequence> It; It; ++It)
				{
					UAnimSequence* Animation = *It;

					if (Animation && Animation->GetSkeleton() == Skeleton)
					{
						AvailableAnimations.Add(Animation);
					}
				}

				std::sort(AvailableAnimations.begin(), AvailableAnimations.end(), [](const UAnimSequence* A, const UAnimSequence* B) { return A->GetPath() < B->GetPath(); });
			}

			if (SelectedCreateAnimationGraphSequence && AvailableAnimations.Find(SelectedCreateAnimationGraphSequence) == -1)
			{
				SelectedCreateAnimationGraphSequence = nullptr;
			}

			if (!SelectedCreateAnimationGraphSequence && !AvailableAnimations.IsEmpty())
			{
				SelectedCreateAnimationGraphSequence = AvailableAnimations[0];
			}

			const FString AnimationLabel = SelectedCreateAnimationGraphSequence ? SelectedCreateAnimationGraphSequence->GetPath() : "Select Initial Animation";

			ImGui::SetNextItemWidth(350.0f);

			if (ImGui::BeginCombo("Initial Animation", AnimationLabel.c_str()))
			{
				for (UAnimSequence* Animation : AvailableAnimations)
				{
					const bool bSelected = Animation == SelectedCreateAnimationGraphSequence;

					if (ImGui::Selectable(Animation->GetPath().c_str(), bSelected))
					{
						SelectedCreateAnimationGraphSequence = Animation;
					}

					if (bSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}

				ImGui::EndCombo();
			}

			ImGui::Spacing();

			const bool bCanCreate = SelectedCreateAnimationGraphMesh != nullptr && SelectedCreateAnimationGraphSequence != nullptr;

			ImGui::BeginDisabled(!bCanCreate);

			if (ImGui::Button("Create"))
			{
				if (CreateAnimationGraphFile(SelectedCreateAnimationGraphMesh, SelectedCreateAnimationGraphSequence))
				{
					SelectedCreateAnimationGraphMesh = nullptr;
					SelectedCreateAnimationGraphSequence = nullptr;

					ImGui::CloseCurrentPopup();
				}
			}

			ImGui::EndDisabled();

			ImGui::SameLine();

			if (ImGui::Button("Cancel"))
			{
				SelectedCreateAnimationGraphMesh = nullptr;
				SelectedCreateAnimationGraphSequence = nullptr;

				ImGui::CloseCurrentPopup();
			}

			if (AvailableMeshes.IsEmpty())
			{
				ImGui::TextDisabled("No compatible Skeletal Mesh is loaded.");
			}
			else if (SelectedCreateAnimationGraphMesh && AvailableAnimations.IsEmpty())
			{
				ImGui::TextDisabled("No Animation Sequence uses this Skeleton.");
			}

			ImGui::EndPopup();
		}
	}
	ImGui::EndChild();
	ImGui::End();
}

void FContentDrawerPanel::ImportModel(bool bImportFbx)
{
	CreateAssetError.clear();

	const FString SelectedFile = FEditorFileUtils::OpenImportModelDialog(PathUtils::ToUtf8(CurrentPath), bImportFbx);
	if (SelectedFile.empty())
	{
		return;
	}

	const fs::path SelectedPath = PathUtils::FromUtf8(SelectedFile);

	std::error_code PathError;
	const fs::path AbsoluteRoot = fs::absolute(AssetRootPath, PathError).lexically_normal();
	if (PathError)
	{
		CreateAssetError = "Could not resolve the Assets directory.";
		return;
	}

	const fs::path AbsoluteFile = fs::absolute(SelectedPath, PathError).lexically_normal();
	if (PathError)
	{
		CreateAssetError = "Could not resolve the selected file path.";
		return;
	}

	const fs::path RelativePath = fs::relative(AbsoluteFile, AbsoluteRoot, PathError);
	if (PathError || RelativePath.empty() || *RelativePath.begin() == "..")
	{
		CreateAssetError = "Please select a model file inside the Assets directory.";
		return;
	}

	const FString AssetKey = PathUtils::ToUtf8(RelativePath);

	if (bImportFbx)
	{
		if (RelativePath.extension() != ".fbx")
		{
			CreateAssetError = "Please select an FBX file.";
			return;
		}

		UAssetManager::Get().LoadFbxModel(SelectedFile, true, true, true);

		if (!UAssetManager::GetAssetByPath<USkeletalMesh>(AssetKey) && !UAssetManager::GetAssetByPath<UStaticMesh>(AssetKey))
		{
			CreateAssetError = "FBX import failed. Check the Output Log.";
			return;
		}
	}
	else
	{
		if (RelativePath.extension() != ".obj")
		{
			CreateAssetError = "Please select an OBJ file.";
			return;
		}

		if (!UAssetManager::LoadObjStaticMesh(SelectedFile, true, true))
		{
			CreateAssetError = "OBJ import failed. Check the Output Log.";
			return;
		}
	}

	HTR_LOG(Info, "Model imported: {}", SelectedFile);
}

void FContentDrawerPanel::CreateLevelSequenceFile()
{
	CreateAssetError.clear();
	ULevelSequence* Sequence = nullptr;
	UMovieScene* Scene = nullptr;
	try
	{
		fs::path Path = CurrentPath / "NewLevelSequence.sequence";
		for (int32 Index = 1; fs::exists(Path); ++Index)
		{
			Path = CurrentPath / ("NewLevelSequence_" + std::to_string(Index) + ".sequence");
		}
		Sequence = NewObject<ULevelSequence>();
		Scene = NewObject<UMovieScene>(Sequence);
		Scene->Initialize(5.f, 150);
		Sequence->Initialize(Scene);
		if (!FJsonArchive::SaveSequence(Sequence, PathUtils::ToUtf8(Path)))
		{
			CreateAssetError = "Could not create the level sequence file.";
		}
	}
	catch (const std::exception& Error)
	{
		CreateAssetError = "Create failed: " + FString(Error.what());
	}
	delete Scene;
	delete Sequence;
}

bool FContentDrawerPanel::CreateBlendSpaceFile(USkeletalMesh* InSkeletalMesh)
{
	CreateAssetError.clear();
	if (!InSkeletalMesh || !InSkeletalMesh->GetSkeleton())
	{
		CreateAssetError = "A valid Skeletal Mesh is required.";
		return false;
	}
	fs::path Path = CurrentPath / "NewBlendSpace1D.blendspace";
	for (int32 Index = 1; fs::exists(Path); ++Index)
	{
		Path = CurrentPath / ("NewBlendSpace1D_" + std::to_string(Index) + ".blendspace");
	}
	const FString FilePath = PathUtils::ToUtf8(Path);
	UBlendSpace1D* NewBlendSpace = NewObject<UBlendSpace1D>();
	if (!NewBlendSpace->SetSkeleton(InSkeletalMesh->GetSkeleton()) || !NewBlendSpace->SetPreviewMesh(InSkeletalMesh))
	{
		delete NewBlendSpace;
		CreateAssetError = "Could not set the BlendSpace Skeleton or Preview Mesh.";
		return false;
	}

	if (!FJsonArchive::SaveBlendSpace(NewBlendSpace, FilePath))
	{
		delete NewBlendSpace;
		CreateAssetError = "Could not save the BlendSpace file.";
		return false;
	}

	delete NewBlendSpace;

	UBlendSpace1D* CreatedBlendSpace = UAssetManager::LoadBlendSpace(FilePath);
	if (!CreatedBlendSpace)
	{
		CreateAssetError = "Could not register the BlendSpace asset.";
		return false;
	}

	if (OpenBlendSpace)
	{
		OpenBlendSpace(CreatedBlendSpace);
	}

	return true;
}

bool FContentDrawerPanel::CreateAnimationGraphFile(USkeletalMesh* InSkeletalMesh, UAnimSequence* InitialAnimation)
{
	CreateAssetError.clear();

	if (!InSkeletalMesh || !InSkeletalMesh->GetSkeleton() || !InitialAnimation || InitialAnimation->GetSkeleton() != InSkeletalMesh->GetSkeleton())
	{
		CreateAssetError = "Invalid Skeletal Mesh or Animation.";
		return false;
	}

	fs::path Path = CurrentPath / "NewAnimationGraph.animgraph";

	for (int32 Index = 1; fs::exists(Path); ++Index)
	{
		Path = CurrentPath / ("NewAnimationGraph_" + std::to_string(Index) + ".animgraph");
	}

	const FString FilePath = PathUtils::ToUtf8(Path);

	UAnimationGraph* NewGraph = NewObject<UAnimationGraph>();

	if (!NewGraph)
	{
		CreateAssetError = "Could not create Animation Graph.";
		return false;
	}

	const FName EntryStateName("Entry");

	if (!NewGraph->SetSkeleton(InSkeletalMesh->GetSkeleton()) || !NewGraph->AddSequenceState(EntryStateName, InitialAnimation) || !NewGraph->SetEntryState(EntryStateName))
	{
		delete NewGraph;

		CreateAssetError = "Could not initialize Animation Graph.";
		return false;
	}

	if (!FJsonArchive::SaveAnimationGraph(NewGraph, FilePath))
	{
		delete NewGraph;

		CreateAssetError = "Could not save Animation Graph.";
		return false;
	}

	delete NewGraph;

	UAnimationGraph* CreatedGraph = UAssetManager::LoadAnimationGraph(FilePath);

	if (!CreatedGraph)
	{
		CreateAssetError = "Could not register Animation Graph asset.";
		return false;
	}

	if (OpenAnimationGraph)
	{
		OpenAnimationGraph(CreatedGraph);
	}

	return true;
}

void FContentDrawerPanel::DrawPathBar()
{
	// 순회 중에 CurrentPath를 바꾸지 않도록 이동할 폴더만 기록해 두고 마지막에 반영한다.
	fs::path NextPath = CurrentPath;

	// 상위 폴더로 이동. 루트에서는 비활성화한다.
	ImGui::BeginDisabled(CurrentPath == AssetRootPath);
	if (ImGui::ArrowButton("##ParentFolder", ImGuiDir_Left))
		NextPath = CurrentPath.parent_path();
	ImGui::EndDisabled();
	ImGui::SetItemTooltip("Parent Folder");

	ImGui::SameLine();
	if (ImGui::SmallButton(PathUtils::ToUtf8(AssetRootPath).c_str()))
		NextPath = AssetRootPath;

	fs::path SegmentPath = AssetRootPath;
	int32 SegmentIndex = 0;
	for (const fs::path& Segment : fs::relative(CurrentPath, AssetRootPath))
	{
		if (Segment == ".")
			continue;

		SegmentPath /= Segment;
		ImGui::SameLine();
		ImGui::TextDisabled(">");
		ImGui::SameLine();

		// 폴더 이름이 같아도 위치가 다르면 다른 버튼이 되도록 순번으로 ID를 나눈다.
		ImGui::PushID(SegmentIndex++);
		if (ImGui::SmallButton(PathUtils::ToUtf8(Segment).c_str()))
			NextPath = SegmentPath;
		ImGui::PopID();
	}

	CurrentPath = NextPath;
}

void FContentDrawerPanel::RenderFolderTree(const fs::path& Path, fs::path& SelectedPath)
{
	std::error_code IterateError;
	fs::directory_iterator It(Path, IterateError);
	const fs::directory_iterator End;

	for (; !IterateError && It != End; It.increment(IterateError))
	{
		const fs::path Entry = It->path();

		std::error_code TypeError;
		const bool bIsDirectory =
			fs::is_directory(Entry, TypeError);

		if (TypeError || !bIsDirectory)
			continue;

		const FString FilenameString =
			PathUtils::ToUtf8(Entry.filename());

		ImGuiTreeNodeFlags Flags =
			ImGuiTreeNodeFlags_OpenOnArrow |
			ImGuiTreeNodeFlags_SpanAvailWidth |
			ImGuiTreeNodeFlags_DrawLinesToNodes;

		if (Entry == SelectedPath)
			Flags |= ImGuiTreeNodeFlags_Selected;

		const bool bIsOpen =
			ImGui::TreeNodeEx(FilenameString.c_str(), Flags);

		if (ImGui::IsItemClicked())
			SelectedPath = Entry;

		if (bIsOpen)
		{
			RenderFolderTree(Entry, SelectedPath);
			ImGui::TreePop();
		}
	}

	if (IterateError)
	{
		ImGui::TextUnformatted("Could not read this folder.");
	}
}
