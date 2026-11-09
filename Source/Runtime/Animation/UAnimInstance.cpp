#include "EnginePCH.h"
#include "UAnimInstance.h"
#include "SequencePlayerNode.h"
#include "TwoWayBlendNode.h"
#include "BlendSpace1DNode.h"


UAnimationInstance::UAnimationInstance()
{
	AnimNode* Sequence1 = AddNode(SequencePlayer);
	SequencePlayerNode* SeqPlayer1 = static_cast<SequencePlayerNode*>(Sequence1);
	SeqPlayer1->InitializeNode(true, "Models/walkrun-and-idle/source/notDeadpool.fbx_Anim_1");
	AnimNode* Sequence2 = AddNode(SequencePlayer);
	SequencePlayerNode* SeqPlayer2 = static_cast<SequencePlayerNode*>(Sequence2);
	SeqPlayer2->InitializeNode(true, "Models/walkrun-and-idle/source/notDeadpool.fbx_Anim_2");
	AnimNode* Sequence3 = AddNode(SequencePlayer);
	SequencePlayerNode* SeqPlayer3 = static_cast<SequencePlayerNode*>(Sequence3);
	SeqPlayer3->InitializeNode(true, "Models/walkrun-and-idle/source/notDeadpool.fbx_Anim_4");
	AnimNode* Blends = AddNode(BlendSpace1D);
	BlendSpace1DNode* Blend1Node = static_cast<BlendSpace1DNode*>(Blends);
	Blend1Node->AddSample(Sequence1, 100);
	Blend1Node->AddSample(Sequence2, 500);
	Blend1Node->AddSample(Sequence3, 900);
	Blend1Node->SetSpeed(101);
	SetRoot(Blend1Node);
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
	Blend1Node->SetSpeed(Speed);
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