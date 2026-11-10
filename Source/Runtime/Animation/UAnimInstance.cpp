#include "EnginePCH.h"
#include "UAnimInstance.h"
#include "SequencePlayerNode.h"
#include "TwoWayBlendNode.h"
#include "BlendSpace1DNode.h"
#include "StateMachineNode.h"


UAnimationInstance::UAnimationInstance()
{
	BuildDefaultLocomotionGraph();
}

UAnimationInstance::~UAnimationInstance()
{
	Root = nullptr;
}

void UAnimationInstance::ClearGraph()
{
	Root = nullptr;
	Nodes.Empty();
	PendingNotifyNames.Reset();
	RootTransform = FTransform::Identity;
	RootMotionNodeIndex = -1;
	CurrentTime = 0.0f;
}

void UAnimationInstance::BuildDefaultLocomotionGraph()
{
	ClearGraph();

	SequencePlayerNode* SeqPlayer1 = AddSequenceNode("Models/statemachinefbx/Mixamo_StateMachine_WithSkin.fbx_Anim_2", true, false);
	SequencePlayerNode* SeqPlayer2 = AddSequenceNode("Models/statemachinefbx/Mixamo_StateMachine_WithSkin.fbx_Anim_7", true, false);
	SequencePlayerNode* SeqPlayer3 = AddSequenceNode("Models/statemachinefbx/Mixamo_StateMachine_WithSkin.fbx_Anim_6", true, false);

	BlendSpace1DNode* Blend1Node = AddBlendSpace1DNode();
	Blend1Node->AddSample(SeqPlayer1, 0.0f);
	Blend1Node->AddSample(SeqPlayer2, 2.0f);
	Blend1Node->AddSample(SeqPlayer3, 10.0f);

	// 공격/피격 클립은 준비 전까지 기존 클립을 임시로 사용한다.
	SequencePlayerNode* AttackNode = AddSequenceNode("Models/statemachinefbx/Mixamo_StateMachine_WithSkin.fbx_Anim_8", false, false);
	SequencePlayerNode* HitNode = AddSequenceNode("Models/statemachinefbx/Mixamo_StateMachine_WithSkin.fbx_Anim_1", false, false);
	SequencePlayerNode* JumpStartNode = AddSequenceNode("Models/statemachinefbx/Mixamo_StateMachine_WithSkin.fbx_Anim_5", false, false);
	SequencePlayerNode* JumpAirNode = AddSequenceNode("Models/statemachinefbx/Mixamo_StateMachine_WithSkin.fbx_Anim_4", true, false);
	SequencePlayerNode* JumpEndNode = AddSequenceNode("Models/statemachinefbx/Mixamo_StateMachine_WithSkin.fbx_Anim_3", false, false);

	StateMachineNode* Machine = AddStateMachineNode();
	Machine->AddTransition(EAnimState::Locomotion, EAnimState::Attack,
		[](const AnimationContext& Context) { return Context.bIsAttacking; }, 0.5f,
		[AttackNode](AnimationContext& Context)
		{
			Context.bIsAttacking = false;
			AttackNode->ResetAnimation();
		});
	Machine->AddTransition(EAnimState::Locomotion, EAnimState::Hit,
		[](const AnimationContext& Context) { return Context.bIsHit; }, 0.5f,
		[HitNode](AnimationContext& Context)
		{
			Context.bIsHit = false;
			HitNode->ResetAnimation();
		});
	Machine->AddTransition(EAnimState::Hit, EAnimState::Locomotion,
		[HitNode](const AnimationContext&) { return HitNode->IsClipFinished(); }, 0.5f);
	Machine->AddTransition(EAnimState::Attack, EAnimState::Locomotion,
		[AttackNode](const AnimationContext&) { return AttackNode->IsClipFinished(); }, 0.5f);

	Machine->AddTransition(EAnimState::Locomotion, EAnimState::JumpStart,
		[](const AnimationContext& Context) { return Context.bIsJumpStart; }, 0.15f,
		[JumpStartNode](AnimationContext&) { JumpStartNode->ResetAnimation(); });
	Machine->AddTransition(EAnimState::Locomotion, EAnimState::JumpAir,
		[](const AnimationContext& Context) { return Context.bIsJumpAir; }, 0.15f,
		[JumpAirNode](AnimationContext&) { JumpAirNode->ResetAnimation(); });

	// 착지가 Start 재생 완료보다 우선한다.
	Machine->AddTransition(EAnimState::JumpStart, EAnimState::JumpEnd,
		[](const AnimationContext& Context) { return Context.bIsJumpEnd; }, 0.15f,
		[JumpEndNode](AnimationContext&) { JumpEndNode->ResetAnimation(); });
	Machine->AddTransition(EAnimState::JumpStart, EAnimState::JumpAir,
		[JumpStartNode](const AnimationContext&) { return JumpStartNode->IsClipFinished(); }, 0.15f,
		[JumpAirNode](AnimationContext&) { JumpAirNode->ResetAnimation(); });
	Machine->AddTransition(EAnimState::JumpAir, EAnimState::JumpEnd,
		[](const AnimationContext& Context) { return Context.bIsJumpEnd; }, 0.15f,
		[JumpEndNode](AnimationContext&) { JumpEndNode->ResetAnimation(); });
	Machine->AddTransition(EAnimState::JumpEnd, EAnimState::Locomotion,
		[JumpEndNode](const AnimationContext& Context)
		{
			return JumpEndNode->IsClipFinished()
				|| (JumpEndNode->GetCurrentPlayTime() >= 0.25f
					&& Context.Speed > 0.1f);
		}, 0.15f);

	Machine->SetStateNode(EAnimState::Locomotion, Blend1Node);
	Machine->SetStateNode(EAnimState::Attack, AttackNode);
	Machine->SetStateNode(EAnimState::Hit, HitNode);
	Machine->SetStateNode(EAnimState::JumpStart, JumpStartNode);
	Machine->SetStateNode(EAnimState::JumpAir, JumpAirNode);
	Machine->SetStateNode(EAnimState::JumpEnd, JumpEndNode);

	SetRoot(Machine);
}

