#include "EnginePCH.h"
#include "GameFramework/Actor/LayerCharacter.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Input/InputSystem.h"

#include "Component/CharacterMovementComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Component/CapsuleComponent.h"
#include "Component/ScriptComponent.h"

#include "Asset/AssetManager.h"
#include "Animation/AnimationGraph.h"
#include "Animation/AnimInstance.h"
#include "Core/EngineLog.h"

#include <cmath>
#include <algorithm>

ALayerCharacter::ALayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// 1. 루트 콜리전 캡슐 생성 (시연 시 콜라이더 위치 확인을 위해 게임 내 디버그 라인 표시 활성화)
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>("CapsuleComponent");
	SetRootComponent(CapsuleComponent);
	CapsuleComponent->SetCapsuleRadius(0.5f);
	CapsuleComponent->SetCapsuleHalfHeight(1.0f);
	CapsuleComponent->SetBlockComponent(true);
	CapsuleComponent->SetDrawOnlyIfSelected(false);
	CapsuleComponent->SetGameHidden(false);
	CapsuleComponent->SetShapeColor(FColor(255, 80, 0, 255));

	// 2. 스켈레탈 메시 부착 (캡슐 기준 바닥 정렬 및 -90도 Yaw 회전)
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>("Mesh");
	Mesh->SetupAttachment(CapsuleComponent);
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, -1.0f));
	Mesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

	USkeletalMesh* SkelMesh = UAssetManager::Get().GetAssetByPath<USkeletalMesh>("Models/Mannequin/UnrealCharac/SKM_Manny_Simple.FBX");
	if (!SkelMesh)
	{
		SkelMesh = UAssetManager::Get().GetAssetByPath<USkeletalMesh>("Models/Mannequin/UAL2_Standard.fbx");
	}
	if (SkelMesh)
	{
		Mesh->SetSkeletalMesh(SkelMesh);
	}

	// 3. 스프링암 부착
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>("CameraBoom");
	CameraBoom->SetupAttachment(CapsuleComponent);
	CameraBoom->SetRelativeLocation(ThirdPersonBoomLocation);

	// 4. 카메라 부착
	FollowCamera = CreateDefaultSubobject<UCameraComponent>("FollowCamera");
	FollowCamera->SetupAttachment(CameraBoom);

	// 5. 캐릭터 무브먼트 연결
	CharacterMovement = CreateDefaultSubobject<UCharacterMovementComponent>("CharacterMovement");
	CharacterMovement->SetUpdatedComponent(CapsuleComponent);
	CharacterMovement->SetCollisionComponent(CapsuleComponent);
	CharacterMovement->SetOrientRotationToMovement(true);

	// 6. 루아 스크립트 컴포넌트 부착 (낙하 리셋 스크립트)
	ScriptComponent = CreateDefaultSubobject<UScriptComponent>("ScriptComponent");
	ScriptComponent->SetScriptPath("Assets/Scripts/Actors/CharacterFallReset.lua");

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	bAutoPossessPlayer = true;
}

void ALayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	InitialSpawnLocation = GetActorLocation();
	bHasInitialSpawnLocation = true;

	if (Mesh)
	{
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, -1.0f));
		Mesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

		if (!Mesh->GetSkeletalMesh())
		{
			USkeletalMesh* SkelMesh = UAssetManager::Get().GetAssetByPath<USkeletalMesh>("Models/Mannequin/UnrealCharac/SKM_Manny_Simple.FBX");
			if (SkelMesh)
			{
				Mesh->SetSkeletalMesh(SkelMesh);
			}
		}

		// LayerAnimationGraph 로드 및 바인딩
		FString GraphKey = AnimationGraphPath;
		if (GraphKey.empty())
		{
			GraphKey = "LayerAnimationGraph.animgraph";
		}

		UAnimationGraph* Graph = UAssetManager::GetAssetByPath<UAnimationGraph>(GraphKey);
		if (!Graph)
		{
			const FString FullPath = (GraphKey.find("Assets/") == 0 || GraphKey.find("Assets\\") == 0)
				? GraphKey
				: ("Assets/" + GraphKey);
			Graph = UAssetManager::LoadAnimationGraph(FullPath);
		}

		if (Graph && Mesh->SetAnimationGraph(Graph))
		{
			if (UAnimInstance* AnimInst = Mesh->GetAnimInstance())
			{
				AnimInst->SetExternalStateControl(false);
				AnimInst->SetFloatParameter("Speed", 0.0f);
				AnimInst->SetBoolParameter("bIsGrounded", true);
				AnimInst->SetBoolParameter("bIsJumping", false);
				AnimInst->SetBoolParameter("bIsAttacking", false);
				AnimInst->SetBoolParameter("bIsHit", false);
			}
			HTR_LOG(Info, "[LayerCharacter] AnimationGraph '{}' initialized successfully.", GraphKey);
		}
		else
		{
			HTR_LOG(Warning, "[LayerCharacter] Failed to initialize AnimationGraph '{}'.", GraphKey);
		}
	}

	if (CharacterMovement)
	{
		CharacterMovement->SetOrientRotationToMovement(true);
	}
	bUseControllerRotationYaw = false;

	CurrentKnockbackScale = (HitKnockbackDistance > 0.0f) ? (HitKnockbackDistance / 1.261f) : 1.0f;
	SetEnableRootMotion(bEnableRootMotion);
}

void ALayerCharacter::SetActorLocation(const FVector& Location)
{
	Super::SetActorLocation(Location);
	if (CharacterMovement)
	{
		CharacterMovement->ResetVelocity();
	}
}

void ALayerCharacter::ResetToLocation(const FVector& NewLocation)
{
	SetActorLocation(NewLocation);
	if (CharacterMovement)
	{
		CharacterMovement->ResetVelocity();
	}
	CurrentAnimSpeed = 0.0f;

	if (Mesh)
	{
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, -1.0f));
		if (UAnimInstance* AnimInst = Mesh->GetAnimInstance())
		{
			AnimInst->SetFloatParameter("Speed", 0.0f);
			AnimInst->SetBoolParameter("bIsJumping", false);
			AnimInst->SetBoolParameter("bIsAttacking", false);
			AnimInst->SetBoolParameter("bIsHit", false);
			AnimInst->SetBoolParameter("bIsGrounded", true);
			AnimInst->RequestState("Locomotion");
		}
	}
}

void ALayerCharacter::SetEnableRootMotion(bool bEnable)
{
	bEnableRootMotion = bEnable;

	if (Mesh)
	{
		// AnimGraph 내에서는 Hit 상태 노드(bAllowRootMotion == true)에만 루트모션이 적용됨
		Mesh->SetRootMotionEnabled(bEnableRootMotion);
		Mesh->SetRootMotionScale(CurrentKnockbackScale);
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, -1.0f));
	}

	if (CapsuleComponent)
	{
		const FRotator CurRot = GetActorRotation();
		if ( std::abs(CurRot.Pitch) > 0.001f || std::abs(CurRot.Roll) > 0.001f )
		{
			SetActorRotation(FRotator(0.0f, CurRot.Yaw, 0.0f));
		}
		CapsuleComponent->SetDrawOnlyIfSelected(false);
		CapsuleComponent->SetGameHidden(false);
		// 루트모션 ON: 주황/빨강 캡슐, 루트모션 OFF: 초록 캡슐로 시각적 구분
		CapsuleComponent->SetShapeColor(bEnableRootMotion ? FColor(255, 80, 0, 255) : FColor(0, 230, 90, 255));
	}

	HTR_LOG(Info, "[LayerCharacter] RootMotion Mode: {}",
		bEnableRootMotion
		? "ON (루트 모션 활성화 - 피격 시 캡슐 콜라이더가 함께 이동하여 낙하)"
		: "OFF (루트 모션 비활성화 - 피격 시 캡슐 콜라이더는 고정, 메쉬만 허공으로 이동)");
}

