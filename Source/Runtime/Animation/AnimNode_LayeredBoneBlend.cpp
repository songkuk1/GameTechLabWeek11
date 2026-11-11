#include "EnginePCH.h"
#include "AnimNode_LayeredBoneBlend.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimationRuntime.h"
#include "Component/SkeletalMeshComponent.h"
#include "Render/SkeletalMesh.h"

void FAnimNode_LayeredBoneBlend::SetSourceNodes(FAnimNode_Base* InBase, FAnimNode_Base* InOverlay)
{
    BasePoseNode = InBase;
    OverlayPoseNode = InOverlay;
}

void FAnimNode_LayeredBoneBlend::SetBlendWeight(float InWeight)
{
    BlendWeight = FMath::Clamp(InWeight, 0.0f, 1.0f);
}

void FAnimNode_LayeredBoneBlend::SetBranchBoneName(const FName& InBoneName)
{
    if (BranchBoneName != InBoneName)
    {
        BranchBoneName = InBoneName;
        bFilterBuilt = false;
    }
}

void FAnimNode_LayeredBoneBlend::SetSkeleton(const USkeleton* InSkeleton)
{
    TargetSkeleton = InSkeleton;
    bFilterBuilt = false;
    BuildBoneFilter(TargetSkeleton);
}

void FAnimNode_LayeredBoneBlend::Update(const FAnimationUpdateContext& Context)
{
	// 캐시된 본 필터가 없으면 Context에서 찾아서 빌드
    if (!bFilterBuilt)
    {
        if (!TargetSkeleton && Context.AnimInstance)
        {
            if (USkeletalMeshComponent* MeshComp = Context.AnimInstance->GetOwningComponent())
            {
                if (USkeletalMesh* Mesh = MeshComp->GetSkeletalMesh())
                {
                    TargetSkeleton = Mesh->GetSkeleton();
                }
            }
        }
        if (TargetSkeleton)
        {
            BuildBoneFilter(TargetSkeleton);
        }
    }
	// 하체 노드 업데이트
    if (BasePoseNode)
    {
        BasePoseNode->Update(Context);
    }
	// 상체 노드 업데이트 (상체 오버레이는 루트 모션을 추출하지 않도록 BlendWeight = 0 전달)
    if (OverlayPoseNode && BlendWeight > 0.0001f)
    {
        FAnimationUpdateContext OverlayContext = Context;
        OverlayContext.BlendWeight = 0.0f;
        OverlayContext.NotifyWeight = Context.NotifyWeight * BlendWeight;
        OverlayPoseNode->Update(OverlayContext);
    }
}

void FAnimNode_LayeredBoneBlend::Evaluate(FPoseContext& Output)
{
    // 노드가 없는 경우
    if (!BasePoseNode && !OverlayPoseNode)
    {
        return;
    }
    // 상체 노드가 없거나 가중치가 0이거나 상체 본 필터가 비어 있다면 Base만 평가
    if (!OverlayPoseNode || BlendWeight <= 0.0001f || UpperBodyNodeIndices.IsEmpty())
    {
        if (BasePoseNode)
        {
            BasePoseNode->Evaluate(Output);
        }
        return;
    }

	// 하체 노드가 없다면 상체만 평가
	if (!BasePoseNode)
	{
		OverlayPoseNode->Evaluate(Output);
		return;
	}

	const int32 NumBones = Output.Pose.Num();
	if (NumBones == 0)
	{
		return;
	}

    // 두 포즈를 담을 버퍼 초기화
    FPoseContext PoseBase;
	FPoseContext PoseOverlay;
	PoseBase.Pose.SetNum(NumBones);
	PoseOverlay.Pose.SetNum(NumBones);

	for (int32 i = 0; i < NumBones; ++i)
	{
        PoseBase.Pose[i] = Output.Pose[i];
		PoseOverlay.Pose[i] = Output.Pose[i];
	}

    // 각각 포즈 평가
	BasePoseNode->Evaluate(PoseBase);
	OverlayPoseNode->Evaluate(PoseOverlay);
	
	// BlendWeight에 따라 두 포즈를 블렌딩
    FPoseContext BlendedPose;
    BlendedPose.Pose.SetNum(NumBones);
    AnimationRuntime::BlendPoses(PoseBase, PoseOverlay, BlendWeight, BlendedPose);

	// 기본은 BasePose로 하고, 상체 본들만 BlendedPose로 덮어쓰기
    Output.Pose = PoseBase.Pose;

    for (int32 UpperBoneIndex : UpperBodyNodeIndices)
    {
        if (UpperBoneIndex >= 0 && UpperBoneIndex < NumBones)
        {
            Output.Pose[UpperBoneIndex] = BlendedPose.Pose[UpperBoneIndex];
        }
    }
}

