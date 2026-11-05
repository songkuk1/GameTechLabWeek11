#pragma once

#include "GameFramework/Actor.h"
#include "UObject/Class.h"
#include "CameraTypes.h"

class UCameraComponent;
class UCameraModifier;
class APlayerController;
class UCameraShakeBase;

struct FViewTarget
{
public:
	AActor* Target;

	FMinimalViewInfo POV;

public:

	FViewTarget()
		: Target(nullptr)
	{
	}

	void SetNewTarget(AActor* NewTarget);

	class APawn* GetTargetPawn() const;
};

class APlayerCameraManager : public AActor
{
	DECLARE_CLASS(APlayerCameraManager, AActor)

public:
	APlayerCameraManager();
	virtual ~APlayerCameraManager();

	void BeginPlay() override;

	//업데이트 카메라
	void UpdateCamera(float DeltaTime);

	void UpdateViewTarget(FViewTarget& InViewTarget, float DeltaTime);

	// ModifierClass로 Modifier를 만들어 Priority 순서에 맞게 넣는다. PCM이 소유한다.
	// 실패(잘못된 클래스, Exclusive 충돌)하면 nullptr.
	UCameraModifier* AddNewCameraModifier(UClass* ModifierClass);

	template<typename T>
	T* AddNewCameraModifier()
	{
		return static_cast<T*>(AddNewCameraModifier(T::StaticClass()));
	}

	UCameraModifier* FindCameraModifierByClass(UClass* ModifierClass) const;

	template<typename T>
	T* FindCameraModifierByClass() const
	{
		return static_cast<T*>(FindCameraModifierByClass(T::StaticClass()));
	}

	// 바로 지우지 않고 이번 프레임 Modifier 적용이 끝난 뒤 지운다.
	void RemoveCameraModifier(UCameraModifier* Modifier);

	// Modifier 소멸자가 부른다. delete하지 않고 목록에서만 뺀다
	void RemoveModifierFromList(UCameraModifier* Modifier);

	void ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& POV);

	// Shake Modifier에 Shake를 하나 추가해서 시작한다. 실패하면 nullptr
	UCameraShakeBase* StartCameraShake(UClass* ShakeClass, float Scale = 1.f);

	template<typename T>
	T* StartCameraShake(float Scale = 1.f)
	{
		return static_cast<T*>(StartCameraShake(T::StaticClass(), Scale));
	}

	void StopAllCameraShakes(bool bImmediate = false);



	//Fade 효과 적용
	void UpdateFade(float DeltaTime);

	void UpdateLetterBox(float DeltaTime);

	const FMinimalViewInfo& GetViewTargetPOV() const;
	const FMinimalViewInfo& GetCameraCachePOV() const;

	void SetViewTarget(AActor* NewTarget);
	AActor* GetViewTarget() const;

	void InitializeFor(APlayerController* PC);

	//bEnableFading를 true로 바꾸고 Fade값들을 설정합니다.
	//[Fade In] From : 1.0, To : 0.0
	//[Fade Out] From : 0.0, To : 1.0
	//bInHoldWhenFinished : Fade가 끝나고 유지할지 입니다. Fade Out의 경우 true로 해야 유지됩니다.
	void StartCameraFade(float From, float To, float Duration, FLinearColor Color, bool bInHoldWhenFinished = false);

	void StopCameraFade();

	void SetManualCameraFade(float Amount, FLinearColor Color);

	//계산된 카메라 효과 값
	FCameraScreenEffects GetScreenEffects()const;

	// 비네트. 게임과 PIE에서 이 값이 화면에 적용된다 (에디터 View는 에디터 설정 값)
	void SetVignette(const FVignetteSettings& InVignette) { Vignette = InVignette; }
	const FVignetteSettings& GetVignette() const { return Vignette; }

private:
	// 정렬 위치에 넣는다. Exclusive 충돌이면 false.
	bool AddCameraModifierToList(UCameraModifier* NewModifier);
	// 제거 예약된 Modifier를 실제로 delete
	void FlushRemovedModifiers();
	// 모든 Modifier delete (PCM 소멸 시)
	void ClearCameraModifiers();

private:
	APlayerController* OwnerPC = nullptr;
	FViewTarget ViewTarget;
	FViewTarget PendingViewTarget;
	FMinimalViewInfo CameraCachePOV;

	//Priority순으로 정렬하여 보관
	TArray<UCameraModifier*> ModifierList;

	//PCM이 생성될 때 자동으로 생성할 Modifier들
	TArray<UClass*> DefaultModifiers;

	//지울 거 저장
	TArray<UCameraModifier*> ModifiersToRemove;

	//전환 시간
	float BlendTimeToGo = 0.f;

	//Fade In and out
	FLinearColor FadeColor = FLinearColor::Black;

	//X는 시작 알파, Y는 종료 알파.
	// SceneColor와 FadeColor를 Alpha로 섞는다.
	//Fade Out이면(0, 1) Fade In이면(1, 0)이다
	FVector2 FadeAlpha = FVector2();

	//실제 Fade 계산량
	float FadeAmount = 0.f;

	float FadeTime = 0.f;
	float FadeTimeRemaining = 0.f;
	bool bEnableFading = true;
	bool bUpdateFading = true;
	bool bHoldWhenFinished = false;

	//비네트
	FVignetteSettings Vignette;
};
