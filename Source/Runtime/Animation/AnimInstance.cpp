#include "EnginePCH.h"
#include "AnimInstance.h"
#include "Component/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationGraph.h"
#include "Animation/AnimNode_StateMachine.h"
#include <algorithm>
#include <cmath>

UAnimInstance::UAnimInstance()
{
	RootNode = &SequencePlayer;
}
UAnimInstance::~UAnimInstance()
{
	ClearAnimationNodes();
}
void UAnimInstance::NativeInitializeAnimation()
{
}

void UAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
}

void UAnimInstance::InitializeAnimation(USkeletalMeshComponent* InOwningComponent)
{
	if (bInitialized)
	{
		return;
	}
	OwningComponent = InOwningComponent;
	NativeInitializeAnimation();
	bInitialized = true;
}

bool UAnimInstance::InitializeAnimationGraph(UAnimationGraph* InGraph)
{
    if (!InGraph || !InGraph->IsAnimationGraphDataValid())
    {
        return false;
    }

    ClearAnimationNodes();
    ClearAnimationParameters();

    for (const FAnimGraphParameterDefinition& Parameter : InGraph->GetParameters())
    {
        if (Parameter.Type == EAnimGraphParameterType::Bool)
        {
            SetBoolParameter(Parameter.Name, Parameter.BoolDefaultValue);
        }
        else if (Parameter.Type == EAnimGraphParameterType::Float)
        {
            SetFloatParameter(Parameter.Name, Parameter.FloatDefaultValue);
        }
    }

    FAnimNode_StateMachine* StateMachine = CreateAnimationNode<FAnimNode_StateMachine>();
	StateMachineNode = StateMachine;

    for (const FAnimGraphStateDefinition& State : InGraph->GetStates())
    {
        FAnimNode_Base* StateNode = nullptr;

        if (State.NodeType == EAnimGraphStateNodeType::SequencePlayer)
        {
            FAnimNode_SequencePlayer* Player = CreateAnimationNode<FAnimNode_SequencePlayer>();

            Player->SetSequence(State.Sequence);
            Player->SetPlayRate(State.PlayRate);
            Player->SetLooping(State.bLooping);

            StateNode = Player;
        }
        else if (State.NodeType == EAnimGraphStateNodeType::BlendSpacePlayer)
        {
            FAnimNode_BlendSpacePlayer* Player = CreateAnimationNode<FAnimNode_BlendSpacePlayer>();

            Player->SetBlendSpace(State.BlendSpace);
            Player->SetBlendParameter(State.BlendParameterName);
            Player->SetPlayRate(State.PlayRate);
            Player->SetLooping(State.bLooping);

            StateNode = Player;
        }

        if (!StateNode || !StateMachine->AddState(State.Name, StateNode))
        {
            ClearAnimationNodes();
            ClearAnimationParameters();
            return false;
        }
    }

    for (const FAnimGraphTransitionDefinition& Transition : InGraph->GetTransitions())
    {
        bool bAdded = false;

        if (Transition.bAnyState)
        {
            bAdded = StateMachine->AddAnyStateTransition(Transition.ToState, Transition.Rule, Transition.ParameterName, Transition.Priority, Transition.BlendDuration, Transition.FloatThreshold);
        }
        else
        {
            bAdded = StateMachine->AddTransition(Transition.FromState, Transition.ToState, Transition.Rule, Transition.ParameterName, Transition.Priority, Transition.BlendDuration, Transition.FloatThreshold);
        }

        if (!bAdded)
        {
            ClearAnimationNodes();
            ClearAnimationParameters();
            return false;
        }
    }

    if (!StateMachine->SetState(InGraph->GetEntryState()))
    {
        ClearAnimationNodes();
        ClearAnimationParameters();
        return false;
    }

    if (!SetRootNode(StateMachine))
    {
        ClearAnimationNodes();
        ClearAnimationParameters();
        return false;
    }

    return true;
}

