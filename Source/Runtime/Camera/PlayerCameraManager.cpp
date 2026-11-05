#include "EnginePCH.h"

#include "PlayerCameraManager.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Camera/CameraModifier.h"
#include "Camera/CameraModifier_FOVTest.h"
#include "Camera/CameraModifier_CameraShake.h"
#include "Camera/CameraShakeHit.h"

APlayerCameraManager::APlayerCameraManager()
{
    DefaultModifiers.Add(UCameraModifier_CameraShake::StaticClass());
}

APlayerCameraManager::~APlayerCameraManager()
{
    ClearCameraModifiers();
}

void APlayerCameraManager::BeginPlay()
{
    Super::BeginPlay();

    //Fade 테스트용 코드, 시작할 때 FadeIn 해본다
    //StartCameraFade(1.f, 0.f, 2.f, FLinearColor::Black);
    //StartCameraFade(0.f, 1.f, 2.f, FLinearColor::Black, true);

    //테스트용 코드
    //AddNewCameraModifier<UCameraModifier_FOVTest>();

    //StartCameraShake<UCameraShakeHit>();    
}

void APlayerCameraManager::UpdateCamera(float DeltaTime)
{
	// 지금 보고 있는 대상의 카메라
    UpdateViewTarget(ViewTarget, DeltaTime);          // ViewTarget.POV = Target->CalcCamera(dt)
    FMinimalViewInfo POV = ViewTarget.POV;

    // 전환 중이면 새 대상과 섞기
    if (PendingViewTarget.Target)
    {
        UpdateViewTarget(PendingViewTarget, DeltaTime);
        BlendTimeToGo -= DeltaTime;

        //const float Alpha = EvalBlend(1.f - BlendTimeToGo / BlendParams.BlendTime);
        //POV = BlendPOV(ViewTarget.POV, PendingViewTarget.POV, Alpha);

        if (BlendTimeToGo <= 0.f) 
        { 
            ViewTarget = PendingViewTarget; 
            PendingViewTarget = {}; 
        }
    }

    // Modifier 적용
    ApplyCameraModifiers(DeltaTime, POV);

    // 화면 효과 값 갱신
    if (bUpdateFading)
    {
        UpdateFade(DeltaTime);
    }

    UpdateLetterBox(DeltaTime);

    // 렌더러가 읽을 값
    CameraCachePOV = POV;
}

void APlayerCameraManager::UpdateViewTarget(FViewTarget& InViewTarget, float DeltaTime)
{
    AActor* Target = InViewTarget.Target;

    if (Target)
    {
        InViewTarget.POV = Target->CalcCamera(DeltaTime);
    }
}

UCameraModifier* APlayerCameraManager::AddNewCameraModifier(UClass* ModifierClass)
{
    if (!ModifierClass || !ModifierClass->IsChildOf(UCameraModifier::StaticClass()))
        return nullptr;

    //클래스를 받아서 직접 생성
    UCameraModifier* NewModifier = NewObject<UCameraModifier>(this, ModifierClass);
    if (!NewModifier)
        return nullptr;

    if (!AddCameraModifierToList(NewModifier))
    {
        delete NewModifier;
        return nullptr;
    }

    return NewModifier;
}

UCameraModifier* APlayerCameraManager::FindCameraModifierByClass(UClass* ModifierClass) const
{
    for (UCameraModifier* Mod : ModifierList)
    {
        if (Mod && Mod->GetClass() == ModifierClass)
            return Mod;
    }
    return nullptr;
}

void APlayerCameraManager::RemoveCameraModifier(UCameraModifier* Modifier)
{
    //없으면 아무것도 안함
    if (!Modifier || ModifiersToRemove.Find(Modifier) != -1)
        return;

    // 이번 프레임부터 적용 안 되게 끄고, 지우는 건 적용 루프가 끝난 뒤
    Modifier->DisableModifier(true);
    ModifiersToRemove.Add(Modifier);
}

void APlayerCameraManager::ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& POV)
{
    // 인덱스 루프로 ModifyCamera 안에서 Add가 일어나도 반복자가 깨지지 않게
    for (int32 i = 0; i < ModifierList.Num(); ++i)
    {
        UCameraModifier* Mod = ModifierList[i];
        if (!Mod || Mod->IsDisabled())
            continue;

        //true가 반환되면 이후 Modifier들은 호출하지 않는다.
        if (Mod->ModifyCamera(DeltaTime, POV))
            break;
    }

    //등록 시켜둔 제거 목록 실제로 제거
    FlushRemovedModifiers();
}

UCameraShakeBase* APlayerCameraManager::StartCameraShake(UClass* ShakeClass, float Scale)
{
    UCameraModifier_CameraShake* ShakeModifier = FindCameraModifierByClass<UCameraModifier_CameraShake>();
    if (!ShakeModifier)
        return nullptr;

    return ShakeModifier->AddCameraShake(ShakeClass, Scale);
}

void APlayerCameraManager::StopAllCameraShakes(bool bImmediate)
{
    if (UCameraModifier_CameraShake* ShakeModifier = FindCameraModifierByClass<UCameraModifier_CameraShake>())
    {
        ShakeModifier->RemoveAllCameraShakes(bImmediate);
    }
}

