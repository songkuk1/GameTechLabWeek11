#pragma once

#include "Animation/AnimationAsset.h"
#include "Animation/AnimDataModel.h"

struct FAnimNotifyEvent
{
	float TriggerTime = 0.0f;
	float Duration = 0.0f;
	FName NotifyName;
};

class UAnimSequenceBase : public UAnimationAsset
{
	DECLARE_CLASS(UAnimSequenceBase, UAnimationAsset)
	REFLECT_START(UAnimSequenceBase)
		PROPERTY(DataModel)
	REFLECT_END()
protected:
	UAnimDataModel* DataModel = nullptr;
	TArray<FAnimNotifyEvent> Notifies;
public:
	UAnimSequenceBase();
	UAnimDataModel* GetDataModel() { return DataModel; }
	const UAnimDataModel* GetDataModel() const { return DataModel; }
	const TArray<FAnimNotifyEvent>& GetNotifies() const { return Notifies; }
};

class UAnimSequence : public UAnimSequenceBase
{
	DECLARE_CLASS(UAnimSequence, UAnimSequenceBase)
};