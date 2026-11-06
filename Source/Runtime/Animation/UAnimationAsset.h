#pragma once

#include "EnginePCH.h"
#include "Asset/Asset.h"
#include "Asset/ModelData.h"

class UAnimationAsset : public UAsset
{
	DECLARE_CLASS(UAnimationAsset, UAsset)
public:
	void Initialize(const TArray<FAnimSeqData>& InAnimData);
};