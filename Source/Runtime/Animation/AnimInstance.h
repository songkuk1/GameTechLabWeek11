#pragma once
#undef GetCurrentTime
#include "UObject/Class.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimNode_TwoWayBlend.h"
#include "Animation/AnimNode_BlendSpacePlayer.h"
#include "Containers/Map.h"

class USkeletalMeshComponent;
class AActor;

class UAnimInstance : public UObject
{
	DECLARE_CLASS(UAnimInstance, UObject)
public:
	UAnimInstance();
	virtual void NativeInitializeAnimation();
	virtual void NativeUpdateAnimation(float DeltaSeconds);
	void InitializeAnimation(USkeletalMeshComponent* InOwningComponent);
	void UpdateAnimation(float DeltaSeconds);
	void EvaluateAnimation(FPoseContext& Output);
	USkeletalMeshComponent* GetOwningComponent() const { return OwningComponent; }
	AActor* GetOwningActor() const;
	void PlayAnimation(UAnimSequence* InSequence);
	void PlayBlendAnimation(UAnimSequence* InSequenceA, UAnimSequence* InSequenceB, float InAlpha);
	void PlayBlendSpace(UBlendSpace1D* InBlendSpace, float InInput);
	void SetAnimationBlendAlpha(float InAlpha);
	float GetAnimationBlendAlpha() const;
	void SetAnimationBlendInput(float InInput);
	float GetAnimationBlendInput() const;
	void SetPlaybackPosition(float InTime);
	float GetCurrentTime() const;
	float GetPlayLength() const;
	float GetFrameRate() const;
	void SetPlayRate(float InPlayRate);
	float GetPlayRate() const;
	void SetLooping(bool bInLoop);
	bool IsLooping() const;

	void SetBoolParameter(const FName& Name, bool Value);
	bool GetBoolParameter(const FName& Name) const;
	bool HasBoolParameter(const FName& Name) const;
	void SetFloatParameter(const FName& Name, float Value);
	float GetFloatParameter(const FName& Name) const;
	void ClearAnimationParameters();

	IAnimPlaybackControl* GetPlaybackControl() { return RootNode ? RootNode->GetPlaybackControl() : nullptr; }
	const IAnimPlaybackControl* GetPlaybackControl() const { return RootNode ? RootNode->GetPlaybackControl() : nullptr; }
protected:
	FAnimNode_Base* RootNode = nullptr;
private:
	FAnimNode_SequencePlayer SequencePlayer;
	FAnimNode_TwoWayBlendPlayer TwoWayBlendPlayer;
	FAnimNode_BlendSpacePlayer BlendSpacePlayer;

	USkeletalMeshComponent* OwningComponent = nullptr;

	TMap<FName, bool, FNameHash> BoolParameters;
	TMap<FName, float, FNameHash> FloatParameters;

	bool bInitialized = false;
};