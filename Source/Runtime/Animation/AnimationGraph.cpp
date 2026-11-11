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

bool UAnimationGraph::AddBlendSpaceState(const FName& StateName, UBlendSpace1D* BlendSpace, float PlayRate, bool bLooping)
{
    if (StateName.IsNone() || FindStateIndex(StateName) != -1 || !Skeleton || !BlendSpace || BlendSpace->GetSkeleton() != Skeleton || !BlendSpace->IsBlendSpaceDataValid() || !std::isfinite(PlayRate))
    {
        return false;
    }

    const FName BlendParameterName = BlendSpace->GetAxis().Name;

    if (!EnsureFloatParameter(BlendParameterName))
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

bool UAnimationGraph::SetEditorNodePosition(const FName& NodeName, float X, float Y)
{
    const bool bIsEditorEntryPoint = NodeName == FName("__EditorEntryPoint__");

    if (!std::isfinite(X) || !std::isfinite(Y) || (!NodeName.IsNone() && !bIsEditorEntryPoint && FindStateIndex(NodeName) == -1))
    {
        return false;
    }

    for (FAnimGraphEditorNodePosition& Position : EditorNodePositions)
    {
        if (Position.NodeName == NodeName)
        {
            Position.X = X;
            Position.Y = Y;
            return true;
        }
    }

    FAnimGraphEditorNodePosition Position;
    Position.NodeName = NodeName;
    Position.X = X;
    Position.Y = Y;

    EditorNodePositions.Add(std::move(Position));

    return true;
}

bool UAnimationGraph::GetEditorNodePosition(const FName& NodeName, float& OutX, float& OutY) const
{
    for (const FAnimGraphEditorNodePosition& Position : EditorNodePositions)
    {
        if (Position.NodeName == NodeName && std::isfinite(Position.X) && std::isfinite(Position.Y))
        {
            OutX = Position.X;
            OutY = Position.Y;
            return true;
        }
    }

    return false;
}

void UAnimationGraph::ClearEditorNodePositions()
{
    EditorNodePositions.Empty();
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
        if (!IsTransitionDefinitionValid(Transition))
        {
            return false;
        }
    }

    return true;
}

bool UAnimationGraph::RenameParameter(const FName& OldName, const FName& NewName)
{
    const int32 ParameterIndex = FindParameterIndex(OldName);
    if (ParameterIndex == -1 || NewName.IsNone())
    {
        return false;
    }

    if (OldName == NewName)
    {
        return true;
    }

    if (FindParameterIndex(NewName) != -1)
    {
        return false;
    }
    
    Parameters[ParameterIndex].Name = NewName;
    for (FAnimGraphStateDefinition& State : States)
    {
        if (State.BlendParameterName == OldName)
        {
            State.BlendParameterName = NewName;
        }
    }

    for (FAnimGraphTransitionDefinition& Transition : Transitions)
    {
        if (Transition.ParameterName == OldName)
        {
            Transition.ParameterName = NewName;
        }
    }
    return true;
}

bool UAnimationGraph::RemoveParameter(const FName& Name)
{
    const int32 ParameterIndex = FindParameterIndex(Name);
    if (ParameterIndex == -1)
    {
        return false;
    }

    for (const FAnimGraphStateDefinition& State : States)
    {
        if (State.BlendParameterName == Name)
        {
            return false;
        }
    }
    for (const FAnimGraphTransitionDefinition& Transition : Transitions)
    {
        if (Transition.ParameterName == Name)
        {
            return false;
        }
    }
    Parameters.RemoveAt(static_cast<uint32>(ParameterIndex));
    return true;
}

bool UAnimationGraph::SetBoolParameterDefault(const FName& Name, bool DefaultValue)
{
    const int32 Index = FindParameterIndex(Name);
    if (Index == -1 || Parameters[Index].Type != EAnimGraphParameterType::Bool)
    {
        return false;
    }
    Parameters[Index].BoolDefaultValue = DefaultValue;
    return true;
}

