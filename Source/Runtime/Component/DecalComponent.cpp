#include "EnginePCH.h"
#include "Component/DecalComponent.h"
#include "Asset/AssetManager.h"
#include "Engine/DecalSceneProxy.h"
#include "Engine/Scene.h"
#include "Render/LineBatcher.h"
#include "Render/Material.h"
#include <algorithm>
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Serialization/MathSerialization.h"

// 기본 외형 및 틱 설정
UDecalComponent::UDecalComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    DecalMaterial = UAssetManager::GetAssetByPath<UMaterial>("DefaultDecalMaterial");
}

// 씬 참조 정리
UDecalComponent::~UDecalComponent()
{
    if (SceneProxy)
    {
        SceneProxy->GetScene()->RemoveDecal(this);
    }
}

void UDecalComponent::InitializeComponent()
{
    Super::InitializeComponent();
    GetOwner()->GetWorld()->GetScene().AddDecal(this);
}

// 게임 시간 재생 시작
void UDecalComponent::BeginPlay()
{
    Super::BeginPlay();
    bHasBegunPlay = true;
    RestartFade();
}

// 재생 상태 종료
void UDecalComponent::EndPlay()
{
    bHasBegunPlay = false;
    ResetFadeRuntime();
    Super::EndPlay();
}

// 저장 설정 복구
void UDecalComponent::PostLoad()
{
    Super::PostLoad();
    bHasBegunPlay = false;
    ClampSettings();
    ResetFadeRuntime();
}

// 복제 설정 복구
void UDecalComponent::PostDuplicate(bool bDuplicateForPIE)
{
    Super::PostDuplicate(bDuplicateForPIE);
    bHasBegunPlay = false;
    ClampSettings();
    ResetFadeRuntime();
}

// 게임 DeltaTime 기반 페이드 갱신
void UDecalComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);
    if (!bFadeRunning)
    {
        return;
    }

    FadeElapsedTime += std::max(DeltaTime, 0.0f);
    float InAlpha = 1.0f;
    if (FadeElapsedTime < FadeInStartDelay)
    {
        InAlpha = 0.0f;
    }
    else if (FadeInDuration > 0.0f)
    {
        InAlpha = FMath::Clamp((FadeElapsedTime - FadeInStartDelay) / FadeInDuration, 0.0f, 1.0f);
    }

    const float OutAlpha = FadeDuration > 0.0f
        ? FMath::Clamp(1.0f - std::max(FadeElapsedTime - FadeStartDelay, 0.0f) / FadeDuration, 0.0f, 1.0f)
        : 1.0f;

    const float NewAlpha = InAlpha * OutAlpha;
    if (NewAlpha != FadeAlpha)
    {
        FadeAlpha = NewAlpha;
        MarkRenderStateDirty();
    }

    const bool bOutFinished = FadeDuration > 0.0f && FadeElapsedTime >= FadeStartDelay + FadeDuration;
    const bool bInFinished = FadeElapsedTime >= FadeInStartDelay + FadeInDuration;

    if (bOutFinished || (FadeDuration <= 0.0f && bInFinished))
    {
        bFadeRunning = false;
        PrimaryComponentTick.bStartWithTickEnabled = false;
        PrimaryComponentTick.SetTickFunctionEnable(false);

        // 페이드 아웃 완료 이후의 소유 액터 파괴 예약
        if (bOutFinished && bDestroyOwnerAfterFade)
        {
            GetOwner()->Destroy();
        }
    }
}

// 편집 설정 반영
void UDecalComponent::PostEditChangeProperty(const FProperty& Property)
{
    Super::PostEditChangeProperty(Property);
    ClampSettings();
    if (Property.Name == "FadeInStartDelay" || Property.Name == "FadeInDuration" ||
        Property.Name == "FadeStartDelay" || Property.Name == "FadeDuration")
    {
        RestartFade();
    }
    MarkRenderStateDirty();
}

// 데칼 도메인 검증
bool UDecalComponent::SetDecalMaterial(UMaterial* InMaterial)
{
    if (InMaterial && InMaterial->MaterialDomain != EMaterialDomain::Decal)
    {
        HTR_LOG(Warning, "Decal material requires Decal domain");
        return false;
    }

    DecalMaterial = InMaterial;
    MarkRenderStateDirty();
    return true;
}

