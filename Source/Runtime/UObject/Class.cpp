#include "EnginePCH.h"
#include "Class.h"

#include "Object.h"

UClass::UClass()
	:UObject(false)
{
	//HTR_LOG(Info, "Create RTTI UClass");
}

bool UClass::IsChildOf(const UClass* BaseClass) const
{
	if (!BaseClass) return false;
	for (const UClass* c = this; c; c = c->Super)
	{
		if (c == BaseClass) return true;
	}
	return false;
}

const FFunction* UClass::FindFunction(const FString& InName) const
{
	for (const UClass* c = this; c; c = c->Super)
	{
		for (const FFunction& Function : c->Functions)
		{
			if (Function.Name == InName)
			{
				return &Function;
			}
		}
	}
	return nullptr;
}

UObject* UClass::GetDefaultObject()
{
	if (!DefaultObject)
	{
		DefaultObject = Constructor(UObject::operator new(ClassSize));
	}
	return DefaultObject;
}


