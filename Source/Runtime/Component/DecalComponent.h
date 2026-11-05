#pragma once

#include "Component/SceneComponent.h"
#include "Math/Color.h"

class UMaterial;
class FDecalSceneProxy;
class FLineBatcher;

class UDecalComponent : public USceneComponent
{
    DECLARE_CLASS(UDecalComponent, USceneComponent)

    REFLECT_START(UDecalComponent)
        PROPERTY(DecalSize)
        PROPERTY(DecalColor)
        PROPERTY(SortOrder)
        PROPERTY(FadeInStartDelay)
        PROPERTY(FadeInDuration)
        PROPERTY(FadeStartDelay)
        PROPERTY(FadeDuration)
        PROPERTY(bDestroyOwnerAfterFade)
        REFLECT_END()

public:
    // 수명주기 갱신
    UDecalComponent();
    ~UDecalComponent() override;
    void InitializeComponent() override;
    void BeginPlay() override;
    void EndPlay() override;
    void TickComponent(float DeltaTime) override;
    void PostLoad() override;
    void PostDuplicate(bool bDuplicateForPIE) override;
    void PostEditChangeProperty(const FProperty& Property) override;
    void OnTransformDirty() override
    {
        Super::OnTransformDirty();
        MarkRenderStateDirty();
    }

    // 투영 및 외형 설정
    UMaterial* GetDecalMaterial() const { return DecalMaterial; }
    bool SetDecalMaterial(UMaterial* InMaterial);
    const FVector& GetDecalSize() const { return DecalSize; }
    void SetDecalSize(const FVector& Value)
    {
        DecalSize = Value;
        ClampSettings();
        MarkRenderStateDirty();
    }
    const FLinearColor& GetDecalColor() const { return DecalColor; }
    void SetDecalColor(const FLinearColor& Value)
    {
        DecalColor = Value;
        ClampSettings();
        MarkRenderStateDirty();
    }
    int32 GetSortOrder() const { return SortOrder; }
    void SetSortOrder(int32 Value)
    {
        SortOrder = Value;
        MarkRenderStateDirty();
    }

    // 공통 시간 기반 페이드 설정
    float GetFadeInStartDelay() const { return FadeInStartDelay; }
    float GetFadeInDuration() const { return FadeInDuration; }
    float GetFadeStartDelay() const { return FadeStartDelay; }
    float GetFadeDuration() const { return FadeDuration; }
    float GetFadeAlpha() const { return FadeAlpha; }
    void SetFadeInStartDelay(float Value) { SetFadeIn(Value, FadeInDuration); }
    void SetFadeInDuration(float Value) { SetFadeIn(FadeInStartDelay, Value); }
    void SetFadeStartDelay(float Value) { SetFadeOut(Value, FadeDuration); }
    void SetFadeDuration(float Value) { SetFadeOut(FadeStartDelay, Value); }
    void SetFadeIn(float Delay, float Duration);
    void SetFadeOut(float Delay, float Duration);
    void ClearFade();

    // 경계 및 프록시 갱신
    FBox CalcLocalBounds() const override;
    void DrawDebug(FLineBatcher* LineBatcher) const;
    void MarkRenderStateDirty();

    // 외형 참조 단일 직렬화
    using Super::Serialize;
    void Serialize(json& Handle, bool bIsLoading) override;
    void Serialize(FStructuredArchive::FRecord Record) override;

    // 씬 소유 프록시의 비소유 참조
    FDecalSceneProxy* SceneProxy = nullptr;

private:
    void ClampSettings();
    void ResetFadeRuntime();
    void RestartFade();

    // 별도 참조 직렬화 대상
    UMaterial* DecalMaterial = nullptr;

    // 전체 로컬 크기 및 투영 설정
    FVector DecalSize = FVector(1.0f, 2.0f, 2.0f);
    FLinearColor DecalColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
    int32 SortOrder = 0;
    float FadeInStartDelay = 0.0f;
    float FadeInDuration = 0.0f;
    float FadeStartDelay = 0.0f;
    float FadeDuration = 0.0f;

    // 페이드 아웃 완료 이후의 소유 액터 삭제 설정
    bool bDestroyOwnerAfterFade = true;

    // 비직렬화 재생 상태
    float FadeElapsedTime = 0.0f;
    float FadeAlpha = 1.0f;
    bool bFadeRunning = false;
    bool bHasBegunPlay = false;
};