void FAnimNode_LayeredBoneBlend::BuildBoneFilter(const USkeleton* Skeleton)
{
    UpperBodyNodeIndices.Empty();
    if (!Skeleton) return;

    const TArray<FModelNode>& Nodes = Skeleton->GetNodes();
    int32 SpineIndex = -1;

    const FString TargetName = BranchBoneName.ToString();

    // 1차: 정확한 이름 매칭 (대소문자 무시)
    for (int32 i = 0; i < Nodes.Num(); ++i)
    {
        if (_stricmp(Nodes[i].Name.c_str(), TargetName.c_str()) == 0)
        {
            SpineIndex = i;
            break;
        }
    }

    // 2차: _01 <-> _1 번호 정규화 매칭 (예: spine_1 <-> spine_01)
    if (SpineIndex == -1)
    {
        auto NormalizeName = [](const FString& In) -> FString {
            FString Out;
            Out.reserve(In.size());
            for (char c : In)
            {
                Out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            }
            size_t pos = 0;
            while ((pos = Out.find('_', pos)) != FString::npos)
            {
                size_t d = pos + 1;
                if (d < Out.size() && Out[d] == '0')
                {
                    size_t nz = Out.find_first_not_of('0', d);
                    if (nz != FString::npos && std::isdigit(static_cast<unsigned char>(Out[nz])))
                    {
                        Out.erase(d, nz - d);
                    }
                }
                pos++;
            }
            return Out;
        };

        const FString NormTarget = NormalizeName(TargetName);
        for (int32 i = 0; i < Nodes.Num(); ++i)
        {
            if (NormalizeName(Nodes[i].Name) == NormTarget)
            {
                SpineIndex = i;
                break;
            }
        }
    }

    if (SpineIndex == -1) return;

    UpperBodyNodeIndices.Add(SpineIndex); // 허리 본 자신 포함

    // 모든 노드의 부모 체인을 검사하여 SpineIndex의 후손인지 판별
    for (int32 i = 0; i < Nodes.Num(); ++i)
    {
        int32 Parent = Nodes[i].ParentNodeIndex;
        while (Parent != -1)
        {
            if (Parent == SpineIndex)
            {
                UpperBodyNodeIndices.Add(i); // 상체 본으로 등록
                break;
            }
            Parent = Nodes[Parent].ParentNodeIndex;
        }
    }
    bFilterBuilt = true;
}

bool FAnimNode_LayeredBoneBlend::EvaluateCurve(const FName& CurveName, const FName& LayerName, float& OutValue) const
{
    if (!BasePoseNode)
    {
        return OverlayPoseNode && OverlayPoseNode->EvaluateCurve(CurveName, LayerName, OutValue);
    }

    if (!OverlayPoseNode || BlendWeight <= 0.0001f)
    {
        return BasePoseNode && BasePoseNode->EvaluateCurve(CurveName, LayerName, OutValue);
    }

    float ValueBase = 0.0f;
    float ValueOverlay = 0.0f;

    const bool bHasBase = BasePoseNode->EvaluateCurve(CurveName, LayerName, ValueBase);
    const bool bHasOverlay = OverlayPoseNode->EvaluateCurve(CurveName, LayerName, ValueOverlay);

    if (!bHasBase && !bHasOverlay)
    {
        return false;
    }

    OutValue = ValueBase * (1.0f - BlendWeight) + ValueOverlay * BlendWeight;
    return std::isfinite(OutValue);
}

