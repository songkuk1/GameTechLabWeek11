#include "EnginePCH.h"
#include "GameFramework/Actor/EnemyCharacter.h"

#include "Engine/World.h"
#include "Engine/Level.h"
#include "Asset/AssetManager.h"
#include "Component/CapsuleComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Component/BoxComponent.h"
#include "Component/DecalComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "GameFramework/Actor/LayerCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Core/EngineLog.h"

#include <algorithm>
#include <cctype>
#include <cmath>

AEnemyCharacter::AEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// 1. 루트 충돌 캡슐
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>("CapsuleComponent");
	SetRootComponent(CapsuleComponent);
	CapsuleComponent->SetCapsuleRadius(0.4f);
	CapsuleComponent->SetCapsuleHalfHeight(0.9f);
	CapsuleComponent->SetBlockComponent(true);

	// 2. 스켈레탈 메시 부착 (캡슐 바닥에 맞춤, Yaw -90도 정렬)
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>("Mesh");
	Mesh->SetupAttachment(CapsuleComponent);
	Mesh->SetRelativeLocation(MeshRelativeLocation);
	Mesh->SetRelativeRotation(MeshRelativeRotation);
	Mesh->SetRelativeScale3D(MeshRelativeScale);

	USkeletalMesh* SkelMesh = UAssetManager::GetAssetByPath<USkeletalMesh>(SkeletalMeshPath);
	if (!SkelMesh && !SkeletalMeshPath.empty())
	{
		const FString FullFbxPath = (SkeletalMeshPath.find("Assets/") == 0 || SkeletalMeshPath.find("Assets\\") == 0)
			? SkeletalMeshPath
			: ("Assets/" + SkeletalMeshPath);
		UAssetManager::Get().LoadFbxModel(FullFbxPath, true, false, false);
		SkelMesh = UAssetManager::GetAssetByPath<USkeletalMesh>(SkeletalMeshPath);
	}
	if (SkelMesh)
	{
		Mesh->SetSkeletalMesh(SkelMesh);
	}

	// 3. 공격 예고 바닥 라인 박스 (선딜레이 동안 바닥에 빨간색 장판 테두리 표시)
	AttackIndicatorBox = CreateDefaultSubobject<UBoxComponent>("AttackIndicatorBox");
	AttackIndicatorBox->SetupAttachment(CapsuleComponent);
	AttackIndicatorBox->SetRelativeLocation(FVector(AttackRange * 0.5f, 0.0f, -0.89f));
	AttackIndicatorBox->SetBoxExtent(FVector(AttackRange * 0.5f, AttackWidth * 0.5f, 0.03f));
	AttackIndicatorBox->SetShapeColor(FColor(255, 30, 30, 255));
	AttackIndicatorBox->SetDrawOnlyIfSelected(true);
	AttackIndicatorBox->SetGameHidden(false);
	AttackIndicatorBox->SetBlockComponent(false);

	// 4. 공격 예고 바닥 데칼 (바닥에 붉은색 반투명 장판 투영)
	AttackIndicatorDecal = CreateDefaultSubobject<UDecalComponent>("AttackIndicatorDecal");
	AttackIndicatorDecal->SetupAttachment(CapsuleComponent);
	AttackIndicatorDecal->SetRelativeLocation(FVector(AttackRange * 0.5f, 0.0f, 0.0f));
	AttackIndicatorDecal->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	AttackIndicatorDecal->SetDecalSize(FVector(0.001f, 0.001f, 0.001f));
	AttackIndicatorDecal->SetDecalColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
}

void AEnemyCharacter::PostLoad()
{
	Super::PostLoad();

	if (SkeletalMeshPath == "Models/Garen/Garen.fbx")
	{
		SkeletalMeshPath = "Models/Garen/source/Garen.fbx";
	}

	if (Mesh && !SkeletalMeshPath.empty())
	{
		USkeletalMesh* SkelMesh = UAssetManager::GetAssetByPath<USkeletalMesh>(SkeletalMeshPath);
		if (!SkelMesh)
		{
			const FString FullFbxPath = (SkeletalMeshPath.find("Assets/") == 0 || SkeletalMeshPath.find("Assets\\") == 0)
				? SkeletalMeshPath
				: ("Assets/" + SkeletalMeshPath);
			UAssetManager::Get().LoadFbxModel(FullFbxPath, true, false, false);
			SkelMesh = UAssetManager::GetAssetByPath<USkeletalMesh>(SkeletalMeshPath);
		}
		if (SkelMesh && Mesh->GetSkeletalMesh() != SkelMesh)
		{
			Mesh->SetSkeletalMesh(SkelMesh);
		}
	}
}

