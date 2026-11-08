#pragma once

#include "Editor/EditorUI/EditorPanel.h"

#include <filesystem>
#include <functional>

namespace fs = std::filesystem;

class UTexture2D;
class UBlendSpace1D;
class USkeletalMesh;
class UAnimationGraph;
class UAnimSequence;

class FContentDrawerPanel : public IEditorPanel
{
public:
	bool Init() override;
	void Tick(float DeltaTime) override;
	void OnRender() override;
	const char* GetPanelName() const override { return "Content Drawer"; }
	void RenderFolderTree(const fs::path& Path, fs::path& SelectedPath);
	void SetOpenLevelSequenceBinding(std::function<void(const FString&)> InOpenLevelSequence) { OpenLevelSequence = std::move(InOpenLevelSequence); }
	void SetOpenBlendSpaceBinding(std::function<void(UBlendSpace1D*)> InOpenBlendSpace) { OpenBlendSpace = std::move(InOpenBlendSpace); }
	void SetOpenAnimationGraphBinding(std::function<void(UAnimationGraph*)> InOpenAnimationGraph) { OpenAnimationGraph = std::move(InOpenAnimationGraph); }
private:
	// 현재 폴더를 Assets > A > B 형태로 그린다. 조각을 누르면 그 폴더로 이동한다.
	void DrawPathBar();
	void CreateLevelSequenceFile();
	bool CreateBlendSpaceFile(USkeletalMesh* InSkeletalMesh);
	std::function<void(UBlendSpace1D*)> OpenBlendSpace;
	bool CreateAnimationGraphFile(USkeletalMesh* InSkeletalMesh, UAnimSequence* InitialAnimation);
	std::function<void(UAnimationGraph*)> OpenAnimationGraph;
	bool bOpenCreateAnimationGraphPopup = false;
	USkeletalMesh* SelectedCreateAnimationGraphMesh = nullptr;
	UAnimSequence* SelectedCreateAnimationGraphSequence = nullptr;

	fs::path AssetRootPath = "Assets";
	fs::path CurrentPath = "Assets";
	FString CreateAssetError;
	std::function<void(const FString&)> OpenLevelSequence;

	UTexture2D* FolderIcon = nullptr;
	UTexture2D* FileIcon = nullptr;

	bool bOpenCreateBlendSpacePopup = false;
	USkeletalMesh* SelectedCreateMesh = nullptr;
};
