#pragma once

#include "Animation/AnimNode_Base.h"

struct FAnimNode_TwoWayBlend : public FAnimNode_Base
{
public: 
	void SetSourceNodes(FAnimNode_Base* InSourceA, FAnimNode_Base* InSourceB);
	void SetAlpha(float InAlpha);
	float GetAlpha() const { return Alpha; }
	virtual void Update(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate(FPoseContext& Output) override;
private:
	FAnimNode_Base* SourcePoseA = nullptr;
	FAnimNode_Base* SourcePoseB = nullptr;
	float Alpha = 0.5f;
};