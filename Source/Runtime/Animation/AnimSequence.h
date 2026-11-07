#pragma once

#include "Animation/AnimationAsset.h"
#include "Animation/AnimDataModel.h"

class USkeleton;

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
	USkeleton* Skeleton = nullptr;
	TArray<FAnimNotifyEvent> Notifies;
public:
	UAnimSequenceBase();
	UAnimDataModel* GetDataModel() { return DataModel; }
	const UAnimDataModel* GetDataModel() const { return DataModel; }
	const TArray<FAnimNotifyEvent>& GetNotifies() const { return Notifies; }
	TArray<FAnimNotifyEvent>& GetMutableNotifies() { return Notifies; }
	int32 AddNotify(float TriggerTime, float Duration, const FName& NotifyName)
	{
		FAnimNotifyEvent NewNotify;
		NewNotify.TriggerTime = TriggerTime;
		NewNotify.Duration = Duration;
		NewNotify.NotifyName = NotifyName;
		return static_cast<int32>(Notifies.Add(std::move(NewNotify)));
	}
	bool RemoveNotify(int32 Index)
	{
		if (!Notifies.IsValidIndex(Index))
		{
			return false;
		}
		Notifies.RemoveAt(static_cast<uint32>(Index));
		return true;
	}
	void SetSkeleton(USkeleton* InSkeleton) { Skeleton = InSkeleton; }
	USkeleton* GetSkeleton() const { return Skeleton; }
};

class UAnimSequence : public UAnimSequenceBase
{
	DECLARE_CLASS(UAnimSequence, UAnimSequenceBase)
};