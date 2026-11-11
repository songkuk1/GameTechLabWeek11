#pragma once

#include "Property.h"
#include "Function.h"
#include "Object.h"

using ClassConstructor = UObject * (*)(void* Memory);

class UClass : public UObject
{
public:
	UClass();

	FString Name;
	UClass* Super = nullptr;
	uint64 ClassSize = 0;
	ClassConstructor Constructor = nullptr;

	TArray<FProperty> Properties;
	TArray<FFunction> Functions;   // UFUNCTION 목록. 이 클래스에 선언된 것만 (부모 것은 Super를 따라가 찾는다)
	UObject* DefaultObject = nullptr;

	bool IsChildOf(const UClass* BaseClass) const;
	UObject* GetDefaultObject();

	inline const TArray<FProperty>& GetProperties() const { return Properties; }
	inline const TArray<FFunction>& GetFunctions() const { return Functions; }

	// 이 클래스와 부모 클래스에서 이름으로 찾는다. 없으면 nullptr
	const FFunction* FindFunction(const FString& InName) const;

	// HHT가 만드는 코드: InClass->AddFunction<&ThisClass::Foo>("Foo", FUNC_ScriptCallable);
	// 시그니처는 TNativeFunction이 멤버 함수 포인터에서 알아낸다 (UObject/Function.h).
	template <auto Func>
	void AddFunction(const FString& InName, uint32 InFlags)
	{
		Functions.Add(TNativeFunction<Func>::Make(InName, InFlags));
	}

	template<class T>
	T* GetDefaultObject()
	{
		return CastChecked<T>(GetDefaultObject());
	}

	template <typename T>
	void AddProperty(const FString& InName, uint64 InOffset, uint32 InFlags = CPF_Default)
	{
		UClass* ObjClass = nullptr;
		if constexpr (std::is_pointer_v<T> &&
			std::is_base_of_v<UObject, std::remove_pointer_t<T>>)
		{
			ObjClass = std::remove_pointer_t<T>::StaticClass();
		}

		Properties.Add({ InName, GetPropertyType<T>(), InOffset, sizeof(T), ObjClass, InFlags });
	}

	template <typename T>
	void AddProperty(const FString& InName, uint64 InOffset, EPropertyType InType, uint32 InFlags = CPF_Default)
	{
		Properties.Add({ InName, InType, InOffset, sizeof(T), nullptr, InFlags });
	}
private:
	mutable int32 ClassUnique = 0;
	friend FName MakeUniqueObjectName(UObject*, const UClass*, FName);
};

//보류
inline void CopyProperties(UObject* Src, UObject* Dst, UClass* FromClass)
{
	for (UClass* c = FromClass; c; c = c->Super)
		for (const FProperty& p : c->Properties)
			memcpy((char*)Dst + p.Offset, (char*)Src + p.Offset, p.Size);
}