void APlayerCameraManager::UpdateFade(float DeltaTime)
{
    if (!bEnableFading) return;

    //남은 fade 시간
    FadeTimeRemaining = std::max(FadeTimeRemaining - DeltaTime, 0.f);

    //Fade 비율
    const float T = FadeTime > 0.f ? 1.f - (FadeTimeRemaining / FadeTime) : 1.f;
    FadeAmount = FadeAlpha.X + (FadeAlpha.Y - FadeAlpha.X) * T;

    //페이드 인(1 -> 0) : bHoldWhenFinished false
    //페이드 아웃(0 -> 1) : bHoldWhenFinished true
    //      아래 조건에 false가 되어 bEnableFading이 false가 되지 않고 계속 
    //      
    if (FadeTimeRemaining <= 0.f && !bHoldWhenFinished)
    {
        bEnableFading = false;
    }
}

void APlayerCameraManager::UpdateLetterBox(float DeltaTime)
{
}

const FMinimalViewInfo& APlayerCameraManager::GetViewTargetPOV() const
{
    return ViewTarget.POV;
}

const FMinimalViewInfo& APlayerCameraManager::GetCameraCachePOV() const
{
    return CameraCachePOV;
}

void APlayerCameraManager::SetViewTarget(AActor* NewTarget)
{
    ViewTarget.SetNewTarget(NewTarget);

    if (NewTarget)
    {
        CameraCachePOV = ViewTarget.POV;
        PendingViewTarget = {};
        BlendTimeToGo = 0.f;
    }
    else
    {
        CameraCachePOV = FMinimalViewInfo();
    }
}

AActor* APlayerCameraManager::GetViewTarget() const
{
    return ViewTarget.Target;
}

void APlayerCameraManager::InitializeFor(APlayerController* PC)
{
    if (!PC) return;

    OwnerPC = PC;

    APawn* Pawn = OwnerPC->GetPawn();
    if (Pawn)
    {
        SetViewTarget(Pawn);
    }
    else
    {
        SetViewTarget(PC);
    }

    //기본 Modifier 추가
    for (auto& ModifierClass : DefaultModifiers)
    {
        AddNewCameraModifier(ModifierClass);
    }
}

void APlayerCameraManager::StartCameraFade(float From, float To, float Duration, FLinearColor Color, bool bInHoldWhenFinished)
{
    bEnableFading = true;
    bUpdateFading = true;
    FadeAlpha.X = From;
    FadeAlpha.Y = To;
    FadeTime = Duration;
    FadeColor = Color;
    bHoldWhenFinished = bInHoldWhenFinished;
    FadeTimeRemaining = FadeTime;
}

void APlayerCameraManager::StopCameraFade()
{
    bEnableFading = false;
}

void APlayerCameraManager::SetManualCameraFade(float Amount, FLinearColor Color)
{
    bEnableFading = true;
    bUpdateFading = false;
    FadeAmount = Amount;
    FadeColor = Color;
}

FCameraScreenEffects APlayerCameraManager::GetScreenEffects() const
{
    FCameraScreenEffects Effects;
    if (bEnableFading)
    {
        Effects.FadeColor = FadeColor;
        Effects.FadeAmount = FadeAmount;
    }

    Effects.Vignette = Vignette;

    return Effects;
}

bool APlayerCameraManager::AddCameraModifierToList(UCameraModifier* NewModifier)
{
    const uint8 NewPriority = NewModifier->GetPriority();

    // Exclusive: 같은 Priority에 Exclusive가 이미 있으면 거절
    if (NewModifier->bExclusive)
    {
        for (UCameraModifier* Mod : ModifierList)
        {
            if (Mod->bExclusive && Mod->GetPriority() == NewPriority)
                return false;
        }
    }

    // 자기보다 Priority '값이 큰' 첫 위치 앞에 넣는다.
    // 같은 Priority끼리는 먼저 들어온 것이 먼저 적용된다.
    int32 InsertIndex = ModifierList.Num();
    for (int32 i = 0; i < ModifierList.Num(); ++i)
    {
        if (ModifierList[i]->GetPriority() > NewPriority)
        {
            InsertIndex = i;
            break;
        }
    }

    ModifierList.Insert(NewModifier, InsertIndex);

    NewModifier->AddedToCamera(this);
    return true;
}

void APlayerCameraManager::RemoveModifierFromList(UCameraModifier* Modifier)
{
    const int32 Index = ModifierList.Find(Modifier);
    if (Index != -1)
        ModifierList.RemoveAt(Index);

    const int32 RemoveIndex = ModifiersToRemove.Find(Modifier);
    if (RemoveIndex != -1)
        ModifiersToRemove.RemoveAt(RemoveIndex);
}

void APlayerCameraManager::FlushRemovedModifiers()
{
    // 복사 후 비운다. Modifier 소멸자가 RemoveModifierFromList로 두 목록에서 자기를 뺀다
    TArray<UCameraModifier*> ToDelete = ModifiersToRemove;
    ModifiersToRemove.Reset();

    for (UCameraModifier* Mod : ToDelete)
        delete Mod;
}

void APlayerCameraManager::ClearCameraModifiers()
{
    // 복사 후 비운다. 이미 비어 있으므로 Modifier 소멸자의 RemoveModifierFromList는 아무것도 안 한다
    TArray<UCameraModifier*> ToDelete = ModifierList;
    ModifierList.Reset();
    ModifiersToRemove.Reset();

    for (UCameraModifier* Mod : ToDelete)
        delete Mod;
}

//------------------------------------------FViewTarget-------------------------------------------

void FViewTarget::SetNewTarget(AActor* NewTarget)
{
    Target = NewTarget;
    if (Target)
    {
        POV = Target->CalcCamera(0.f);
    }    
}

APawn* FViewTarget::GetTargetPawn() const
{
    return Cast<APawn>(Target);
}

