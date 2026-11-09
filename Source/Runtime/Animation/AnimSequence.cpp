#include "EnginePCH.h"
#include "Animation/AnimSequence.h"
#include <cmath>
UAnimSequenceBase::UAnimSequenceBase()
{
	DataModel = CreateDefaultSubobject<UAnimDataModel>(FName("DataModel"));
}

void UAnimSequenceBase::SerializeNotifies(FStructuredArchive::FRecord Record)
{
    FArchive& Archive = Record.GetUnderlyingArchive();
    const bool bIsLoading = Archive.IsLoading();

    if (bIsLoading)
    {
        Notifies.Reset();
    }

    int32 NotifyCount = bIsLoading ? 0 : Notifies.Num();
    FStructuredArchive::FArray NotifyArray = Record.EnterArray("Notifies", NotifyCount);

    for (int32 Index = 0; Index < NotifyCount; ++Index)
    {
        FStructuredArchive::FRecord NotifyRecord = NotifyArray.EnterElement().EnterRecord();

        float TriggerTime = bIsLoading ? 0.0f : Notifies[Index].TriggerTime;
        float Duration = bIsLoading ? 0.0f : Notifies[Index].Duration;
        FName NotifyName = bIsLoading ? NAME_None : Notifies[Index].NotifyName;

        NotifyRecord << SA_VALUE("TriggerTime", TriggerTime);
        NotifyRecord << SA_VALUE("Duration", Duration);
        NotifyRecord << SA_VALUE("NotifyName", NotifyName);

        if (!bIsLoading)
        {
            continue;
        }

        const float PlayLength = DataModel ? DataModel->PlayLength : 0.0f;

        if (!std::isfinite(TriggerTime) || !std::isfinite(Duration) || TriggerTime < 0.0f || Duration < 0.0f || NotifyName.IsNone() || PlayLength <= 0.0f || TriggerTime > PlayLength || TriggerTime + Duration > PlayLength)
        {
            HTR_LOG(Warning, "AnimSequence: invalid notify at index {}", Index);
            continue;
        }

        AddNotify(TriggerTime, Duration, NotifyName);
    }
}