// 페이드 인 설정 및 공통 시간 재시작
void UDecalComponent::SetFadeIn(float Delay, float Duration)
{
    FadeInStartDelay = Delay;
    FadeInDuration = Duration;
    ClampSettings();
    RestartFade();
}

// 페이드 아웃 설정 및 공통 시간 재시작
void UDecalComponent::SetFadeOut(float Delay, float Duration)
{
    FadeStartDelay = Delay;
    FadeDuration = Duration;
    ClampSettings();
    RestartFade();
}

// 지속 표시 설정
void UDecalComponent::ClearFade()
{
    FadeInStartDelay = FadeInDuration = 0.0f;
    FadeStartDelay = FadeDuration = 0.0f;
    RestartFade();
}

// 편집 상태의 알파 복구
void UDecalComponent::ResetFadeRuntime()
{
    FadeElapsedTime = 0.0f;
    FadeAlpha = 1.0f;
    bFadeRunning = false;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.SetTickFunctionEnable(false);
    MarkRenderStateDirty();
}

// 플레이 기준의 공통 시간 재시작
void UDecalComponent::RestartFade()
{
    ResetFadeRuntime();
    if (!bHasBegunPlay)
    {
        return;
    }

    const bool bHasFadeIn = FadeInStartDelay > 0.0f || FadeInDuration > 0.0f;
    FadeAlpha = bHasFadeIn ? 0.0f : 1.0f;
    bFadeRunning = bHasFadeIn || FadeDuration > 0.0f;
    PrimaryComponentTick.bStartWithTickEnabled = bFadeRunning;
    PrimaryComponentTick.SetTickFunctionEnable(bFadeRunning);
    MarkRenderStateDirty();
}

// 설정 범위 제한
void UDecalComponent::ClampSettings()
{
    DecalSize.X = std::max(DecalSize.X, 0.001f);
    DecalSize.Y = std::max(DecalSize.Y, 0.001f);
    DecalSize.Z = std::max(DecalSize.Z, 0.001f);
    DecalColor.A = FMath::Clamp(DecalColor.A, 0.0f, 1.0f);
    FadeInStartDelay = std::max(FadeInStartDelay, 0.0f);
    FadeInDuration = std::max(FadeInDuration, 0.0f);
    FadeStartDelay = std::max(FadeStartDelay, 0.0f);
    FadeDuration = std::max(FadeDuration, 0.0f);
}

// 전체 크기 기준의 로컬 경계
FBox UDecalComponent::CalcLocalBounds() const
{
    const FVector Half = DecalSize * 0.5f;
    return FBox{ Half * -1.0f, Half };
}

// 투영 볼륨 및 전방 표시
void UDecalComponent::DrawDebug(FLineBatcher* LineBatcher) const
{
    if (!LineBatcher)
    {
        return;
    }

    const FMatrix World = GetWorldMatrix();
    const FVector4 Color(0.15f, 0.8f, 1.0f, 1.0f);
    LineBatcher->AddOrientedBox(CalcLocalBounds(), World, Color);
}

// 프록시 갱신 예약
void UDecalComponent::MarkRenderStateDirty()
{
    if (SceneProxy)
    {
        SceneProxy->GetScene()->MarkDecalDirty(SceneProxy);
    }
}

