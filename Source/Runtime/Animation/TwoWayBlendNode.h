#pragma once

#include "EnginePCH.h"
#include "AnimNode.h"
#include "UAnimSequence.h"

class TwoWayBlendNode : public AnimNode
{
public:
	AnimNode* InputA;
	AnimNode* InputB;
	float Alpha = 0.0f; 

	void Update(float deltaTime) override
	{
		InputA->Update(deltaTime);   // 입력 2개에 대해서 각자 Update 진행
		InputB->Update(deltaTime);
	}

	void EvaluatePose(PoseContext& Context) override
	{
		PoseContext ContextA = Context;
		PoseContext ContextB = Context;

		InputA->EvaluatePose(ContextA);
		InputB->EvaluatePose(ContextB);

		// 본마다 위치 / 스케일은 Lerp, 회전은 Quaternion Slerp
		BlendTwoPosesTogether(ContextA.LocalPoseTrans, ContextB.LocalPoseTrans, Alpha, Context.LocalPoseTrans);
	}

	void InitializeNode(AnimNode* A, AnimNode* B, float Alphaval)
	{
		InputA = A;
		InputB = B;
		Alpha = Alphaval;
	}

	void BlendTwoPosesTogether(const TArray<PoseTransform>& APoseTrans, const TArray<PoseTransform>& BPoseTrans, float Alphaval, TArray<PoseTransform>& OutLocalPoseTrans)
	{
		for (int i = 0;i < OutLocalPoseTrans.Num();i++)
		{
			OutLocalPoseTrans[i].Translation = FVector(
				std::lerp(APoseTrans[i].Translation.X, BPoseTrans[i].Translation.X, Alphaval),
				std::lerp(APoseTrans[i].Translation.Y, BPoseTrans[i].Translation.Y, Alphaval),
				std::lerp(APoseTrans[i].Translation.Z, BPoseTrans[i].Translation.Z, Alphaval)
			);
			OutLocalPoseTrans[i].Rotation = FQuat::Slerp(APoseTrans[i].Rotation, BPoseTrans[i].Rotation, Alphaval);
			OutLocalPoseTrans[i].Scale = FVector(
				std::lerp(APoseTrans[i].Scale.X, BPoseTrans[i].Scale.X, Alphaval),
				std::lerp(APoseTrans[i].Scale.Y, BPoseTrans[i].Scale.Y, Alphaval),
				std::lerp(APoseTrans[i].Scale.Z, BPoseTrans[i].Scale.Z, Alphaval)
			);
		}
	}
};