#pragma once

#include "GameFramework/Pawn.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class UBoxComponent;
class UDecalComponent;
class UAnimSequence;
class USkeleton;
class ALayerCharacter;

enum class EEnemyAttackState : uint8
{
	Idle,
	Windup
};

class AEnemyCharacter : public APawn
{
	DECLARE_CLASS(AEnemyCharacter, APawn)
	REFLECT_START(AEnemyCharacter)
		PROPERTY(SkeletalMeshPath)
		PROPERTY(IdleAnimPath)
		PROPERTY(AttackAnimPath)
		PROPERTY(MeshRelativeLocation)
		PROPERTY(MeshRelativeRotation)
		PROPERTY(MeshRelativeScale)
		PROPERTY(AttackCooldown)
		PROPERTY(AttackWindupTime)
		PROPERTY(AttackDuration)
		PROPERTY(AttackRange)
		PROPERTY(AttackWidth)
		PROPERTY(Damage)
		PROPERTY(KnockbackDistance)
		PROPERTY(bRotateTowardsPlayer)
	REFLECT_END()

public:
	AEnemyCharacter();
	virtual ~AEnemyCharacter() override = default;

	virtual void PostLoad() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void PostEditChangeProperty(const FProperty& Property) override;

protected:
	void StartAttack();
	void PerformAttackHitCheck();
	void EndAttack();
	void UpdateIndicator(bool bShow);

	ALayerCharacter* FindPlayerCharacter() const;
	UAnimSequence* ResolveAnimSequence(const FString& AnimPathOrKeyword, USkeleton* TargetSkeleton, const TArray<UAnimSequence*>& EmbeddedSequences);

protected:
	UCapsuleComponent* CapsuleComponent = nullptr;
	USkeletalMeshComponent* Mesh = nullptr;
	UBoxComponent* AttackIndicatorBox = nullptr;
	UDecalComponent* AttackIndicatorDecal = nullptr;
	UAnimSequence* IdleAnim = nullptr;
	UAnimSequence* AttackAnim = nullptr;

	// 모델 및 애니메이션 경로 설정 (Assets 폴더 기준 상대 경로 또는 내장 애니메이션 키워드)
	FString SkeletalMeshPath = "Models/Garen/source/Garen.fbx";
	FString IdleAnimPath = "idle1";
	FString AttackAnimPath = "attack_01";

	// 메시 트랜스폼 보정 프로퍼티
	FVector MeshRelativeLocation = FVector(0.0f, 0.0f, -0.9f);
	FRotator MeshRelativeRotation = FRotator(0.0f, -90.0f, 0.0f);
	FVector MeshRelativeScale = FVector(0.01f, 0.01f, 0.01f);

	// 공격 타이밍 설정
	float AttackCooldown = 3.0f;    // 대기 시간 (초)
	float AttackWindupTime = 0.6f;  // 공격 모션 시작 후 실제 타격 판정까지의 선딜레이 (초)
	float AttackDuration = 1.3f;    // 공격 모션 전체 지속 시간 (초)

	// 공격 범위 및 데미지 설정
	float AttackRange = 3.0f;       // 전방 사거리 (미터)
	float AttackWidth = 2.0f;       // 좌우 폭 (미터)
	float Damage = 20.0f;           // 공격 데미지
	float KnockbackDistance = 1.5f; // 피격 시 넉백 거리 (미터)
	bool bRotateTowardsPlayer = false; // 플레이어를 향해 회전할지 여부 (기본: 고정 방향)

private:
	EEnemyAttackState CurrentState = EEnemyAttackState::Idle;
	float StateTimer = 0.0f;
	bool bHitCheckPerformed = false;
};
