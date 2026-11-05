#pragma once

#include "Editor/EditorUI/EditorPanel.h"

#include <filesystem>
#include <functional>

namespace fs = std::filesystem;

class UTexture2D;

class FContentDrawerPanel : public IEditorPanel
{
public:
	bool Init() override;
	void Tick(float DeltaTime) override;
	void OnRender() override;
	const char* GetPanelName() const override { return "Content Drawer"; }
	void RenderFolderTree(const fs::path& Path, fs::path& SelectedPath);
	void SetOpenLevelSequenceBinding(std::function<void(const FString&)> InOpenLevelSequence)
	{
		OpenLevelSequence = std::move(InOpenLevelSequence);
	}

private:
	// 현재 폴더를 Assets > A > B 형태로 그린다. 조각을 누르면 그 폴더로 이동한다.
	void DrawPathBar();
	void CreateLevelSequenceFile();

	fs::path AssetRootPath = "Assets";
	fs::path CurrentPath = "Assets";
	FString CreateAssetError;
	std::function<void(const FString&)> OpenLevelSequence;

	UTexture2D* FolderIcon = nullptr;
	UTexture2D* FileIcon = nullptr;

};