void UAnimInstance::UpdateAnimation(float DeltaSeconds)
{
	if (!bInitialized)
	{
		return;
	}

	PendingAnimNotifyUpdates.Reset();
    PendingRootMotionTranslation = FVector::ZeroVector;
    PendingRootMotionRotationSum = FQuat(0.0f, 0.0f, 0.0f, 0.0f);
    PendingRootMotionWeight = 0.0f;

	NativeUpdateAnimation(DeltaSeconds);

	FAnimationUpdateContext Context;
	Context.DeltaTime = DeltaSeconds;
	Context.AnimInstance = this;

	if (RootNode)
	{
		RootNode->Update(Context);
	}

	TriggerAnimNotifies(DeltaSeconds);
}

void UAnimInstance::EvaluateAnimation(FPoseContext& Output)
{
	if (!bInitialized || !RootNode)
	{
		return;
	}
	RootNode->Evaluate(Output);
}

AActor* UAnimInstance::GetOwningActor() const
{
	return OwningComponent ? OwningComponent->GetOwner() : nullptr;
}

void UAnimInstance::PlayAnimation(UAnimSequence* InSequence)
{
    EndActiveAnimNotifyStates();
    PendingAnimNotifyUpdates.Reset();

    SequencePlayer.SetSequence(InSequence);
    RootNode = &SequencePlayer;
}

void UAnimInstance::PlayBlendAnimation(UAnimSequence* InSequenceA, UAnimSequence* InSequenceB, float InAlpha)
{
    EndActiveAnimNotifyStates();
    PendingAnimNotifyUpdates.Reset();

	TwoWayBlendPlayer.SetAnimations(InSequenceA, InSequenceB);
	TwoWayBlendPlayer.SetAlpha(InAlpha);
	RootNode = &TwoWayBlendPlayer;
}

void UAnimInstance::PlayBlendSpace(UBlendSpace1D* InBlendSpace, float InInput)
{
    EndActiveAnimNotifyStates();
    PendingAnimNotifyUpdates.Reset();

	BlendSpacePlayer.SetBlendSpace(InBlendSpace);
	BlendSpacePlayer.SetBlendInput(InInput);
	RootNode = &BlendSpacePlayer;
}

void UAnimInstance::SetAnimationBlendAlpha(float InAlpha)
{
	if (RootNode == &TwoWayBlendPlayer)
	{
		TwoWayBlendPlayer.SetAlpha(InAlpha);
	}
}

float UAnimInstance::GetAnimationBlendAlpha() const
{
	return RootNode == &TwoWayBlendPlayer ? TwoWayBlendPlayer.GetAlpha() : 0.0f;
}

void UAnimInstance::SetAnimationBlendInput(float InInput)
{
	if (RootNode == &BlendSpacePlayer)
	{
		BlendSpacePlayer.SetBlendInput(InInput);
	}
}

float UAnimInstance::GetAnimationBlendInput() const
{
	return RootNode == &BlendSpacePlayer ? BlendSpacePlayer.GetBlendInput() : 0.0f;
}

void UAnimInstance::SetPlaybackPosition(float InTime)
{
	if (IAnimPlaybackControl* Control = GetPlaybackControl())
	{
		Control->SetPlaybackPosition(InTime);
	}
}

float UAnimInstance::GetCurrentTime() const
{
	const IAnimPlaybackControl* Control = GetPlaybackControl();
	return Control ? Control->GetCurrentTime() : 0.0f;
}

float UAnimInstance::GetPlayLength() const
{
	const IAnimPlaybackControl* Control = GetPlaybackControl();
	return Control ? Control->GetPlayLength() : 0.0f;
}

float UAnimInstance::GetFrameRate() const
{
	const IAnimPlaybackControl* Control = GetPlaybackControl();
	return Control ? Control->GetFrameRate() : 0.0f;
}

bool UAnimInstance::GetCurveValue(const FName& CurveName, const FName& LayerName, float& OutValue) const
{
    OutValue = 0.0f;

    if (!RootNode || CurveName.IsNone())
    {
        return false;
    }

    return RootNode->EvaluateCurve(CurveName, LayerName, OutValue);
}

void UAnimInstance::SetPlayRate(float InPlayRate)
{
	if (IAnimPlaybackControl* Control = GetPlaybackControl())
	{
		Control->SetPlayRate(InPlayRate);
	}
}

