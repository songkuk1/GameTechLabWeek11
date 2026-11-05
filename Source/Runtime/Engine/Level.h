#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Containers/Array.h"

class UWorld;
class AActor;

class ULevel : public UObject
{
    DECLARE_CLASS(ULevel, UObject)

public:
    ULevel() = default;
    virtual ~ULevel() = default;

    UWorld* GetWorld() const { return OwningWorld; }
    void SetWorld(UWorld* InWorld) { OwningWorld = InWorld; }

    const TArray<AActor*>& GetActors() const  { return Actors; }
    uint32 GetActorNum() const { return Actors.Num(); }

    void AddActor(AActor* Actor);
    void ClearActors();

    using Super::Serialize;
    virtual void Serialize(FStructuredArchive::FRecord Record) override;

    // 액터 하나를 클래스 이름과 함께 Record에 저장한다. 씬 저장과 에디터 복사가 같은 형식을 쓴다.
    static void SaveActorRecord(FStructuredArchive::FRecord ActorRecord, AActor* Actor);
    // Record의 클래스로 이 레벨에 액터를 스폰하고 값을 불러온다. 클래스를 모르면 nullptr이다.
    AActor* SpawnActorFromRecord(FStructuredArchive::FRecord ActorRecord);

    //virtual void Serialize(FArchive& Ar) override; // Save Level 구현예정

    AActor* GetActorOfClass(const UClass* Clss);

private:
    UWorld* OwningWorld = nullptr;
    TArray<AActor*> Actors;

    // AWorldSettings* WorldSettings = nullptr;

    bool bIsVisible = true;

    friend class UWorld;
};