bool UAnimationGraph::SetFloatParameterDefault(const FName& Name, float DefaultValue)
{
    const int32 Index = FindParameterIndex(Name);
    if (Index == -1 || Parameters[Index].Type != EAnimGraphParameterType::Float || !std::isfinite(DefaultValue))
    {
        return false;
    }
    Parameters[Index].FloatDefaultValue = DefaultValue;
    return true;
}

bool UAnimationGraph::RenameState(const FName& OldName, const FName& NewName)
{
    const int32 StateIndex = FindStateIndex(OldName);

    if (StateIndex == -1 || NewName.IsNone())
    {
        return false;
    }

    if (OldName == NewName)
    {
        return true;
    }

    if (FindStateIndex(NewName) != -1)
    {
        return false;
    }

    States[StateIndex].Name = NewName;

    for (FAnimGraphEditorNodePosition& Position : EditorNodePositions)
    {
        if (Position.NodeName == OldName)
        {
            Position.NodeName = NewName;
        }
    }

    if (EntryStateName == OldName)
    {
        EntryStateName = NewName;
    }

    for (FAnimGraphTransitionDefinition& Transition : Transitions)
    {
        if (Transition.FromState == OldName)
        {
            Transition.FromState = NewName;
        }

        if (Transition.ToState == OldName)
        {
            Transition.ToState = NewName;
        }
    }

    return true;
}

bool UAnimationGraph::RemoveState(const FName& StateName)
{
    const int32 StateIndex = FindStateIndex(StateName);

    if (StateIndex == -1)
    {
        return false;
    }

    for (int32 Index = Transitions.Num() - 1; Index >= 0; --Index)
    {
        const FAnimGraphTransitionDefinition& Transition = Transitions[Index];

        const bool bIsSource = Transition.FromState == StateName;
        const bool bIsDestination = Transition.ToState == StateName;

        if (bIsSource || bIsDestination)
        {
            Transitions.RemoveAt(static_cast<uint32>(Index));
        }
    }

    if (EntryStateName == StateName)
    {
        EntryStateName = NAME_None;
    }

    for (int32 Index = EditorNodePositions.Num() - 1; Index >= 0; --Index)
    {
        if (EditorNodePositions[Index].NodeName == StateName)
        {
            EditorNodePositions.RemoveAt(static_cast<uint32>(Index));
        }
    }

    States.RemoveAt(static_cast<uint32>(StateIndex));

    return true;
}

bool UAnimationGraph::SetSequenceState(const FName& StateName, UAnimSequence* Sequence, float PlayRate, bool bLooping)
{
    const int32 Index = FindStateIndex(StateName);

    if (Index == -1 || !Skeleton || !Sequence || Sequence->GetSkeleton() != Skeleton || !std::isfinite(PlayRate))
    {
        return false;
    }

    FAnimGraphStateDefinition& State = States[Index];

    State.NodeType = EAnimGraphStateNodeType::SequencePlayer;
    State.Sequence = Sequence;
    State.BlendSpace = nullptr;
    State.BlendParameterName = NAME_None;
    State.PlayRate = PlayRate;
    State.bLooping = bLooping;

    return true;
}

bool UAnimationGraph::SetBlendSpaceState(const FName& StateName, UBlendSpace1D* BlendSpace, float PlayRate, bool bLooping)
{
    const int32 Index = FindStateIndex(StateName);

    if (Index == -1 || !Skeleton || !BlendSpace || BlendSpace->GetSkeleton() != Skeleton || !BlendSpace->IsBlendSpaceDataValid() || !std::isfinite(PlayRate))
    {
        return false;
    }

    const FName BlendParameterName = BlendSpace->GetAxis().Name;

    if (!EnsureFloatParameter(BlendParameterName))
    {
        return false;
    }

    FAnimGraphStateDefinition& State = States[Index];

    State.NodeType = EAnimGraphStateNodeType::BlendSpacePlayer;
    State.Sequence = nullptr;
    State.BlendSpace = BlendSpace;
    State.BlendParameterName = BlendParameterName;
    State.PlayRate = PlayRate;
    State.bLooping = bLooping;

    return true;
}

