#pragma once

// 에디터 패널 간 드래그앤드롭 페이로드 식별자.
// ImGui는 이 문자열로 소스와 타깃을 짝지으므로 양쪽이 같은 값을 써야 한다.
// (ImGui 제한: 널 포함 32자 이내)
namespace EditorDragDrop
{
	// Payload: Actor UUID (uint32).
	inline constexpr const char* Actor = "ACTOR_UUID";

	// 페이로드 내용: UTexture2D* 한 개
	inline constexpr const char* Texture = "ASSET_TEXTURE";

	// 페이로드 내용: UFont* 한 개
	inline constexpr const char* Font = "ASSET_FONT";

	// Payload: null-terminated sequence file path.
	inline constexpr const char* Sequence = "ASSET_SEQUENCE_PATH";

	inline constexpr const char* Component = "COMPONENT";
}