// JSON 외형 참조 및 표시 설정 저장
void UDecalComponent::Serialize(json& Handle, bool bIsLoading)
{
    Super::Serialize(Handle, bIsLoading);
    if (bIsLoading)
    {
        // 이전 Opacity 설정의 색상 알파 이관
        if (!Handle.contains("DecalColor") && Handle.contains("Opacity"))
        {
            DecalColor.A = Handle["Opacity"].get<float>();
        }
        // 이전 단위 볼륨의 Transform 크기 유지
        if (Handle.contains("Opacity") && !Handle.contains("DecalSize"))
        {
            DecalSize = FVector(1.0f);
        }
        if (Handle.contains("DecalMaterial"))
        {
            UMaterial* Loaded = nullptr;
            if (Handle["DecalMaterial"].is_string())
            {
                Loaded = UAssetManager::GetAssetByPath<UMaterial>(Handle["DecalMaterial"].get<FString>());
                if (Loaded && Loaded->MaterialDomain != EMaterialDomain::Decal)
                {
                    Loaded = UMaterial::CreateInstance(Loaded);
                    Loaded->MaterialDomain = EMaterialDomain::Decal;
                }
            }
            else if (!Handle["DecalMaterial"].is_null())
            {
                Loaded = UMaterial::LoadMaterial(Handle["DecalMaterial"]);
            }
            SetDecalMaterial(Loaded);
        }
        if (Handle.contains("Active"))
        {
            SetActive(Handle["Active"].get<bool>());
        }
        if (Handle.contains("HiddenInGame"))
        {
            SetHiddenInGame(Handle["HiddenInGame"].get<bool>());
        }
        ClampSettings();
        ResetFadeRuntime();
        return;
    }

    Handle["DecalMaterial"] = DecalMaterial ? UMaterial::SaveMaterial(DecalMaterial) : json(nullptr);
    Handle["Active"] = IsActive();
    Handle["HiddenInGame"] = IsHiddenInGame();
}

// 구조화 외형 참조 및 표시 설정 저장
void UDecalComponent::Serialize(FStructuredArchive::FRecord Record)
{
    Super::Serialize(Record);
    FArchive& Ar = Record.GetUnderlyingArchive();
    UMaterial* Loaded = DecalMaterial;
    if (Ar.IsLoading() && !Ar.HasAnyPortFlags(EPropertyPortFlags::PPF_Duplicate))
    {
        // 이전 반사 속성의 외형 및 알파 이관
        FStructuredArchive::FRecord Properties = Record.EnterRecord("Properties");
        bool bHasLegacyOpacity = false;
        if (TOptional<FStructuredArchive::FSlot> Slot = Properties.TryEnterField("Opacity", false))
        {
            Slot.GetValue() << DecalColor.A;
            Loaded = nullptr;
            bHasLegacyOpacity = true;
        }
        if (TOptional<FStructuredArchive::FSlot> Slot = Properties.TryEnterField("DecalColor", false))
        {
            Slot.GetValue() << DecalColor;
        }
        // 명시된 크기 우선 및 이전 단위 볼륨 유지
        if (TOptional<FStructuredArchive::FSlot> Slot = Properties.TryEnterField("DecalSize", false))
        {
            Slot.GetValue() << DecalSize;
        }
        else if (bHasLegacyOpacity)
        {
            DecalSize = FVector(1.0f);
        }
        if (TOptional<FStructuredArchive::FSlot> Slot = Properties.TryEnterField("DecalMaterial", false))
        {
            UObject* LegacyMaterial = nullptr;
            Slot.GetValue() << LegacyMaterial;
            Loaded = Cast<UMaterial>(LegacyMaterial);
            if (Loaded && Loaded->MaterialDomain != EMaterialDomain::Decal)
            {
                Loaded = UMaterial::CreateInstance(Loaded);
                Loaded->MaterialDomain = EMaterialDomain::Decal;
            }
        }
    }
    if (TOptional<FStructuredArchive::FSlot> Slot = Record.TryEnterField("DecalMaterial", true))
    {
        UMaterial::SerializeMaterialReference(Slot.GetValue(), Loaded);
    }
    bool bActive = IsActive();
    bool bHidden = IsHiddenInGame();
    if (TOptional<FStructuredArchive::FSlot> Slot = Record.TryEnterField("Active", true))
    {
        Slot.GetValue() << bActive;
    }
    if (TOptional<FStructuredArchive::FSlot> Slot = Record.TryEnterField("HiddenInGame", true))
    {
        Slot.GetValue() << bHidden;
    }

    if (Record.GetUnderlyingArchive().IsLoading())
    {
        SetDecalMaterial(Loaded);
        SetActive(bActive);
        SetHiddenInGame(bHidden);
        ClampSettings();
        ResetFadeRuntime();
    }
}
