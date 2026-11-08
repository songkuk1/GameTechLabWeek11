#include "EnginePCH.h"
#include "Animation/AnimationGraph.h"

#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/Skeleton.h"

#include <cmath>

bool UAnimationGraph::SetSkeleton(USkeleton* InSkeleton)
{
    if (!InSkeleton)
    {
        return false;
    }
    for (const FAnimGraphStateDefinition& State : States)
    {
        if (State.NodeType == EAnimGraphStateNodeType::SequencePlayer)
        {
            if (!State.Sequence || State.Sequence->GetSkeleton() != InSkeleton)
            {
                return false;
            }
        }
        else if (State.NodeType == EAnimGraphStateNodeType::BlendSpacePlayer)
        {
            if (!State.BlendSpace || State.BlendSpace->GetSkeleton() != InSkeleton)
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }
    Skeleton = InSkeleton;
    return true;
}

bool UAnimationGraph::AddBoolParameter(const FName& Name, bool DefaultValue)
{
    if (Name.IsNone() || FindParameterIndex(Name) != -1)
    {
        return false;
    }

    FAnimGraphParameterDefinition Parameter;
    Parameter.Name = Name;
    Parameter.Type = EAnimGraphParameterType::Bool;
    Parameter.BoolDefaultValue = DefaultValue;
    Parameters.Add(std::move(Parameter));
    return true;
}

bool UAnimationGraph::AddFloatParameter(const FName& Name, float DefaultValue)
{
    if (Name.IsNone() || FindParameterIndex(Name) != -1 || !std::isfinite(DefaultValue))
    {
        return false;
    }
    FAnimGraphParameterDefinition Parameter;
    Parameter.Name = Name;
    Parameter.Type = EAnimGraphParameterType::Float;
    Parameter.FloatDefaultValue = DefaultValue;
    Parameters.Add(std::move(Parameter));
    return true;
}

bool UAnimationGraph::AddSequenceState(const FName& StateName, UAnimSequence* Sequence, float PlayRate, bool bLooping)
{
    if (StateName.IsNone() || FindStateIndex(StateName) != -1 || !Skeleton || !Sequence || Sequence->GetSkeleton() != Skeleton || !std::isfinite(PlayRate))
    {
        return false;
    }
    FAnimGraphStateDefinition State;
    State.Name = StateName;
    State.NodeType = EAnimGraphStateNodeType::SequencePlayer;
    State.Sequence = Sequence;
    State.PlayRate = PlayRate;
    State.bLooping = bLooping;
    States.Add(std::move(State));
    return true;
}

bool UAnimationGraph::AddBlendSpaceState(const FName& StateName, UBlendSpace1D* BlendSpace, const FName& BlendParameterName, float PlayRate, bool bLooping)
{
    if (StateName.IsNone() || FindStateIndex(StateName) != -1 || !Skeleton || !BlendSpace || BlendSpace->GetSkeleton() != Skeleton || !BlendSpace->IsBlendSpaceDataValid() || !std::isfinite(PlayRate))
    {
        return false;
    }
    if (!BlendParameterName.IsNone() && !HasParameterType(BlendParameterName, EAnimGraphParameterType::Float))
    {
        return false;
    }
    FAnimGraphStateDefinition State;
    State.Name = StateName;
    State.NodeType = EAnimGraphStateNodeType::BlendSpacePlayer;
    State.BlendSpace = BlendSpace;
    State.BlendParameterName = BlendParameterName;
    State.PlayRate = PlayRate;
    State.bLooping = bLooping;
    States.Add(std::move(State));
    return true;
}

bool UAnimationGraph::SetEntryState(const FName& StateName)
{
    if (FindStateIndex(StateName) == -1)
    {
        return false;
    }
    EntryStateName = StateName;
    return true;
}

bool UAnimationGraph::AddTransition(const FName& FromState, const FName& ToState, EAnimTransitionRule Rule, const FName& ParameterName, int32 Priority, float BlendDuration, float FloatThreshold)
{
    return AddTransitionInternal(FromState, ToState, Rule, ParameterName, Priority, BlendDuration, FloatThreshold, false);
}

bool UAnimationGraph::AddAnyStateTransition(const FName& ToState, EAnimTransitionRule Rule, const FName& ParameterName, int32 Priority, float BlendDuration, float FloatThreshold)
{
    return AddTransitionInternal(NAME_None, ToState, Rule, ParameterName, Priority, BlendDuration, FloatThreshold, true);
}

bool UAnimationGraph::IsAnimationGraphDataValid() const
{
    if (!bSerializationValid || !Skeleton || States.IsEmpty() || FindStateIndex(EntryStateName) == -1)
    {
        return false;
    }

    for (int32 Index = 0; Index < Parameters.Num(); ++Index)
    {
        const FAnimGraphParameterDefinition& Parameter = Parameters[Index];

        if (Parameter.Name.IsNone())
        {
            return false;
        }

        for (int32 OtherIndex = Index + 1; OtherIndex < Parameters.Num(); ++OtherIndex)
        {
            if (Parameter.Name == Parameters[OtherIndex].Name)
            {
                return false;
            }
        }

        if (Parameter.Type == EAnimGraphParameterType::Float && !std::isfinite(Parameter.FloatDefaultValue))
        {
            return false;
        }

        if (Parameter.Type != EAnimGraphParameterType::Bool && Parameter.Type != EAnimGraphParameterType::Float)
        {
            return false;
        }
    }

    for (int32 Index = 0; Index < States.Num(); ++Index)
    {
        const FAnimGraphStateDefinition& State = States[Index];

        if (State.Name.IsNone() || !std::isfinite(State.PlayRate))
        {
            return false;
        }

        for (int32 OtherIndex = Index + 1; OtherIndex < States.Num(); ++OtherIndex)
        {
            if (State.Name == States[OtherIndex].Name)
            {
                return false;
            }
        }

        if (State.NodeType == EAnimGraphStateNodeType::SequencePlayer)
        {
            if (!State.Sequence || State.BlendSpace ||  State.Sequence->GetSkeleton() != Skeleton || !State.BlendParameterName.IsNone())
            {
                return false;
            }
        }
        else if (State.NodeType == EAnimGraphStateNodeType::BlendSpacePlayer)
        {
            if (State.Sequence || !State.BlendSpace || State.BlendSpace->GetSkeleton() != Skeleton || !State.BlendSpace->IsBlendSpaceDataValid())
            {
                return false;
            }

            if (!State.BlendParameterName.IsNone() && !HasParameterType(State.BlendParameterName, EAnimGraphParameterType::Float))
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    for (const FAnimGraphTransitionDefinition& Transition : Transitions)
    {
        if (FindStateIndex(Transition.ToState) == -1 || (!Transition.bAnyState && FindStateIndex(Transition.FromState) == -1) || !std::isfinite(Transition.BlendDuration) || Transition.BlendDuration < 0.0f || !std::isfinite(Transition.FloatThreshold))
        {
            return false;
        }

        switch (Transition.Rule)
        {
        case EAnimTransitionRule::Always:
        case EAnimTransitionRule::StateFinished:
            break;
        case EAnimTransitionRule::BoolTrue:
        case EAnimTransitionRule::BoolFalse:
            if (!HasParameterType(Transition.ParameterName, EAnimGraphParameterType::Bool))
            {
                return false;
            }
            break;
        case EAnimTransitionRule::FloatGreaterThan:
        case EAnimTransitionRule::FloatLessThan:
        case EAnimTransitionRule::FloatGreaterOrEqual:
        case EAnimTransitionRule::FloatLessOrEqual:
            if (!HasParameterType(Transition.ParameterName, EAnimGraphParameterType::Float))
            {
                return false;
            }
            break;
        default:
            return false;
        }
    }

    return true;
}

void UAnimationGraph::Serialize(FStructuredArchive::FRecord Record)
{
    Super::Serialize(Record);
    const bool bIsLoading = Record.GetUnderlyingArchive().IsLoading();
    if (bIsLoading)
    {
        bSerializationValid = true;
        Parameters.Empty();
        States.Empty();
        Transitions.Empty();
    }

    UObject* SkeletonObject = Skeleton;
    Record << SA_VALUE("Skeleton", SkeletonObject);
    if (bIsLoading)
    {
        Skeleton = Cast<USkeleton>(SkeletonObject);
        if (!Skeleton)
        {
            bSerializationValid = false;
        }
    }
    Record << SA_VALUE("EntryState", EntryStateName);
    int32 ParameterCount = Parameters.Num();

    FStructuredArchive::FArray ParameterArray =
        Record.EnterArray("Parameters", ParameterCount);

    for (int32 Index = 0; Index < ParameterCount; ++Index)
    {
        FStructuredArchive::FRecord ParameterRecord =
            ParameterArray.EnterElement().EnterRecord();

        FAnimGraphParameterDefinition LoadedParameter;

        FAnimGraphParameterDefinition& Parameter =
            bIsLoading ? LoadedParameter : Parameters[Index];

        uint8 TypeValue = static_cast<uint8>(Parameter.Type);

        ParameterRecord << SA_VALUE("Name", Parameter.Name);
        ParameterRecord << SA_VALUE("Type", TypeValue);
        ParameterRecord << SA_VALUE("BoolDefault", Parameter.BoolDefaultValue);
        ParameterRecord << SA_VALUE("FloatDefault", Parameter.FloatDefaultValue);

        if (bIsLoading)
        {
            Parameter.Type = static_cast<EAnimGraphParameterType>(TypeValue);
            Parameters.Add(std::move(LoadedParameter));
        }
    }

    int32 StateCount = States.Num();
    FStructuredArchive::FArray StateArray = Record.EnterArray("States", StateCount);

    for (int32 Index = 0; Index < StateCount; ++Index)
    {
        FStructuredArchive::FRecord StateRecord = StateArray.EnterElement().EnterRecord();

        FAnimGraphStateDefinition LoadedState;

        FAnimGraphStateDefinition& State = bIsLoading ? LoadedState : States[Index];

        uint8 NodeTypeValue = static_cast<uint8>(State.NodeType);

        UObject* SequenceObject = State.Sequence;
        UObject* BlendSpaceObject = State.BlendSpace;

        StateRecord << SA_VALUE("Name", State.Name);
        StateRecord << SA_VALUE("NodeType", NodeTypeValue);
        StateRecord << SA_VALUE("Sequence", SequenceObject);
        StateRecord << SA_VALUE("BlendSpace", BlendSpaceObject);
        StateRecord << SA_VALUE("BlendParameterName", State.BlendParameterName);
        StateRecord << SA_VALUE("PlayRate", State.PlayRate);
        StateRecord << SA_VALUE("Looping", State.bLooping);

        if (bIsLoading)
        {
            State.NodeType = static_cast<EAnimGraphStateNodeType>(NodeTypeValue);

            State.Sequence = Cast<UAnimSequence>(SequenceObject);
            State.BlendSpace = Cast<UBlendSpace1D>(BlendSpaceObject);

            if ((SequenceObject && !State.Sequence) || (BlendSpaceObject && !State.BlendSpace))
            {
                bSerializationValid = false;
            }

            States.Add(std::move(LoadedState));
        }
    }

    int32 TransitionCount = Transitions.Num();

    FStructuredArchive::FArray TransitionArray = Record.EnterArray("Transitions", TransitionCount);

    for (int32 Index = 0; Index < TransitionCount; ++Index)
    {
        FStructuredArchive::FRecord TransitionRecord = TransitionArray.EnterElement().EnterRecord();

        FAnimGraphTransitionDefinition LoadedTransition;

        FAnimGraphTransitionDefinition& Transition = bIsLoading ? LoadedTransition : Transitions[Index];

        uint8 RuleValue = static_cast<uint8>(Transition.Rule);

        TransitionRecord << SA_VALUE("FromState", Transition.FromState);
        TransitionRecord << SA_VALUE("ToState", Transition.ToState);
        TransitionRecord << SA_VALUE("ParameterName", Transition.ParameterName);
        TransitionRecord << SA_VALUE("Rule", RuleValue);
        TransitionRecord << SA_VALUE("Priority", Transition.Priority);
        TransitionRecord << SA_VALUE("BlendDuration", Transition.BlendDuration);
        TransitionRecord << SA_VALUE("FloatThreshold", Transition.FloatThreshold);
        TransitionRecord << SA_VALUE("AnyState", Transition.bAnyState);

        if (bIsLoading)
        {
            Transition.Rule = static_cast<EAnimTransitionRule>(RuleValue);

            Transitions.Add(std::move(LoadedTransition));
        }
    }

    if (bIsLoading && !IsAnimationGraphDataValid())
    {
        bSerializationValid = false;
    }
}

int32 UAnimationGraph::FindStateIndex(const FName& StateName) const
{
    for (int32 Index = 0; Index < States.Num(); ++Index)
    {
        if (States[Index].Name == StateName)
        {
            return Index;
        }
    }
    return -1;
}

int32 UAnimationGraph::FindParameterIndex(const FName& ParameterName) const
{
    for (int32 Index = 0; Index < Parameters.Num(); ++Index)
    {
        if (Parameters[Index].Name == ParameterName)
        {
            return Index;
        }
    }
    return -1;
}

bool UAnimationGraph::HasParameterType(const FName& ParameterName, EAnimGraphParameterType Type) const
{
    const int32 Index = FindParameterIndex(ParameterName);
    return Index != -1 && Parameters[Index].Type == Type;
}

bool UAnimationGraph::AddTransitionInternal(const FName& FromState, const FName& ToState, EAnimTransitionRule Rule, const FName& ParameterName, int32 Priority, float BlendDuration, float FloatThreshold, bool bAnyState)
{
    if (FindStateIndex(ToState) == -1 || (!bAnyState && FindStateIndex(FromState) == -1) || !std::isfinite(BlendDuration) || BlendDuration < 0.0f || !std::isfinite(FloatThreshold))
    {
        return false;
    }
    switch (Rule)
    {
    case EAnimTransitionRule::Always:
    case EAnimTransitionRule::StateFinished:
        break;
    case EAnimTransitionRule::BoolTrue:
    case EAnimTransitionRule::BoolFalse:
        if (!HasParameterType(ParameterName, EAnimGraphParameterType::Bool))
        {
            return false;
        }
        break;
    case EAnimTransitionRule::FloatGreaterThan:
    case EAnimTransitionRule::FloatLessThan:
    case EAnimTransitionRule::FloatGreaterOrEqual:
    case EAnimTransitionRule::FloatLessOrEqual:
        if (!HasParameterType(ParameterName, EAnimGraphParameterType::Float))
        {
            return false;
        }
        break;
    default:
        return false;
    }
    FAnimGraphTransitionDefinition Transition;
    Transition.FromState = FromState;
    Transition.ToState = ToState;
    Transition.ParameterName = ParameterName;
    Transition.Rule = Rule;
    Transition.Priority = Priority;
    Transition.BlendDuration = BlendDuration;
    Transition.FloatThreshold = FloatThreshold;
    Transition.bAnyState = bAnyState;

    Transitions.Add(std::move(Transition));
    return true;
}
