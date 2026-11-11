#pragma once

enum class EWorldType : uint8
{
	None,
	Game,
	Editor,
	PIE,
	EditorPreview
};

enum class EBoneDrawMode : uint8
{
	AllHierarchy,       // 모든 계층구조
	SelectedOnly,       // 선택된 본만
	SelectedAndParents, // 선택된 본과 부모
	None                // 없음
};