float UAnimInstance::GetPlayRate() const
{
	const IAnimPlaybackControl* Control = GetPlaybackControl();
	return Control ? Control->GetPlayRate() : 1.0f;
}

void UAnimInstance::SetLooping(bool bInLoop)
{
	if (IAnimPlaybackControl* Control = GetPlaybackControl())
	{
		Control->SetLooping(bInLoop);
	}
}

bool UAnimInstance::IsLooping() const
{
	const IAnimPlaybackControl* Control = GetPlaybackControl();
	return Control ? Control->IsLooping() : true;
}

void UAnimInstance::SetBoolParameter(const FName& Name, bool Value)
{
	if (Name.IsNone())
	{
		return;
	}
	BoolParameters.Add(Name, Value);
}

bool UAnimInstance::GetBoolParameter(const FName& Name) const
{
	const bool* Value = BoolParameters.Find(Name);
	return Value ? *Value : false;
}

bool UAnimInstance::HasBoolParameter(const FName& Name) const
{
	return BoolParameters.Contains(Name);
}

bool UAnimInstance::HasFloatParameter(const FName& Name) const
{
	return FloatParameters.Contains(Name);
}

void UAnimInstance::SetFloatParameter(const FName& Name, float Value)
{
	if (Name.IsNone())
	{
		return;
	}

	FloatParameters.Add(Name, std::isfinite(Value) ? Value : 0.0f);
}

float UAnimInstance::GetFloatParameter(const FName& Name) const
{
	const float* Value = FloatParameters.Find(Name);
	return Value ? *Value : 0.0f;
}

void UAnimInstance::ClearAnimationParameters()
{
	BoolParameters.Reset();
	FloatParameters.Reset();
}

bool UAnimInstance::SetRootNode(FAnimNode_Base* InRootNode)
{
    if (InRootNode && InRootNode != &SequencePlayer && InRootNode != &TwoWayBlendPlayer && InRootNode != &BlendSpacePlayer && OwnedAnimationNodes.Find(InRootNode) == -1)
    {
        return false;
    }

    if (RootNode != InRootNode)
    {
        EndActiveAnimNotifyStates();
        PendingAnimNotifyUpdates.Reset();
    }

    RootNode = InRootNode;
    return true;
}

void UAnimInstance::ClearAnimationNodes()
{
	EndActiveAnimNotifyStates();
	PendingAnimNotifyUpdates.Reset();

	RootNode = &SequencePlayer;
	StateMachineNode = nullptr;

	SequencePlayer.SetSequence(nullptr);

	for (FAnimNode_Base* Node : OwnedAnimationNodes)
	{
		delete Node;
	}

	OwnedAnimationNodes.Reset();
}

bool UAnimInstance::GetStateMachineDebugInfo(FAnimStateMachineDebugInfo& OutInfo) const
{
	if (!StateMachineNode || RootNode != StateMachineNode)
	{
		return false;
	}
	StateMachineNode->GetDebugInfo(OutInfo);
	return true;
}

void UAnimInstance::QueueAnimNotifyUpdate(const FAnimNode_SequencePlayer* Player, UAnimSequence* Sequence, float PreviousTime, float Advance, float CurrentTime, float PlayLength, bool bLooping, bool bIsFirstUpdate)
{
	if (!Player || !Sequence || !Sequence->GetDataModel())
	{
		return;
	}

	if (!std::isfinite(PreviousTime) || !std::isfinite(Advance) || !std::isfinite(CurrentTime) || !std::isfinite(PlayLength) || PlayLength <= 0.0f)
	{
		return;
	}

	FQueuedAnimNotifyUpdate Update;
	Update.Player = Player;
	Update.Sequence = Sequence;
	Update.PreviousTime = PreviousTime;
	Update.Advance = Advance;
	Update.CurrentTime = CurrentTime;
	Update.PlayLength = PlayLength;
	Update.bLooping = bLooping;
	Update.bIsFirstUpdate = bIsFirstUpdate;

	PendingAnimNotifyUpdates.Add(std::move(Update));
}

