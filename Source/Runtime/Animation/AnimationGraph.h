#pragma once

#include "Animation/AnimationAsset.h"
#include "Animation/AnimNode_StateMachine.h"
#include "Containers/Array.h"
#include "Serialization/StructuredArchive.h"
#include "AnimationGraph.generated.h"

class UAnimSequence;
class UBlendSpace1D;
class USkeleton;

enum class EAnimGraphParameterType : uint8
{
    Bool,
    Float
};

enum class EAnimGraphStateNodeType : uint8
{
    SequencePlayer,
    BlendSpacePlayer
};

struct FAnimGraphParameterDefinition
{
    FName Name;
    EAnimGraphParameterType Type = EAnimGraphParameterType::Bool;
    bool BoolDefaultValue = false;
    float FloatDefaultValue = 0.0f;
};

struct FAnimGraphStateDefinition
{
    FName Name;
    EAnimGraphStateNodeType NodeType = EAnimGraphStateNodeType::SequencePlayer;
    UAnimSequence* Sequence = nullptr;
    UBlendSpace1D* BlendSpace = nullptr;
    FName BlendParameterName = NAME_None;
    float PlayRate = 1.0f;
    bool bLooping = true;
};

struct FAnimGraphTransitionDefinition
{
    FName FromState;
    FName ToState;
    FName ParameterName;
    EAnimTransitionRule Rule = EAnimTransitionRule::Always;
    int32 Priority = 0;
    float BlendDuration = 0.0f;
    float FloatThreshold = 0.0f;
    bool bAnyState = false;
};

struct FAnimGraphEditorNodePosition
{
    FName NodeName = NAME_None;
    float X = 0.0f;
    float Y = 0.0f;
};

UCLASS()
class UAnimationGraph : public UAnimationAsset
{
    GENERATED_BODY()
public:
    bool SetSkeleton(USkeleton* InSkeleton);
    USkeleton* GetSkeleton() const { return Skeleton; }
    bool AddBoolParameter(const FName& Name, bool DefaultValue = false);
    bool AddFloatParameter(const FName& Name, float DefaultValue = 0.0f);
    bool AddSequenceState(const FName& StateName, UAnimSequence* Sequence, float PlayRate = 1.0f, bool bLooping = true);
    bool AddBlendSpaceState(const FName& StateName, UBlendSpace1D* BlendSpace, const FName& BlendParameterName = NAME_None, float PlayRate = 1.0f, bool bLooping = true);
    bool SetEntryState(const FName& StateName);
    FName GetEntryState() const { return EntryStateName; }
    bool AddTransition(const FName& FromState, const FName& ToState, EAnimTransitionRule Rule, const FName& ParameterName = NAME_None, int32 Priority = 0, float BlendDuration = 0.0f, float FloatThreshold = 0.0f);
    bool AddAnyStateTransition(const FName& ToState, EAnimTransitionRule Rule, const FName& ParameterName = NAME_None, int32 Priority = 0, float BlendDuration = 0.0f, float FloatThreshold = 0.0f);
    const TArray<FAnimGraphParameterDefinition>& GetParameters() const { return Parameters; }
    const TArray<FAnimGraphStateDefinition>& GetStates() const { return States; }
    const TArray<FAnimGraphTransitionDefinition>& GetTransitions() const { return Transitions; }
    bool SetEditorNodePosition(const FName& NodeName, float X, float Y);
    bool GetEditorNodePosition(const FName& NodeName, float& OutX, float& OutY) const;
    void ClearEditorNodePositions();
    bool IsAnimationGraphDataValid() const;

    // Parameter
    bool RenameParameter(const FName& OldName, const FName& NewName);
    bool RemoveParameter(const FName& Name);
    bool SetBoolParameterDefault(const FName& Name, bool DefaultValue);
    bool SetFloatParameterDefault(const FName& Name, float DefaultValue);

    // State
    bool RenameState(const FName& OldName, const FName& NewName);
    bool RemoveState(const FName& StateName);
    bool SetSequenceState(const FName& StateName, UAnimSequence* Sequence, float PlayRate = 1.0f, bool bLooping = true);
    bool SetBlendSpaceState( const FName& StateName, UBlendSpace1D* BlendSpace, const FName& BlendParameterName = NAME_None, float PlayRate = 1.0f, bool bLooping = true);

    // Transition
    bool SetTransition(int32 Index, const FAnimGraphTransitionDefinition& Transition);
    bool RemoveTransition(int32 Index);

    virtual void Serialize(FStructuredArchive::FRecord Record) override;
private:
    int32 FindStateIndex(const FName& StateName) const;
    int32 FindParameterIndex(const FName& ParameterName) const;
    bool HasParameterType(const FName& ParameterName, EAnimGraphParameterType Type) const;
    bool AddTransitionInternal( const FName& FromState, const FName& ToState, EAnimTransitionRule Rule, const FName& ParameterName, int32 Priority, float BlendDuration, float FloatThreshold, bool bAnyState);
    bool IsTransitionDefinitionValid(const FAnimGraphTransitionDefinition& Transition) const;
private:
    USkeleton* Skeleton = nullptr;
    FName EntryStateName = NAME_None;
    TArray<FAnimGraphParameterDefinition> Parameters;
    TArray<FAnimGraphStateDefinition> States;
    TArray<FAnimGraphTransitionDefinition> Transitions;
    TArray<FAnimGraphEditorNodePosition> EditorNodePositions;
    bool bSerializationValid = true;
};