void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (SkeletalMeshPath == "Models/Garen/Garen.fbx")
	{
		SkeletalMeshPath = "Models/Garen/source/Garen.fbx";
	}

	// 내장 애니메이션 및 메시가 동일한 스켈레톤 인스턴스를 공유하도록 먼저 로드 보장
	TArray<UAnimSequence*> EmbeddedAnims = UAssetManager::Get().GetAnimSequencesForMesh(SkeletalMeshPath);
	USkeletalMesh* SkelMesh = UAssetManager::GetAssetByPath<USkeletalMesh>(SkeletalMeshPath);

	if ((!SkelMesh || EmbeddedAnims.IsEmpty()) && !SkeletalMeshPath.empty())
	{
		const FString FullFbxPath = (SkeletalMeshPath.find("Assets/") == 0 || SkeletalMeshPath.find("Assets\\") == 0)
			? SkeletalMeshPath
			: ("Assets/" + SkeletalMeshPath);
		UAssetManager::Get().LoadFbxModel(FullFbxPath, true, false, EmbeddedAnims.IsEmpty());
		SkelMesh = UAssetManager::GetAssetByPath<USkeletalMesh>(SkeletalMeshPath);
		EmbeddedAnims = UAssetManager::Get().GetAnimSequencesForMesh(SkeletalMeshPath);
	}

	if (!SkelMesh)
	{
		SkelMesh = UAssetManager::GetAssetByPath<USkeletalMesh>("Models/Mannequin/UnrealCharac/SKM_Manny_Simple.FBX");
	}
	if (SkelMesh && Mesh)
	{
		Mesh->SetSkeletalMesh(SkelMesh);
	}

	USkeleton* MeshSkeleton = (Mesh && Mesh->GetSkeletalMesh()) ? Mesh->GetSkeletalMesh()->GetSkeleton() : nullptr;

	if (CapsuleComponent && CapsuleComponent->GetRelativeScale3D().X < 0.05f)
	{
		CapsuleComponent->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
	}

	if (MeshRelativeLocation == FVector::ZeroVector)
	{
		MeshRelativeLocation = FVector(0.0f, 0.0f, -0.9f);
	}
	if (MeshRelativeRotation.Pitch == 0.0f && MeshRelativeRotation.Yaw == 0.0f && MeshRelativeRotation.Roll == 0.0f)
	{
		MeshRelativeRotation = FRotator(0.0f, -90.0f, 0.0f);
	}
	if (MeshRelativeScale.X <= 0.0001f)
	{
		MeshRelativeScale = FVector(0.01f, 0.01f, 0.01f);
	}

	if (Mesh)
	{
		Mesh->SetRelativeLocation(MeshRelativeLocation);
		Mesh->SetRelativeRotation(MeshRelativeRotation);
		Mesh->SetRelativeScale3D(MeshRelativeScale);
	}

	IdleAnim = ResolveAnimSequence(IdleAnimPath, MeshSkeleton, EmbeddedAnims);
	AttackAnim = ResolveAnimSequence(AttackAnimPath, MeshSkeleton, EmbeddedAnims);

	if (!IdleAnim && !EmbeddedAnims.IsEmpty())
	{
		IdleAnim = ResolveAnimSequence("idle1", MeshSkeleton, EmbeddedAnims);
		if (!IdleAnim) IdleAnim = ResolveAnimSequence("idle", MeshSkeleton, EmbeddedAnims);
		if (!IdleAnim) IdleAnim = EmbeddedAnims[0];
	}
	if (!AttackAnim && !EmbeddedAnims.IsEmpty())
	{
		AttackAnim = ResolveAnimSequence("attack_01", MeshSkeleton, EmbeddedAnims);
		if (!AttackAnim) AttackAnim = ResolveAnimSequence("attack", MeshSkeleton, EmbeddedAnims);
		if (!AttackAnim) AttackAnim = ResolveAnimSequence("spell1", MeshSkeleton, EmbeddedAnims);
		if (!AttackAnim && EmbeddedAnims.Num() > 1) AttackAnim = EmbeddedAnims[1];
	}

	if (MeshSkeleton)
	{
		if (IdleAnim && IdleAnim->GetSkeleton() != MeshSkeleton)
		{
			IdleAnim->RetargetToSkeleton(MeshSkeleton);
		}
		if (AttackAnim && AttackAnim->GetSkeleton() != MeshSkeleton)
		{
			AttackAnim->RetargetToSkeleton(MeshSkeleton);
		}
	}

	if (AttackAnim && AttackAnim->GetPlayLength() > 0.5f)
	{
		AttackDuration = AttackAnim->GetPlayLength();
	}

	if (Mesh && IdleAnim)
	{
		Mesh->PlayAnimation(IdleAnim);
		Mesh->SetAnimationLooping(true);
		Mesh->SetAnimationPlayRate(1.0f);
	}

	CurrentState = EEnemyAttackState::Idle;
	StateTimer = 0.0f;
	bHitCheckPerformed = false;
	UpdateIndicator(false);

	HTR_LOG(Info, "[EnemyCharacter] Spawned at ({:.1f}, {:.1f}, {:.1f}) | Mesh={} | IdleAnim={} | AttackAnim={}",
		GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z,
		SkelMesh ? SkelMesh->GetPath() : "None",
		IdleAnim ? IdleAnim->GetPath() : "None",
		AttackAnim ? AttackAnim->GetPath() : "None");
}