void UAnimInstance::TriggerAnimNotifies(float DeltaSeconds)
{
    struct FNotifyDispatch
    {
        double Progress = 0.0;
        FName NotifyName;
        EAnimNotifyEventType EventType = EAnimNotifyEventType::Trigger;
    };
    TArray<FNotifyDispatch> AllDispatches;
    TArray<FActiveAnimNotifyState> NextActiveStates;
    TArray<FActiveAnimNotifyState> ProcessedActiveStates;

    const float NotifyDeltaSeconds = std::isfinite(DeltaSeconds) ? std::max(DeltaSeconds, 0.0f) : 0.0f;
    constexpr double TimeTolerance = 1.0e-6;

    for (const FQueuedAnimNotifyUpdate& Update : PendingAnimNotifyUpdates)
    {
        if (!Update.Player || !Update.Sequence || !Update.Sequence->GetDataModel())
        {
            continue;
        }

        const TArray<FAnimNotifyEvent>& Notifies = Update.Sequence->GetNotifies();
        const double PathStart = Update.PreviousTime;
        const double PathEnd = PathStart + Update.Advance;

        if (!std::isfinite(PathEnd))
        {
            continue;
        }

        const double MinPath = std::min(PathStart, PathEnd);
        const double MaxPath = std::max(PathStart, PathEnd);

        auto AddDispatch = [&](double EventTime, const FName& NotifyName, EAnimNotifyEventType EventType)
            {
                FNotifyDispatch Dispatch;
                Dispatch.Progress = std::abs(Update.Advance) > TimeTolerance ? std::clamp((EventTime - PathStart) / Update.Advance, 0.0, 1.0) : 0.0;
                Dispatch.NotifyName = NotifyName;
                Dispatch.EventType = EventType;
                AllDispatches.Add(std::move(Dispatch));
            };

        for (int32 NotifyIndex = 0; NotifyIndex < Notifies.Num(); ++NotifyIndex)
        {
            const FAnimNotifyEvent& Notify = Notifies[NotifyIndex];

            if (Notify.NotifyName.IsNone() || !std::isfinite(Notify.TriggerTime) || !std::isfinite(Notify.Duration) || Notify.TriggerTime < 0.0f || Notify.Duration < 0.0f)
            {
                continue;
            }

            const double NotifyStart = Notify.TriggerTime;
            const double NotifyEnd = NotifyStart + Notify.Duration;

            if (NotifyEnd > Update.PlayLength + TimeTolerance)
            {
                continue;
            }

            if (Notify.Duration == 0.0f)
            {
                if (std::abs(Update.Advance) <= TimeTolerance && !Update.bIsFirstUpdate)
                {
                    continue;
                }

                const long long FirstCycle = Update.bLooping ? static_cast<long long>(std::floor((MinPath - NotifyStart) / Update.PlayLength)) : 0;
                const long long LastCycle = Update.bLooping ? static_cast<long long>(std::ceil((MaxPath - NotifyStart) / Update.PlayLength)) : 0;

                for (long long Cycle = FirstCycle; Cycle <= LastCycle; ++Cycle)
                {
                    const double EventTime = NotifyStart + Cycle * Update.PlayLength;

                    bool bCrossed = false;

                    if (Update.Advance > TimeTolerance)
                    {
                        bCrossed = EventTime > PathStart && EventTime <= PathEnd;
                    }
                    else if (Update.Advance < -TimeTolerance)
                    {
                        bCrossed = EventTime >= PathEnd && EventTime < PathStart;
                    }

                    if (!bCrossed && Update.bIsFirstUpdate && std::abs(EventTime - PathStart) <= TimeTolerance)
                    {
                        bCrossed = true;
                    }

                    if (bCrossed)
                    {
                        AddDispatch(EventTime, Notify.NotifyName, EAnimNotifyEventType::Trigger);
                    }
                }

                continue;
            }

            bool bWasActive = false;

            for (const FActiveAnimNotifyState& ActiveState : ActiveAnimNotifyStates)
            {
                if (ActiveState.Player == Update.Player && ActiveState.Sequence == Update.Sequence && ActiveState.NotifyIndex == NotifyIndex && ActiveState.NotifyName == Notify.NotifyName)
                {
                    bWasActive = true;
                    ProcessedActiveStates.Add(ActiveState);
                    break;
                }
            }

            const bool bReverse = Update.Advance < -TimeTolerance;

            const bool bEndpointInside = bReverse
                ? Update.CurrentTime > NotifyStart && Update.CurrentTime <= NotifyEnd
                : Update.CurrentTime >= NotifyStart && Update.CurrentTime < NotifyEnd;

            const long long FirstCycle = Update.bLooping ? static_cast<long long>(std::floor((MinPath - NotifyEnd) / Update.PlayLength)) : 0;
            const long long LastCycle = Update.bLooping ? static_cast<long long>(std::ceil((MaxPath - NotifyStart) / Update.PlayLength)) : 0;

            bool bEndDispatched = false;

            for (long long Cycle = FirstCycle; Cycle <= LastCycle; ++Cycle)
            {
                const double OccurrenceStart = NotifyStart + Cycle * Update.PlayLength;
                const double OccurrenceEnd = NotifyEnd + Cycle * Update.PlayLength;

                bool bStartInside = false;
                bool bCrossBegin = false;
                bool bCrossEnd = false;

                if (Update.Advance > TimeTolerance)
                {
                    bStartInside = PathStart >= OccurrenceStart && PathStart < OccurrenceEnd;
                    bCrossBegin = OccurrenceStart > PathStart && OccurrenceStart <= PathEnd;
                    bCrossEnd = OccurrenceEnd > PathStart && OccurrenceEnd <= PathEnd;
                }
                else if (Update.Advance < -TimeTolerance)
                {
                    bStartInside = PathStart > OccurrenceStart && PathStart <= OccurrenceEnd;
                    bCrossBegin = OccurrenceEnd < PathStart && OccurrenceEnd >= PathEnd;
                    bCrossEnd = OccurrenceStart < PathStart && OccurrenceStart >= PathEnd;
                }
                else
                {
                    bStartInside = PathStart >= OccurrenceStart && PathStart < OccurrenceEnd;
                }

                if (bCrossBegin || (bStartInside && !bWasActive))
                {
                    const double BeginTime = bReverse ? OccurrenceEnd : OccurrenceStart;
                    AddDispatch(bCrossBegin ? BeginTime : PathStart, Notify.NotifyName, EAnimNotifyEventType::Begin);
                }

                const double OverlapStart = std::max(MinPath, OccurrenceStart);
                const double OverlapEnd = std::min(MaxPath, OccurrenceEnd);
                const bool bHasOverlap = OverlapEnd > OverlapStart || (std::abs(Update.Advance) <= TimeTolerance && bStartInside);

                if (bHasOverlap)
                {
                    const double TickTime = std::abs(Update.Advance) > TimeTolerance ? (OverlapStart + OverlapEnd) * 0.5 : PathStart;
                    AddDispatch(TickTime, Notify.NotifyName, EAnimNotifyEventType::Tick);
                }

                if (bCrossEnd)
                {
                    const double EndTime = bReverse ? OccurrenceStart : OccurrenceEnd;
                    AddDispatch(EndTime, Notify.NotifyName, EAnimNotifyEventType::End);
                    bEndDispatched = true;
                }
            }

            if (bWasActive && !bEndpointInside && !bEndDispatched)
            {
                AddDispatch(PathEnd, Notify.NotifyName, EAnimNotifyEventType::End);
            }

            if (bEndpointInside)
            {
                FActiveAnimNotifyState ActiveState;
                ActiveState.Player = Update.Player;
                ActiveState.Sequence = Update.Sequence;
                ActiveState.NotifyIndex = NotifyIndex;
                ActiveState.NotifyName = Notify.NotifyName;
                NextActiveStates.Add(std::move(ActiveState));
            }
        }
    }

    for (const FActiveAnimNotifyState& PreviousState : ActiveAnimNotifyStates)
    {
        bool bWasProcessed = false;

        for (const FActiveAnimNotifyState& ProcessedState : ProcessedActiveStates)
        {
            if (PreviousState.Player == ProcessedState.Player && PreviousState.Sequence == ProcessedState.Sequence && PreviousState.NotifyIndex == ProcessedState.NotifyIndex && PreviousState.NotifyName == ProcessedState.NotifyName)
            {
                bWasProcessed = true;
                break;
            }
        }

        if (!bWasProcessed)
        {
            FNotifyDispatch Dispatch;
            Dispatch.Progress = 0.0;
            Dispatch.NotifyName = PreviousState.NotifyName;
            Dispatch.EventType = EAnimNotifyEventType::End;
            AllDispatches.Add(std::move(Dispatch));
        }
    }

    std::stable_sort(AllDispatches.begin(), AllDispatches.end(), [](const FNotifyDispatch& A, const FNotifyDispatch& B)
        {
            if (std::abs(A.Progress - B.Progress) > 1.0e-6)
            {
                return A.Progress < B.Progress;
            }

            if (A.EventType == EAnimNotifyEventType::End && B.EventType != EAnimNotifyEventType::End)
            {
                return true;
            }
            return false;
        });

    if (OwningComponent)
    {
        for (const FNotifyDispatch& Dispatch : AllDispatches)
        {
            OwningComponent->HandleAnimNotify(Dispatch.NotifyName, Dispatch.EventType, NotifyDeltaSeconds);
        }
    }

    ActiveAnimNotifyStates.Reset();

    for (const FActiveAnimNotifyState& ActiveState : NextActiveStates)
    {
        ActiveAnimNotifyStates.Add(ActiveState);
    }

    PendingAnimNotifyUpdates.Reset();
}

