#include "EnginePCH.h"
#include "UAnimInstance.h"

UAnimationInstance::UAnimationInstance()
{

}

UAnimationInstance::~UAnimationInstance()
{
	Root = nullptr;
}

void UAnimationInstance::Tick(float deltaTime, PoseContext& Context)
{
	if (!Root)
		return;

	Root->Update(deltaTime);
	Root->EvaluatePose(Context);
}