bool UAnimationGraph::SetTransition(int32 Index, const FAnimGraphTransitionDefinition& InTransition)
{
    if (!Transitions.IsValidIndex(Index))
    {
        return false;
    }

    FAnimGraphTransitionDefinition NewTransition = InTransition;

    if (NewTransition.bAnyState)
    {
        NewTransition.FromState = NAME_None;
    }

    if (!IsTransitionDefinitionValid(NewTransition))
    {
        return false;
    }

    for (int32 ExistingIndex = 0; ExistingIndex < Transitions.Num(); ++ExistingIndex)
    {
        if (ExistingIndex == Index)
        {
            continue;
        }

        const FAnimGraphTransitionDefinition& Existing = Transitions[ExistingIndex];

        if (Existing.bAnyState == NewTransition.bAnyState && Existing.FromState == NewTransition.FromState && Existing.ToState == NewTransition.ToState)
        {
            return false;
        }
    }

    Transitions[Index] = std::move(NewTransition);

    return true;
}

bool UAnimationGraph::RemoveTransition(int32 Index)
{
    if (!Transitions.IsValidIndex(Index))
    {
        return false;
    }

    Transitions.RemoveAt(static_cast<uint32>(Index));

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
        EditorNodePositions.Empty();
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

    int32 NodePositionCount = EditorNodePositions.Num();

    FStructuredArchive::FArray NodePositionArray = Record.EnterArray("EditorNodePositions", NodePositionCount);

    for (int32 Index = 0; Index < NodePositionCount; ++Index)
    {
        FStructuredArchive::FRecord PositionRecord = NodePositionArray.EnterElement().EnterRecord();

        FAnimGraphEditorNodePosition LoadedPosition;
        FAnimGraphEditorNodePosition& Position = bIsLoading ? LoadedPosition : EditorNodePositions[Index];

        PositionRecord << SA_VALUE("NodeName", Position.NodeName);
        PositionRecord << SA_VALUE("X", Position.X);
        PositionRecord << SA_VALUE("Y", Position.Y);

        if (bIsLoading)
        {
            EditorNodePositions.Add(std::move(LoadedPosition));
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
    FAnimGraphTransitionDefinition Transition;

    Transition.FromState = FromState;
    Transition.ToState = ToState;
    Transition.ParameterName = ParameterName;
    Transition.Rule = Rule;
    Transition.Priority = Priority;
    Transition.BlendDuration = BlendDuration;
    Transition.FloatThreshold = FloatThreshold;
    Transition.bAnyState = bAnyState;

    if (!IsTransitionDefinitionValid(Transition))
    {
        return false;
    }

    for (const FAnimGraphTransitionDefinition& Existing : Transitions)
    {
        if (Existing.bAnyState == bAnyState && Existing.FromState == FromState && Existing.ToState == ToState)
        {
            return false;
        }
    }

    Transitions.Add(std::move(Transition));

    return true;
}

bool UAnimationGraph::IsTransitionDefinitionValid( const FAnimGraphTransitionDefinition& Transition) const
{
    if (Transition.ToState.IsNone() || FindStateIndex(Transition.ToState) == -1 || !std::isfinite(Transition.BlendDuration) || Transition.BlendDuration < 0.0f || !std::isfinite(Transition.FloatThreshold))
    {
        return false;
    }

    if (Transition.bAnyState)
    {
        if (!Transition.FromState.IsNone())
        {
            return false;
        }
    }
    else
    {
        if (Transition.FromState.IsNone() || FindStateIndex(Transition.FromState) == -1 || Transition.FromState == Transition.ToState)
        {
            return false;
        }
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
    return true;
}

bool UAnimationGraph::EnsureFloatParameter(const FName& Name)
{
    if (Name.IsNone())
    {
        return false;
    }

    const int32 Index = FindParameterIndex(Name);

    if (Index != -1)
    {
        return Parameters[Index].Type == EAnimGraphParameterType::Float;
    }

    return AddFloatParameter(Name, 0.0f);
}