void ALayerCharacter::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
	CurrentKnockbackScale = (HitKnockbackDistance > 0.0f) ? (HitKnockbackDistance / 1.261f) : 1.0f;
	SetEnableRootMotion(bEnableRootMotion);
}

void ALayerCharacter::ToggleCameraPerspective()
{
	bIsFirstPerson = !bIsFirstPerson;

	if (CameraBoom)
	{
		if (bIsFirstPerson)
		{
			CameraBoom->SetTargetArmLength(0.0f);
			CameraBoom->SetRelativeLocation(FirstPersonCameraOffset);
			CameraBoom->SetDoCollisionTest(false);
			if (Mesh)
			{
				Mesh->SetVisible(false);
			}
			HTR_LOG(Info, "[LayerCharacter] Switched to 1st Person view (V key)");
		}
		else
		{
			CameraBoom->SetTargetArmLength(ThirdPersonArmLength);
			CameraBoom->SetRelativeLocation(ThirdPersonBoomLocation);
			CameraBoom->SetDoCollisionTest(true);
			if (Mesh)
			{
				Mesh->SetVisible(true);
			}
			HTR_LOG(Info, "[LayerCharacter] Switched to 3rd Person view (V key)");
		}
	}
}

bool ALayerCharacter::IsGrounded() const
{
	return CharacterMovement ? CharacterMovement->IsGrounded() : true;
}

void ALayerCharacter::ProcessPlayerInput(float DeltaTime)
{
	Super::ProcessPlayerInput(DeltaTime);
	if (!Controller || !Controller->IsInputEnabled())
	{
		if (CharacterMovement)
		{
			CharacterMovement->SetMoveInput(FVector::ZeroVector);
		}
		return;
	}

	UAnimInstance* AnimInst = Mesh ? Mesh->GetAnimInstance() : nullptr;

	// T 키: 루트 모션 ON / OFF 토글
	if (FInputSystem::IsKeyPressed(EKeyCode::T))
	{
		SetEnableRootMotion(!bEnableRootMotion);
	}

	// V 키: 1인칭 / 3인칭 시점 전환
	if (FInputSystem::IsKeyPressed(EKeyCode::V))
	{
		ToggleCameraPerspective();
	}

	// H 키: 수동 피격(Hit) 테스트 트리거
	if (FInputSystem::IsKeyPressed(EKeyCode::H))
	{
		const FVector BackwardDir = -GetActorRotation().Quaternion().GetForwardVector();
		TakeDamage(10.0f, BackwardDir, HitKnockbackDistance);
	}

	// 마우스 우클릭 드래그: 카메라 회전
	if (FInputSystem::IsMouseDown(EMouseButton::Right))
	{
		const float MouseDeltaX = static_cast<float>(FInputSystem::GetMouseDeltaX());
		const float MouseDeltaY = static_cast<float>(FInputSystem::GetMouseDeltaY());

		AddControllerYawInput(MouseDeltaX * MouseSensitivity);
		AddControllerPitchInput(MouseDeltaY * MouseSensitivity);
	}

	const FName CurrentState = AnimInst ? AnimInst->GetCurrentStateName() : NAME_None;
	const bool bInHitState = (CurrentState == "Hit") || (AnimInst && AnimInst->GetBoolParameter("bIsHit"));

	if (bInHitState)
	{
		if (CharacterMovement)
		{
			CharacterMovement->SetMoveInput(FVector::ZeroVector);
		}
		return;
	}

	// 마우스 좌클릭: 상/하체 분리(LayeredBoneBlend) 공격 트리거
	if (FInputSystem::IsMousePressed(EMouseButton::Left))
	{
		if (AnimInst && CurrentState == "Locomotion")
		{
			AnimInst->SetBoolParameter("bIsAttacking", true);
		}
	}

	// 카메라의 수평(Yaw) 시선 방향을 기준으로 WASD 이동 방향 계산
	const FRotator ControlRot = Controller->GetControlRotation();
	const FQuat YawQuat = FRotator(0.0f, ControlRot.Yaw, 0.0f).Quaternion();
	const FVector Forward = YawQuat.GetForwardVector();
	const FVector Right = YawQuat.GetRightVector();

	FVector MoveDir = FVector::ZeroVector;
	if (FInputSystem::IsKeyDown(EKeyCode::W)) MoveDir += Forward;
	if (FInputSystem::IsKeyDown(EKeyCode::S)) MoveDir -= Forward;
	if (FInputSystem::IsKeyDown(EKeyCode::A)) MoveDir -= Right;
	if (FInputSystem::IsKeyDown(EKeyCode::D)) MoveDir += Right;

	// Shift 또는 R 키를 누르고 있으면 달리기(RunSpeed), 아니면 걷기(WalkSpeed)
	const bool bSprinting = FInputSystem::IsKeyDown(EKeyCode::Shift) ||
		FInputSystem::IsKeyDown(EKeyCode::LShift) ||
		FInputSystem::IsKeyDown(EKeyCode::R);
	const float DesiredSpeed = bSprinting ? RunSpeed : WalkSpeed;

	if (CharacterMovement)
	{
		CharacterMovement->SetMoveSpeed(DesiredSpeed);
		CharacterMovement->SetMoveInput(MoveDir);

		// Space 키: 3단 점프 시작 (Jump_Start -> Jump_Loop -> Jump_Land)
		if (FInputSystem::IsKeyPressed(EKeyCode::Space) && CharacterMovement->IsGrounded())
		{
			CharacterMovement->Jump(JumpPower);
			if (AnimInst)
			{
				AnimInst->SetBoolParameter("bIsJumping", true);
				AnimInst->SetBoolParameter("bIsGrounded", false);
			}
		}
	}
}

void ALayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bHasInitialSpawnLocation)
	{
		InitialSpawnLocation = GetActorLocation();
		bHasInitialSpawnLocation = true;
	}

	// 절벽 아래로 추락 시 스폰 위치로 자동 리셋
	if (GetActorLocation().Z <= FallResetZ)
	{
		HTR_LOG(Info, "[LayerCharacter] Fell below Z={:.1f}! Respawning at ({:.2f}, {:.2f}, {:.2f}).",
			FallResetZ, InitialSpawnLocation.X, InitialSpawnLocation.Y, InitialSpawnLocation.Z);
		ResetToLocation(InitialSpawnLocation);
		return;
	}

	UAnimInstance* AnimInst = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!AnimInst || !CharacterMovement)
	{
		return;
	}

	const FName CurrentState = AnimInst->GetCurrentStateName();

	if (Mesh)
	{
		// AnimGraph 내에서는 Hit 상태 노드(bAllowRootMotion == true)에만 루트모션이 적용됨
		Mesh->SetRootMotionEnabled(bEnableRootMotion);
		Mesh->SetRootMotionScale(CurrentKnockbackScale);
	}

	// 상태 진입 확인 후 트리거성 Bool 파라미터 자동 해제 (One-shot 트리거)
	if (CurrentState == "Hit")
	{
		AnimInst->SetBoolParameter("bIsHit", false);
		AnimInst->SetBoolParameter("bIsAttacking", false);
		AnimInst->SetBoolParameter("bIsJumping", false);
		CharacterMovement->SetMoveInput(FVector::ZeroVector);
		return;
	}

	if (CurrentState == "Attack")
	{
		AnimInst->SetBoolParameter("bIsAttacking", false);
	}

	if (CurrentState == "Jump_Start" || CurrentState == "Jump_Loop" || CurrentState == "Jump_Land")
	{
		AnimInst->SetBoolParameter("bIsJumping", false);
	}

	// 착지 판정 업데이트 (상승 중이 아닐 때 바닥 접촉 시 bIsGrounded = true)
	const bool bGrounded = CharacterMovement->IsGrounded() && (CharacterMovement->Velocity.Z <= 0.05f);
	AnimInst->SetBoolParameter("bIsGrounded", bGrounded);

	// BlendSpace1D(NewBlendSpace1D_1.blendspace)의 Speed 축 매핑:
	// 0.0 = Idle, 150.0 = Walk(WalkSpeed), 450.0 = Jog(RunSpeed)
	const FVector Vel = CharacterMovement->Velocity;
	const float HorizSpeed = FVector(Vel.X, Vel.Y, 0.0f).Size();

	float TargetAnimSpeed = 0.0f;
	if (HorizSpeed > 0.05f)
	{
		const float SafeWalkSpeed = std::max(WalkSpeed, 0.1f);
		const float SafeRunSpeed = std::max(RunSpeed, SafeWalkSpeed + 0.1f);

		if (HorizSpeed <= SafeWalkSpeed * 1.05f)
		{
			TargetAnimSpeed = (HorizSpeed / SafeWalkSpeed) * 150.0f;
		}
		else
		{
			const float RunAlpha = std::clamp((HorizSpeed - SafeWalkSpeed) / (SafeRunSpeed - SafeWalkSpeed), 0.0f, 1.0f);
			TargetAnimSpeed = 150.0f + RunAlpha * 300.0f;
		}
	}

	CurrentAnimSpeed = FMath::FInterpTo(CurrentAnimSpeed, TargetAnimSpeed, DeltaTime, 25.0f);
	if (TargetAnimSpeed <= 0.01f && CurrentAnimSpeed < 8.0f)
	{
		CurrentAnimSpeed = 0.0f;
	}

	AnimInst->SetFloatParameter("Speed", CurrentAnimSpeed);
}

