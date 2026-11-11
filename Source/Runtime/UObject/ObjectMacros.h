#pragma once

#include "Core/Types.h"

enum class EObjectFlags : uint32
{
	RF_NoFlags = 0x00000000, // 플래그 없음
	RF_Transactional = 0x00000008, // Undo Redo 관리 대상
	RF_Transient = 0x00000040, // 직렬화 제외 플래그
	RF_DefaultSubObject = 0x00040000,
	RF_AllFlags = 0xffffffff, // 모든 플래그
};

DEFINE_ENUM_OPERATORS(EObjectFlags)

// 리플렉션 표시 매크로. 컴파일러에게는 빈 매크로이고, HitoriHeaderTool이 헤더를 읽고 해석한다.
#define USTRUCT(...)
#define UCLASS(...)
#define UPROPERTY(...)
#define UFUNCTION(...)
#define UENUM(...)
#define UMETA(...)

// GENERATED_BODY()는 "파일 ID_줄 번호_GENERATED_BODY" 매크로로 바뀐다.
// 그 매크로는 HitoriHeaderTool이 만든 X.generated.h에 들어 있고, 파일 ID(CURRENT_FILE_ID)도 거기서 정의한다.
#define BODY_MACRO_COMBINE_INNER(A,B,C,D) A##B##C##D
#define BODY_MACRO_COMBINE(A,B,C,D) BODY_MACRO_COMBINE_INNER(A,B,C,D)
#define GENERATED_BODY(...) BODY_MACRO_COMBINE(CURRENT_FILE_ID, _, __LINE__, _GENERATED_BODY)