bool UAnimInstance::IsRootMotionEnabled() const
{
    return OwningComponent && OwningComponent->IsRootMotionEnabled();
}

void UAnimInstance::QueueRootMotionDelta(const FVector& Translation, const FQuat& Rotation, float Weight)
{
    if (!std::isfinite(Weight) || Weight <= 0.0f || !std::isfinite(Translation.X) || !std::isfinite(Translation.Y) || !std::isfinite(Translation.Z))
    {
        return;
    }

    FQuat Contribution = Rotation.Normalized();

    if (PendingRootMotionWeight > 0.0f)
    {
        const float Dot = PendingRootMotionRotationSum.X * Contribution.X + PendingRootMotionRotationSum.Y * Contribution.Y + PendingRootMotionRotationSum.Z * Contribution.Z + PendingRootMotionRotationSum.W * Contribution.W;

        if (Dot < 0.0f)
        {
            Contribution = FQuat(-Contribution.X, -Contribution.Y, -Contribution.Z, -Contribution.W);
        }
    }

    PendingRootMotionTranslation += Translation * Weight;
    PendingRootMotionRotationSum.X += Contribution.X * Weight;
    PendingRootMotionRotationSum.Y += Contribution.Y * Weight;
    PendingRootMotionRotationSum.Z += Contribution.Z * Weight;
    PendingRootMotionRotationSum.W += Contribution.W * Weight;
    PendingRootMotionWeight += Weight;
}

bool UAnimInstance::ConsumeRootMotionDelta(FVector& OutTranslation, FQuat& OutRotation)
{
    if (PendingRootMotionWeight <= 1.0e-6f)
    {
        OutTranslation = FVector::ZeroVector;
        OutRotation = FQuat::Identity();
        return false;
    }

    OutTranslation = PendingRootMotionTranslation / PendingRootMotionWeight;
    OutRotation = PendingRootMotionRotationSum.Normalized();

    PendingRootMotionTranslation = FVector::ZeroVector;
    PendingRootMotionRotationSum = FQuat(0.0f, 0.0f, 0.0f, 0.0f);
    PendingRootMotionWeight = 0.0f;

    return true;
}

void UAnimInstance::EndActiveAnimNotifyStates()
{
	if (OwningComponent)
	{
		for (const FActiveAnimNotifyState& ActiveState : ActiveAnimNotifyStates)
		{
			OwningComponent->HandleAnimNotify(ActiveState.NotifyName, EAnimNotifyEventType::End, 0.0f);
		}
	}
	ActiveAnimNotifyStates.Reset();
}
