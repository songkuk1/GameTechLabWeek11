#pragma once

#include "Animation/AnimationAsset.h"
#include "Animation/AnimDataModel.h"
#include "AnimSequence.generated.h"

class USkeleton;

struct FAnimNotifyEvent
{
	float TriggerTime = 0.0f;
	float Duration = 0.0f;
	FName NotifyName;
};

enum class EAnimNotifyEventType : uint8
{
	Trigger,
	Begin,
	Tick,
	End
};

UCLASS()
class UAnimSequenceBase : public UAnimationAsset
{
	GENERATED_BODY()
protected:
	UPROPERTY(EditAnywhere)
	UAnimDataModel* DataModel = nullptr;
	USkeleton* Skeleton = nullptr;
	TArray<FAnimNotifyEvent> Notifies;
	bool bForceRootLock = false;
public:
	UAnimSequenceBase();
	UAnimDataModel* GetDataModel() { return DataModel; }
	const UAnimDataModel* GetDataModel() const { return DataModel; }
	float GetPlayLength() const { return DataModel ? DataModel->PlayLength : 0.0f; }
	float GetFrameRate() const { return (DataModel && DataModel->FrameRate.IsValid()) ? DataModel->FrameRate.AsDecimal() : 30.0f; }
	int32 GetTotalFrames() const { return DataModel ? DataModel->NumberOfFrames : 0; }
	void RetargetToSkeleton(USkeleton* InSkeleton);
	bool IsForceRootLock() const { return bForceRootLock; }
	void SetForceRootLock(bool bInLock) { bForceRootLock = bInLock; }
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
	int32 AddNotify(const FAnimNotifyEvent& NewNotify)
	{
		return static_cast<int32>(Notifies.Add(NewNotify));
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
	void SerializeNotifies(FStructuredArchive::FRecord Record);
	void SetSkeleton(USkeleton* InSkeleton) { Skeleton = InSkeleton; }
	USkeleton* GetSkeleton() const { return Skeleton; }
	int32 GetRootBoneNodeIndex() const;
	bool GetRootBoneTransform(float Time, FVector& OutPosition, FQuat& OutRotation, FVector& OutScale) const;
	bool ExtractRootMotion(float StartTime, float DeltaTime, bool bLooping, FVector& OutTranslation, FQuat& OutRotation) const;
};

UCLASS()
class UAnimSequence : public UAnimSequenceBase
{
	GENERATED_BODY()
};