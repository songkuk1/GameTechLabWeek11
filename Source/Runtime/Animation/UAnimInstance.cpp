#include "EnginePCH.h"
#include "UAnimInstance.h"
#include "SequencePlayerNode.h"
#include "TwoWayBlendNode.h"
#include "BlendSpace1DNode.h"
#include "StateMachineNode.h"


UAnimationInstance::UAnimationInstance()
{
	AnimNode* Sequence1 = AddNode(SequencePlayer);
	SequencePlayerNode* SeqPlayer1 = static_cast<SequencePlayerNode*>(Sequence1);
	SeqPlayer1->InitializeNode(true, "Models/statemachinefbx/Mixamo_StateMachine_WithSkin.fbx_Anim_2");
	AnimNode* Sequence2 = AddNode(SequencePlayer);
	SequencePlayerNode* SeqPlayer2 = static_cast<SequencePlayerNode*>(Sequence2);
	SeqPlayer2->InitializeNode(true, "Models/statemachinefbx/Mixamo_StateMachine_WithSkin.fbx_Anim_7");
	AnimNode* Sequence3 = AddNode(SequencePlayer);
	SequencePlayerNode* SeqPlayer3 = static_cast<SequencePlayerNode*>(Sequence3);
	SeqPlayer3->InitializeNode(true, "Models/statemachinefbx/Mixamo_StateMachine_WithSkin.fbx_Anim_6");

	AnimNode* Blends = AddNode(BlendSpace1D);
	BlendSpace1DNode* Blend1Node = static_cast<BlendSpace1DNode*>(Blends);
	Blend1Node->AddSample(Sequence1, 0.0f);
	Blend1Node->AddSample(Sequence2, 2.0f);
	Blend1Node->AddSample(Sequence3, 10.0f);
	// 공격/피격 클립은 준비 전까지 기존 클립을 임시로 사용한다.
	AnimNode* AttackNode = AddNode(SequencePlayer);
	static_cast<SequencePlayerNode*>(AttackNode)->InitializeNode(false, "Models/statemachinefbx/Mixamo_StateMachine_WithSkin.fbx_Anim_0");
	AnimNode* HitNode = AddNode(SequencePlayer);
	static_cast<SequencePlayerNode*>(HitNode)->InitializeNode(false, "Models/statemachinefbx/Mixamo_StateMachine_WithSkin.fbx_Anim_1");
	AnimNode* JumpStartNode = AddNode(SequencePlayer);
	static_cast<SequencePlayerNode*>(JumpStartNode)->InitializeNode(false, "Models/statemachinefbx/Mixamo_StateMachine_WithSkin.fbx_Anim_5");
	AnimNode* JumpAirNode = AddNode(SequencePlayer);
	static_cast<SequencePlayerNode*>(JumpAirNode)->InitializeNode(true, "Models/statemachinefbx/Mixamo_StateMachine_WithSkin.fbx_Anim_4");
	AnimNode* JumpEndNode = AddNode(SequencePlayer);
	static_cast<SequencePlayerNode*>(JumpEndNode)->InitializeNode(false, "Models/statemachinefbx/Mixamo_StateMachine_WithSkin.fbx_Anim_3");



	StateMachineNode* Machine = static_cast<StateMachineNode*>(AddNode(NodeType::StateMachine));
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
			auto* Player = static_cast<SequencePlayerNode*>(JumpEndNode);

			return Player->IsClipFinished()
				|| (Player->GetCurrentPlayTime() >= 0.25f
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

UAnimationInstance::~UAnimationInstance()
{
	Root = nullptr;
}

void UAnimationInstance::Tick(float deltaTime, PoseContext& Context)
{
	if (!Root) return;

	PendingNotifyNames.Reset();
	AnimationContext AnimContext{ PendingNotifyNames, deltaTime, Speed, IsAttacking, IsHit, IsJumpStart, IsJumpAir, IsJumpEnd };
	CurrentTime += deltaTime;
	Root->Update(AnimContext);
	Root->EvaluatePose(Context);
}

AnimNode* UAnimationInstance::AddNode(NodeType Node)
{
	if (Node == NodeType::SequencePlayer)
	{
		auto Node = std::make_unique<SequencePlayerNode>();
		AnimNode* RawNode = Node.get();
		Nodes.Add(std::move(Node));
		return RawNode;
	}
	else if (Node == NodeType::TwoWayBlend)
	{
		auto Node = std::make_unique<TwoWayBlendNode>();
		AnimNode* RawNode = Node.get();
		Nodes.Add(std::move(Node));
		return RawNode;
	}
	else if (Node == NodeType::BlendSpace1D)
	{
		auto Node = std::make_unique<BlendSpace1DNode>();
		AnimNode* RawNode = Node.get();
		Nodes.Add(std::move(Node));
		return RawNode;
	}
	else if (Node == NodeType::StateMachine)
	{
		auto Node = std::make_unique<StateMachineNode>();
		AnimNode* RawNode = Node.get();
		Nodes.Add(std::move(Node));
		return RawNode;
	}
}

void UAnimationInstance::RemoveNode(AnimNode* Node)
{

}

void UAnimationInstance::SetRoot(AnimNode* Node)
{
	Root = Node;
}