void AEnemyCharacter::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);

	if (Mesh)
	{
		Mesh->SetRelativeLocation(MeshRelativeLocation);
		Mesh->SetRelativeRotation(MeshRelativeRotation);
		Mesh->SetRelativeScale3D(MeshRelativeScale);
	}
}

UAnimSequence* AEnemyCharacter::ResolveAnimSequence(const FString& AnimPathOrKeyword, USkeleton* TargetSkeleton, const TArray<UAnimSequence*>& EmbeddedSequences)
{
	if (AnimPathOrKeyword.empty())
	{
		return nullptr;
	}

	if (UAnimSequence* Found = UAssetManager::GetAssetByPath<UAnimSequence>(AnimPathOrKeyword))
	{
		return Found;
	}

	FString LowerKey = AnimPathOrKeyword;
	std::transform(LowerKey.begin(), LowerKey.end(), LowerKey.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	for (UAnimSequence* Anim : EmbeddedSequences)
	{
		if (!Anim) continue;
		FString LowerName = !Anim->GetPath().empty() ? Anim->GetPath() : Anim->GetName();
		std::transform(LowerName.begin(), LowerName.end(), LowerName.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		if (LowerName.ends_with("_" + LowerKey + ".anm") ||
			LowerName.ends_with("/" + LowerKey + ".anm") ||
			LowerName.ends_with("|" + LowerKey + ".anm") ||
			LowerName.ends_with(":" + LowerKey + ".anm") ||
			LowerName.ends_with("_" + LowerKey) ||
			LowerName.ends_with(":" + LowerKey) ||
			LowerName == LowerKey)
		{
			return Anim;
		}
	}

	auto ContainsCaseInsensitive = [](const FString& FullStr, const FString& SubStr) -> bool
	{
		if (SubStr.empty()) return false;
		auto it = std::search(
			FullStr.begin(), FullStr.end(),
			SubStr.begin(), SubStr.end(),
			[](char ch1, char ch2) { return std::tolower(static_cast<unsigned char>(ch1)) == std::tolower(static_cast<unsigned char>(ch2)); }
		);
		return (it != FullStr.end());
	};

	for (UAnimSequence* Anim : EmbeddedSequences)
	{
		if (!Anim) continue;
		const FString CandidateName = !Anim->GetPath().empty() ? Anim->GetPath() : Anim->GetName();
		if (ContainsCaseInsensitive(CandidateName, AnimPathOrKeyword))
		{
			return Anim;
		}
	}

	return nullptr;
}

void AEnemyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bRotateTowardsPlayer)
	{
		if (ALayerCharacter* Player = FindPlayerCharacter())
		{
			FVector Dir = Player->GetActorLocation() - GetActorLocation();
			Dir.Z = 0.0f;
			if (Dir.SizeSquared() > 0.001f)
			{
				const float TargetYaw = std::atan2(Dir.Y, Dir.X) * 180.0f / PI;
				FRotator CurrentRot = GetActorRotation();
				CurrentRot.Yaw = TargetYaw;
				SetActorRotation(CurrentRot);
			}
		}
	}

	StateTimer += DeltaTime;

	switch (CurrentState)
	{
	case EEnemyAttackState::Idle:
		if (StateTimer >= AttackCooldown)
		{
			StartAttack();
		}
		break;

	case EEnemyAttackState::Windup:
		if (StateTimer >= AttackWindupTime && !bHitCheckPerformed)
		{
			PerformAttackHitCheck();
			bHitCheckPerformed = true;
		}

		if (StateTimer >= AttackDuration)
		{
			EndAttack();
		}
		break;

	default:
		break;
	}
}

