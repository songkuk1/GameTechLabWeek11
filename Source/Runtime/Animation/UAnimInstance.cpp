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
	SeqPlayer2->InitializeNode(true, "Models/statemachinefbx/Mixamo_StateMachine_WithSkin.fbx_Anim_5");
	AnimNode* Sequence3 = AddNode(SequencePlayer);
	SequencePlayerNode* SeqPlayer3 = static_cast<SequencePlayerNode*>(Sequence3);
	SeqPlayer3->InitializeNode(true, "Models/statemachinefbx/Mixamo_StateMachine_WithSkin.fbx_Anim_4");

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
	SequencePlayerNode* JumpPlayer = static_cast<SequencePlayerNode*>(AddNode(SequencePlayer));
	JumpPlayer->InitializeNode(false, "Models/statemachinefbx/Mixamo_StateMachine_WithSkin.fbx_Anim_3");

	StateMachineNode* Machine = static_cast<StateMachineNode*>(AddNode(NodeType::StateMachine));
	Machine->SetStateNode(EAnimState::Locomotion, Blend1Node);
	Machine->SetStateNode(EAnimState::Attack, AttackNode);
	Machine->SetStateNode(EAnimState::Hit, HitNode);
	Machine->SetStateNode(EAnimState::Jump, JumpPlayer);

	SetRoot(Machine);
}

UAnimationInstance::~UAnimationInstance()
{
	Root = nullptr;
}

void UAnimationInstance::Tick(float deltaTime, PoseContext& Context)
{
	if (!Root) return;

	AnimationContext AnimContext{ deltaTime, Speed, JumpTime, IsAttacking, IsHit, IsJumping };
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
