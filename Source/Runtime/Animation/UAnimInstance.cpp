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
	SeqPlayer2->InitializeNode(true, "Models/statemachinefbx/Mixamo_StateMachine_WithSkin.fbx_Anim_4");
	AnimNode* Sequence3 = AddNode(SequencePlayer);
	SequencePlayerNode* SeqPlayer3 = static_cast<SequencePlayerNode*>(Sequence3);
	SeqPlayer3->InitializeNode(true, "Models/statemachinefbx/Mixamo_StateMachine_WithSkin.fbx_Anim_3");
	AnimNode* Blends = AddNode(BlendSpace1D);
	BlendSpace1DNode* Blend1Node = static_cast<BlendSpace1DNode*>(Blends);
	Blend1Node->AddSample(Sequence1, 100);
	Blend1Node->AddSample(Sequence2, 500);
	Blend1Node->AddSample(Sequence3, 900);
	// 공격/피격 클립은 준비 전까지 기존 클립을 임시로 사용한다.
	AnimNode* AttackNode = AddNode(SequencePlayer);
	static_cast<SequencePlayerNode*>(AttackNode)->InitializeNode(false, "Models/statemachinefbx/Mixamo_StateMachine_WithSkin.fbx_Anim_0");
	AnimNode* HitNode = AddNode(SequencePlayer);
	static_cast<SequencePlayerNode*>(HitNode)->InitializeNode(false, "Models/statemachinefbx/Mixamo_StateMachine_WithSkin.fbx_Anim_1");

	StateMachineNode* Machine = static_cast<StateMachineNode*>(AddNode(NodeType::StateMachine));
	Machine->SetStateNode(EAnimState::Locomotion, Blend1Node);
	Machine->SetStateNode(EAnimState::Attack, AttackNode);
	Machine->SetStateNode(EAnimState::Hit, HitNode);

	SetRoot(Machine);
	Speed = 101;
}

UAnimationInstance::~UAnimationInstance()
{
	Root = nullptr;
}

void UAnimationInstance::Tick(float deltaTime, PoseContext& Context)
{
	if (!Root) return;

	AnimationContext AnimContext{ deltaTime, Speed, IsAttacking, IsHit };
	CurrentTime += deltaTime;
	Speed += deltaTime*18;
	BlendSpace1DNode* Blend1Node = static_cast<BlendSpace1DNode*>(Nodes[3].get());
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

/*TwoWayBlendNode* UAnimationInstance::ConnectBlendNode(TwoWayBlendNode Blend, SequencePlayerNode A, SequencePlayerNode B)
{
	Blend.InputA = &A;
	Blend.InputA = &B;
	return &Blend;
}*/

void UAnimationInstance::RemoveNode(AnimNode* Node)
{

}

void UAnimationInstance::SetRoot(AnimNode* Node)
{
	Root = Node;
}