IAnimPlaybackControl* FAnimNode_LayeredBoneBlend::GetPlaybackControl()
{
    if (OverlayPoseNode)
    {
        if (IAnimPlaybackControl* OverlayControl = OverlayPoseNode->GetPlaybackControl())
        {
            return OverlayControl;
        }
    }
    return BasePoseNode ? BasePoseNode->GetPlaybackControl() : nullptr;
}

const IAnimPlaybackControl* FAnimNode_LayeredBoneBlend::GetPlaybackControl() const
{
    if (OverlayPoseNode)
    {
        if (const IAnimPlaybackControl* OverlayControl = OverlayPoseNode->GetPlaybackControl())
        {
            return OverlayControl;
        }
    }
    return BasePoseNode ? BasePoseNode->GetPlaybackControl() : nullptr;
}

FAnimNode_LayeredBoneBlendPlayer::FAnimNode_LayeredBoneBlendPlayer()
{
    LayeredBlendNode.SetSourceNodes(&BaseSequencePlayer, &OverlaySequencePlayer);
}

void FAnimNode_LayeredBoneBlendPlayer::SetAnimations(UAnimSequence* InBaseAnim, UAnimSequence* InOverlayAnim)
{
    BaseSequencePlayer.SetSequence(InBaseAnim);
    OverlaySequencePlayer.SetSequence(InOverlayAnim);

    SetPlayRate(PlayRate);
}

void FAnimNode_LayeredBoneBlendPlayer::SetBranchBoneName(const FName& InBoneName)
{
    LayeredBlendNode.SetBranchBoneName(InBoneName);
}

void FAnimNode_LayeredBoneBlendPlayer::SetBlendWeight(float InWeight)
{
    LayeredBlendNode.SetBlendWeight(InWeight);
}

float FAnimNode_LayeredBoneBlendPlayer::GetBlendWeight() const
{
    return LayeredBlendNode.GetBlendWeight();
}

void FAnimNode_LayeredBoneBlendPlayer::SetPlaybackPosition(float InTime)
{
    const float LengthBase = BaseSequencePlayer.GetPlayLength();
    const float LengthOverlay = OverlaySequencePlayer.GetPlayLength();

    if (LengthBase > 0.0f && std::isfinite(InTime))
    {
        const float Progress = std::clamp(InTime / LengthBase, 0.0f, 1.0f);
        BaseSequencePlayer.SetPlaybackPosition(Progress * LengthBase);

        if (LengthOverlay > 0.0f)
        {
            OverlaySequencePlayer.SetPlaybackPosition(Progress * LengthOverlay);
        }
    }
}

float FAnimNode_LayeredBoneBlendPlayer::GetCurrentTime() const
{
    return BaseSequencePlayer.GetCurrentTime();
}

float FAnimNode_LayeredBoneBlendPlayer::GetPlayLength() const
{
    return BaseSequencePlayer.GetPlayLength();
}

float FAnimNode_LayeredBoneBlendPlayer::GetFrameRate() const
{
    return BaseSequencePlayer.GetFrameRate();
}

void FAnimNode_LayeredBoneBlendPlayer::SetPlayRate(float InPlayRate)
{
    PlayRate = std::isfinite(InPlayRate) ? InPlayRate : 1.0f;
    BaseSequencePlayer.SetPlayRate(PlayRate);
    OverlaySequencePlayer.SetPlayRate(PlayRate);
}

void FAnimNode_LayeredBoneBlendPlayer::SetLooping(bool bInLoop)
{
    BaseSequencePlayer.SetLooping(bInLoop);
    OverlaySequencePlayer.SetLooping(bInLoop);
}

bool FAnimNode_LayeredBoneBlendPlayer::IsLooping() const
{
    return BaseSequencePlayer.IsLooping();
}

void FAnimNode_LayeredBoneBlendPlayer::Update(const FAnimationUpdateContext& Context)
{
    LayeredBlendNode.Update(Context);
}

void FAnimNode_LayeredBoneBlendPlayer::Evaluate(FPoseContext& Output)
{
    LayeredBlendNode.Evaluate(Output);
}

bool FAnimNode_LayeredBoneBlendPlayer::EvaluateCurve(const FName& CurveName, const FName& LayerName, float& OutValue) const
{
    return LayeredBlendNode.EvaluateCurve(CurveName, LayerName, OutValue);
}