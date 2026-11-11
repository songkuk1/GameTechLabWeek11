#pragma once
#undef GetCurrentTime
#include "AnimNode_Base.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Containers/Set.h"

class USkeleton;

struct FAnimNode_LayeredBoneBlend : public FAnimNode_Base
{
public:
    // 소스 노드 연결
    void SetSourceNodes(FAnimNode_Base* InBase, FAnimNode_Base* InOverlay);

    void SetBlendWeight(float InWeight);
    void SetBranchBoneName(const FName& InBoneName);

    void Update(const FAnimationUpdateContext& Context) override;
    void Evaluate(FPoseContext& Output) override;
    bool EvaluateCurve(const FName& CurveName, const FName& LayerName, float& OutValue) const override;

    void SetSkeleton(const USkeleton* InSkeleton);
	float GetBlendWeight() const { return BlendWeight; }

    IAnimPlaybackControl* GetPlaybackControl() override;
    const IAnimPlaybackControl* GetPlaybackControl() const override;

private:
    // 허리 이하 자식 본들을 한 번만 찾아서 저장하는 캐시 함수
    void BuildBoneFilter(const USkeleton* Skeleton);

private:
    FAnimNode_Base* BasePoseNode = nullptr;    // 하체
    FAnimNode_Base* OverlayPoseNode = nullptr; // 상체

    FName BranchBoneName = "spine_01"; // 분기 기준 본 이름
    float BlendWeight = 1.0f;          // 상체 합성 비율

    TSet<int32> UpperBodyNodeIndices;
    bool bFilterBuilt = false;

    const USkeleton* TargetSkeleton = nullptr;
};

struct FAnimNode_LayeredBoneBlendPlayer : public FAnimNode_Base, public IAnimPlaybackControl
{
public:
    FAnimNode_LayeredBoneBlendPlayer();
    void SetAnimations(UAnimSequence* InBaseAnim, UAnimSequence* InOverlayAnim);
    void SetBranchBoneName(const FName& InBoneName);
    void SetBlendWeight(float InWeight);
    float GetBlendWeight() const;

    void SetPlaybackPosition(float InTime) override;
    float GetCurrentTime() const override;
    float GetPlayLength() const override;
    float GetFrameRate() const override;
    void SetPlayRate(float InPlayRate) override;
    float GetPlayRate() const override { return PlayRate; }
    void SetLooping(bool bInLoop) override;
    bool IsLooping() const override;

    IAnimPlaybackControl* GetPlaybackControl() override { return this; }
    const IAnimPlaybackControl* GetPlaybackControl() const override { return this; }

    virtual void Update(const FAnimationUpdateContext& Context) override;
    virtual void Evaluate(FPoseContext& Output) override;
    virtual bool EvaluateCurve(const FName& CurveName, const FName& LayerName, float& OutValue) const override;

private:
    FAnimNode_SequencePlayer BaseSequencePlayer;
    FAnimNode_SequencePlayer OverlaySequencePlayer;
    FAnimNode_LayeredBoneBlend LayeredBlendNode;
    float PlayRate = 1.0f;
};