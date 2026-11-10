#pragma once

#include "EnginePCH.h"

struct FAnimSeqTrack
{
	uint32 NodeIndex; // 키프레임과 연결되는 노드를 찾기 위한 Index
	TArray<FVector> PosKeys; // 위치 키프레임
	TArray<FQuat> RotKeys; // 회전 키프레임 (Quaternion)
	TArray<FVector> ScaleKeys; // 스케일 키프레임
};

struct FBoneAnimTrack	// Track과 Bone을 이어주는 구조체
{
	FName Name;
	FAnimSeqTrack InternalTrack;
};

struct FAnimSeqData
{
	TArray<FBoneAnimTrack> BoneAnimationTracks;		// Bone-Track 연결된 정보
	float PlayLength;		// 재생 길이
	float FrameRate;		// 초당 프레임
	int32 RootNodeIndex;	// RootMotion 계산을 위한 NodeIndex
	int32 NumberOfFrames;	// 전체 프레임 개수, FrameRate * PlayLength
	int32 NumberOfKeys;		// Sampling 진행했을때 만들어진 전체 Pose(자세) 갯수
};