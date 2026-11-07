#pragma once

#include "UObject/Class.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimNode_TwoWayBlend.h"
#include "Animation/AnimNode_BlendSpacePlayer.h"

class USkeletalMeshComponent;
class AActor;

class UAnimInstance : public UObject
{
	DECLARE_CLASS(UAnimInstance, UObject)
public:
	virtual void NativeInitializeAnimation();
	virtual void NativeUpdateAnimation(float DeltaSeconds);
	void InitializeAnimation(USkeletalMeshComponent* InOwningComponent);
	void UpdateAnimation(float DeltaSeconds);
	void EvaluateAnimation(FPoseContext& Output);
	USkeletalMeshComponent* GetOwningComponent() const { return OwningComponent; }
	AActor* GetOwningActor() const;
	IAnimPlaybackControl* GetPlaybackControl() { return RootNode ? RootNode->GetPlaybackControl() : nullptr; }
	const IAnimPlaybackControl* GetPlaybackControl() const { return RootNode ? RootNode->GetPlaybackControl() : nullptr; }
protected:
	FAnimNode_Base* RootNode = nullptr;
private:
	USkeletalMeshComponent* OwningComponent = nullptr;
	bool bInitialized = false;
};

class UAnimSingleNodeInstance : public UAnimInstance
{
	DECLARE_CLASS(UAnimSingleNodeInstance, UAnimInstance)
public:
	UAnimSingleNodeInstance();
	void SetAnimation(UAnimSequence* InSequence);
	void SetPlaybackPosition(float InTime);
	float GetCurrentTime() const;
	float GetPlayLength() const;
	float GetFrameRate() const;
	void SetPlayRate(float InPlayRate);
	float GetPlayRate() const;
	void SetLooping(bool bInLoop);
	bool IsLooping() const;
private:
	FAnimNode_SequencePlayer SequencePlayer;
};

class UAnimTwoWayBlendInstance : public UAnimInstance
{
	DECLARE_CLASS(UAnimTwoWayBlendInstance, UAnimInstance)
public:
	UAnimTwoWayBlendInstance();
	void SetAnimations(UAnimSequence* InSequenceA, UAnimSequence* InSequenceB);
	void SetAlpha(float InAlpha);
	float GetAlpha() const;
	void SetPlaybackPosition(float InTime);
	float GetCurrentTime() const;
	float GetPlayLength() const;
	float GetFrameRate() const;
	void SetPlayRate(float InPlayRate);
	float GetPlayRate() const;
	void SetLooping(bool bInLoop);
	bool IsLooping() const;
private:
	FAnimNode_TwoWayBlendPlayer BlendPlayer;
};

class UAnimBlendSpaceInstance : public UAnimInstance
{
	DECLARE_CLASS(UAnimBlendSpaceInstance, UAnimInstance)
public:
	UAnimBlendSpaceInstance();
	void SetBlendSpace(UBlendSpace1D* InBlendSpace);
	void SetBlendInput(float InInput);
	float GetBlendInput() const;
	void SetPlaybackPosition(float InTime);
	float GetCurrentTime() const;
	float GetPlayLength() const;
	float GetFrameRate() const;
	void SetPlayRate(float InPlayRate);
	float GetPlayRate() const;
	void SetLooping(bool bInLoop);
	bool IsLooping() const;
private:
	FAnimNode_BlendSpacePlayer BlendSpacePlayer;
	float PlayRate = 1.0f;
	bool bLoop = true;
};