#include "EnginePCH.h"
#include "Animation/AnimSequence.h"

UAnimSequenceBase::UAnimSequenceBase()
{
	DataModel = CreateDefaultSubobject<UAnimDataModel>(FName("DataModel"));
}