void ALayerCharacter::TakeDamage(float DamageAmount, const FVector& HitDirection, float InKnockbackDistance)
{
	const float TargetDist = (InKnockbackDistance >= 0.0f) ? InKnockbackDistance : HitKnockbackDistance;
	CurrentKnockbackScale = (TargetDist > 0.0f) ? (TargetDist / 1.261f) : 1.0f;

	// 공격자가 밀어내는 방향(HitDirection)으로 캐릭터의 등이 향하도록(-HitDirection을 바라보도록) 정렬
	// 이렇게 하면 정면 피격 애니메이션(MM_Death_Front_01)의 뒤로 물러나는 루트 모션이 정확히 HitDirection으로 발생함
	FVector FaceDir = -HitDirection;
	FaceDir.Z = 0.0f;
	if (FaceDir.SizeSquared() > 0.001f)
	{
		const float FaceYaw = std::atan2(FaceDir.Y, FaceDir.X) * 180.0f / PI;
		SetActorRotation(FRotator(0.0f, FaceYaw, 0.0f));
	}

	if (CharacterMovement)
	{
		CharacterMovement->ResetVelocity();
	}

	if (Mesh)
	{
		Mesh->SetRootMotionEnabled(bEnableRootMotion);
		Mesh->SetRootMotionScale(CurrentKnockbackScale);

		if (UAnimInstance* AnimInst = Mesh->GetAnimInstance())
		{
			AnimInst->SetBoolParameter("bIsAttacking", false);
			AnimInst->SetBoolParameter("bIsJumping", false);
			AnimInst->SetBoolParameter("bIsHit", true);
		}
	}

	HTR_LOG(Info, "[LayerCharacter] Took {:.1f} damage! Knockback={:.2f}m (Scale={:.2f}), RootMotion={}",
		DamageAmount, TargetDist, CurrentKnockbackScale, bEnableRootMotion ? "ON (Collider Moves)" : "OFF (Mesh Only)");
}

void ALayerCharacter::HandleAnimNotify(const FName& NotifyName)
{
	Super::HandleAnimNotify(NotifyName);
	if (!NotifyName.IsNone())
	{
		HTR_LOG(Info, "[LayerCharacter] AnimNotify triggered: '{}'", NotifyName.ToString());
	}
}