void UAnimationInstance::Tick(float deltaTime, PoseContext& Context)
{
	if (!Root) return;

	PendingNotifyNames.Reset();
	RootTransform = FTransform::Identity;
	RootMotionNodeIndex = -1;
	AnimationContext AnimContext{ PendingNotifyNames, RootTransform, deltaTime, Speed, IsAttacking, IsHit, IsJumpStart, IsJumpAir, IsJumpEnd, RootMotionNodeIndex };
	CurrentTime += deltaTime;
	Root->Update(AnimContext);
	Root->EvaluatePose(Context);
}

AnimNode* UAnimationInstance::AddNode(NodeType Node)
{
	if (Node == NodeType::SequencePlayer)
	{
		auto NewNode = std::make_unique<SequencePlayerNode>();
		AnimNode* RawNode = NewNode.get();
		Nodes.Add(std::move(NewNode));
		return RawNode;
	}
	else if (Node == NodeType::TwoWayBlend)
	{
		auto NewNode = std::make_unique<TwoWayBlendNode>();
		AnimNode* RawNode = NewNode.get();
		Nodes.Add(std::move(NewNode));
		return RawNode;
	}
	else if (Node == NodeType::BlendSpace1D)
	{
		auto NewNode = std::make_unique<BlendSpace1DNode>();
		AnimNode* RawNode = NewNode.get();
		Nodes.Add(std::move(NewNode));
		return RawNode;
	}
	else if (Node == NodeType::StateMachine)
	{
		auto NewNode = std::make_unique<StateMachineNode>();
		AnimNode* RawNode = NewNode.get();
		Nodes.Add(std::move(NewNode));
		return RawNode;
	}
	return nullptr;
}

SequencePlayerNode* UAnimationInstance::AddSequenceNode(const FString& Path, bool bLoop, bool bReverse)
{
	auto* Node = static_cast<SequencePlayerNode*>(AddNode(NodeType::SequencePlayer));
	Node->InitializeNode(bLoop, bReverse, Path);
	return Node;
}

SequencePlayerNode* UAnimationInstance::AddSequenceNode(UAnimSequence* InSequence, bool bLoop, bool bReverse)
{
	auto* Node = static_cast<SequencePlayerNode*>(AddNode(NodeType::SequencePlayer));
	Node->InitializeNode(bLoop, bReverse, InSequence);
	return Node;
}

TwoWayBlendNode* UAnimationInstance::AddTwoWayBlendNode(AnimNode* NodeA, AnimNode* NodeB, float Alpha)
{
	auto* Node = static_cast<TwoWayBlendNode*>(AddNode(NodeType::TwoWayBlend));
	Node->InitializeNode(NodeA, NodeB, Alpha);
	return Node;
}

BlendSpace1DNode* UAnimationInstance::AddBlendSpace1DNode()
{
	return static_cast<BlendSpace1DNode*>(AddNode(NodeType::BlendSpace1D));
}

StateMachineNode* UAnimationInstance::AddStateMachineNode()
{
	return static_cast<StateMachineNode*>(AddNode(NodeType::StateMachine));
}

SequencePlayerNode* UAnimationInstance::PlaySingleAnimation(const FString& Path, bool bLoop, bool bReverse)
{
	ClearGraph();
	SequencePlayerNode* Node = AddSequenceNode(Path, bLoop, bReverse);
	SetRoot(Node);
	return Node;
}

SequencePlayerNode* UAnimationInstance::PlaySingleAnimation(UAnimSequence* InSequence, bool bLoop, bool bReverse)
{
	ClearGraph();
	SequencePlayerNode* Node = AddSequenceNode(InSequence, bLoop, bReverse);
	SetRoot(Node);
	return Node;
}

void UAnimationInstance::RemoveNode(AnimNode* Node)
{
	if (!Node)
		return;

	if (Root == Node)
		Root = nullptr;

	for (int32 i = 0; i < Nodes.Num(); ++i)
	{
		if (Nodes[i].get() == Node)
		{
			Nodes.RemoveAt(i);
			break;
		}
	}
}

void UAnimationInstance::SetRoot(AnimNode* Node)
{
	Root = Node;
}
