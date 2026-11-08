#include "EnginePCH.h"
#include "AnimNode_StateMachine.h"

bool FAnimNode_StateMachine::AddState(const FName& StateName, FAnimNode_Base* StateNode)
{
	if (StateName.IsNone() || !StateNode)
	{
		return false;
	}
	if (FindStateIndex(StateName) != -1)
	{
		return false;
	}
	FAnimStateEntry Entry;
	Entry.Name = StateName;
	Entry.Node = StateNode;
	States.Add(Entry);
	return true;
}

bool FAnimNode_StateMachine::SetState(const FName& StateName, bool bResetPlayback)
{
	const int32 NewIndex = FindStateIndex(StateName);
	if (NewIndex == -1)
	{
		return false;
	}
	CurrentStateIndex = NewIndex;
	if (bResetPlayback)
	{
		if (IAnimPlaybackControl* Playback = GetPlaybackControl())
		{
			const float StartTime = Playback->GetPlayRate() < 0.0f ? Playback->GetPlayLength() : 0.0f;
			Playback->SetPlaybackPosition(StartTime);
		}
	}
	return true;
}

FName FAnimNode_StateMachine::GetCurrentStateName() const
{
	if (!States.IsValidIndex(CurrentStateIndex))
	{
		return NAME_None;
	}
	return States[CurrentStateIndex].Name;
}

bool FAnimNode_StateMachine::IsCurrentStateFinished() const
{
	const IAnimPlaybackControl* Playback = GetPlaybackControl();
	if (!Playback || Playback->IsLooping())
	{
		return false;
	}

	const float PlayLength = Playback->GetPlayLength();
	if (PlayLength <= 0.0f)
	{
		return false;
	}
	const float CurrentTime = Playback->GetCurrentTime();
	const float PlayRate = Playback->GetPlayRate();

	if (PlayRate > 0.0f)
	{
		return CurrentTime >= PlayLength;
	}

	if (PlayRate < 0.0f)
	{
		return CurrentTime <= 0.0f;
	}

	return false;
}

void FAnimNode_StateMachine::Update(const FAnimationUpdateContext& Context)
{
	FAnimNode_Base* ActiveNode = GetActiveStateNode();

	if (ActiveNode)
	{
		ActiveNode->Update(Context);
	}
}

void FAnimNode_StateMachine::Evaluate(FPoseContext& Output)
{
	FAnimNode_Base* ActiveNode = GetActiveStateNode();

	if (ActiveNode)
	{
		ActiveNode->Evaluate(Output);
	}
}

IAnimPlaybackControl* FAnimNode_StateMachine::GetPlaybackControl()
{
	FAnimNode_Base* ActiveNode = GetActiveStateNode();
	return ActiveNode ? ActiveNode->GetPlaybackControl() : nullptr;
}

const IAnimPlaybackControl* FAnimNode_StateMachine::GetPlaybackControl() const
{
	const FAnimNode_Base* ActiveNode = GetActiveStateNode();
	return ActiveNode ? ActiveNode->GetPlaybackControl() : nullptr;
}

int32 FAnimNode_StateMachine::FindStateIndex(const FName& StateName) const
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

FAnimNode_Base* FAnimNode_StateMachine::GetActiveStateNode()
{
	if (!States.IsValidIndex(CurrentStateIndex))
	{
		return nullptr;
	}
	return States[CurrentStateIndex].Node;
}

const FAnimNode_Base* FAnimNode_StateMachine::GetActiveStateNode() const
{
	if (!States.IsValidIndex(CurrentStateIndex))
	{
		return nullptr;
	}
	return States[CurrentStateIndex].Node;
}
