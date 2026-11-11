#pragma once

#include "GameFramework/Pawn.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UCharacterMovementComponent;
class UScriptComponent;

class ALayerCharacter : public APawn
{
	DECLARE_CLASS(ALayerCharacter, APawn)
	REFLECT_START(ALayerCharacter)
		PROPERTY(WalkSpeed)
		PROPERTY(RunSpeed)
		PROPERTY(JumpPower)
		PROPERTY(MouseSensitivity)
		PROPERTY(bEnableRootMotion)
		PROPERTY(HitKnockbackDistance)
		PROPERTY(FallResetZ)
		PROPERTY(AnimationGraphPath)
	REFLECT_END()

public:
	ALayerCharacter();
	virtual ~ALayerCharacter() override = default;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void ProcessPlayerInput(float DeltaTime) override;
	virtual void SetActorLocation(const FVector& Location);
	virtual void PostEditChangeProperty(const FProperty& Property) override;
	virtual void HandleAnimNotify(const FName& NotifyName) override;

	void ResetToLocation(const FVector& NewLocation);

	void SetEnableRootMotion(bool bEnable);
	bool IsRootMotionEnabled() const { return bEnableRootMotion; }

	void ToggleCameraPerspective();
	bool IsGrounded() const;

	void TakeDamage(float DamageAmount, const FVector& HitDirection = FVector::ZeroVector, float InKnockbackDistance = -1.0f);

	UCapsuleComponent* GetCapsuleComponent() const { return CapsuleComponent; }
	USkeletalMeshComponent* GetMesh() const { return Mesh; }
	UCharacterMovementComponent* GetCharacterMovement() const { return CharacterMovement; }

protected:
	UCapsuleComponent* CapsuleComponent = nullptr;
	USkeletalMeshComponent* Mesh = nullptr;
	USpringArmComponent* CameraBoom = nullptr;
	UCameraComponent* FollowCamera = nullptr;
	UCharacterMovementComponent* CharacterMovement = nullptr;
	UScriptComponent* ScriptComponent = nullptr;

	float WalkSpeed = 2.0f;
	float RunSpeed = 4.8f;
	float JumpPower = 6.0f;
	float MouseSensitivity = 0.1f;

	// 루트 모션 활성화 여부 (T 키 또는 디테일 패널에서 토글)
	// true: 피격 시 실제 캡슐 콜라이더가 이동하여 절벽 아래로 추락
	// false: 피격 시 캡슐 콜라이더는 제자리에 고정되고 메쉬만 허공으로 밀려났다가 복귀
	bool bEnableRootMotion = true;

	// 기본 피격 넉백 거리 (미터 단위)
	float HitKnockbackDistance = 1.5f;
	float CurrentKnockbackScale = 1.0f;

	// 낙하 리스폰 고도 임계값
	float FallResetZ = -15.0f;

	// 사용할 애니메이션 그래프 경로 (Assets 기준 상대 경로)
	FString AnimationGraphPath = "LayerAnimationGraph.animgraph";

private:
	FVector InitialSpawnLocation = FVector::ZeroVector;
	bool bHasInitialSpawnLocation = false;

	float CurrentAnimSpeed = 0.0f;
	bool bIsFirstPerson = false;
	float ThirdPersonArmLength = 4.0f;
	FVector ThirdPersonBoomLocation = FVector(-0.5f, 0.0f, 0.6f);
	FVector FirstPersonCameraOffset = FVector(0.25f, 0.0f, 0.7f);
};
