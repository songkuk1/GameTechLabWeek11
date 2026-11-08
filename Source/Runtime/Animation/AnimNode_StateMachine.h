#pragma once

#include "AnimNode_Base.h"
#include "Containers/Array.h"
#include "Core/NameTypes.h"

struct FAnimNode_StateMachine : public FAnimNode_Base
{
public:
	bool AddState(const FName& StateName, FAnimNode_Base* StateNode);
	bool SetState(const FName& StateName, bool bResetPlayback = true);
	FName GetCurrentStateName() const;
	bool IsCurrentStateFinished() const;
	virtual void Update(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate(FPoseContext& Output) override;
	IAnimPlaybackControl* GetPlaybackControl() override;
	const IAnimPlaybackControl* GetPlaybackControl() const override;
private:
	struct FAnimStateEntry
	{
		FName Name;
		FAnimNode_Base* Node = nullptr;
	};
	int32 FindStateIndex(const FName& StateName) const;
	FAnimNode_Base* GetActiveStateNode();
	const FAnimNode_Base* GetActiveStateNode() const;
	TArray<FAnimStateEntry> States;
	int32 CurrentStateIndex = -1;
};