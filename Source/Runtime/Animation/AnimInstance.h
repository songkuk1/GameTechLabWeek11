#pragma once

#include "UObject/Class.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimNode_TwoWayBlend.h"

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
	FAnimNode_SequencePlayer SequencePlayerA;
	FAnimNode_SequencePlayer SequencePlayerB;
	FAnimNode_TwoWayBlend BlendNode;
};