void AEnemyCharacter::StartAttack()
{
	CurrentState = EEnemyAttackState::Windup;
	StateTimer = 0.0f;
	bHitCheckPerformed = false;

	UpdateIndicator(true);

	if (Mesh && AttackAnim)
	{
		Mesh->PlayAnimation(AttackAnim);
		Mesh->SetAnimationLooping(false);
		Mesh->SetAnimationPlaybackTime(0.0f);
		Mesh->SetAnimationPlayRate(1.0f);
	}
}

void AEnemyCharacter::PerformAttackHitCheck()
{
	ALayerCharacter* Player = FindPlayerCharacter();
	if (!Player)
	{
		return;
	}

	const FVector EnemyLoc = GetActorLocation();
	const FVector EnemyFwd = GetActorRotation().Quaternion().GetForwardVector();
	const FVector PlayerLoc = Player->GetActorLocation();

	FVector ToPlayer = PlayerLoc - EnemyLoc;
	const float HeightDiff = std::abs(ToPlayer.Z);
	ToPlayer.Z = 0.0f;

	const float ForwardDist = ToPlayer.Dot(EnemyFwd);
	const FVector LateralVec = ToPlayer - EnemyFwd * ForwardDist;
	const float LateralDist = LateralVec.Size();

	const bool bInForwardRange = (ForwardDist >= 0.0f && ForwardDist <= AttackRange);
	const bool bInLateralRange = (LateralDist <= (AttackWidth * 0.5f));
	const bool bInHeightRange = (HeightDiff <= 2.0f);

	if (bInForwardRange && bInLateralRange && bInHeightRange)
	{
		HTR_LOG(Info, "[EnemyCharacter] Strike Hit LayerCharacter! (Dist={:.2f}m, Lat={:.2f}m, Knockback={:.2f}m)",
			ForwardDist, LateralDist, KnockbackDistance);
		Player->TakeDamage(Damage, EnemyFwd, KnockbackDistance);
	}
}

void AEnemyCharacter::EndAttack()
{
	CurrentState = EEnemyAttackState::Idle;
	StateTimer = 0.0f;
	bHitCheckPerformed = false;

	UpdateIndicator(false);

	if (Mesh && IdleAnim)
	{
		Mesh->PlayAnimation(IdleAnim);
		Mesh->SetAnimationLooping(true);
		Mesh->SetAnimationPlayRate(1.0f);
	}
}

void AEnemyCharacter::UpdateIndicator(bool bShow)
{
	if (AttackIndicatorBox)
	{
		AttackIndicatorBox->SetRelativeLocation(FVector(AttackRange * 0.5f, 0.0f, -0.89f));
		AttackIndicatorBox->SetBoxExtent(FVector(AttackRange * 0.5f, AttackWidth * 0.5f, 0.03f));
		AttackIndicatorBox->SetShapeColor(FColor(255, 30, 30, 255));
		AttackIndicatorBox->SetDrawOnlyIfSelected(!bShow);
		AttackIndicatorBox->SetGameHidden(!bShow);
	}

	if (AttackIndicatorDecal)
	{
		const float ProjectionDepth = 2.5f;
		AttackIndicatorDecal->SetRelativeLocation(FVector(AttackRange * 0.5f, 0.0f, 0.0f));
		AttackIndicatorDecal->SetDecalSize(bShow ? FVector(ProjectionDepth, AttackWidth, AttackRange) : FVector(0.001f, 0.001f, 0.001f));
		AttackIndicatorDecal->SetDecalColor(bShow ? FLinearColor(1.0f, 0.08f, 0.08f, 0.45f) : FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
	}
}

ALayerCharacter* AEnemyCharacter::FindPlayerCharacter() const
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	if (APlayerController* PC = World->GetPlayerController())
	{
		if (ALayerCharacter* Char = Cast<ALayerCharacter>(PC->GetPawn()))
		{
			return Char;
		}
	}

	if (ULevel* Level = World->GetCurrentLevel())
	{
		for (AActor* Actor : Level->GetActors())
		{
			if (ALayerCharacter* Char = Cast<ALayerCharacter>(Actor))
			{
				return Char;
			}
		}
	}

	return nullptr;
}
