#include "EnginePCH.h"
#include "Engine.h"
#include "Script/LuaScriptSystem.h"
#include "Serialization/JsonArchive.h"
#include "Engine/GameViewportClient.h"

UEngine* GEngine = nullptr;

bool UEngine::Init()
{
	/*World = FObjectFactory::ConstructObject<UWorld>();

	if (!World || !World->Init()) return false;*/

	FLuaScriptSystem::Get().Initialize();
 	return true;
}

FWorldContext& UEngine::CreateNewWorldContext(EWorldType WorldType)
{
	FWorldContext* NewWorldContext = new FWorldContext();
	WorldList.Add(NewWorldContext);
	NewWorldContext->WorldType = WorldType;
	NewWorldContext->ContextHandle = FName();

	return *NewWorldContext;
}

FWorldContext* UEngine::GetWorldContextFromWorld(const UWorld* InWorld)
{
	for (FWorldContext& WorldContext : WorldList)
	{
		if (WorldContext.World() == InWorld)
		{
			return &WorldContext;
		}
	}
	return nullptr;
}

void UEngine::DestroyWorldContext(UWorld* InWorld)
{
	for (int32 idx = 0; idx < WorldList.Num(); ++idx)
	{
		FWorldContext& WorldContext = WorldList[idx];

		if (WorldContext.World() == InWorld)
		{
			UWorld* PrevWorld = WorldContext.World();
			WorldContext.SetCurrentWorld(NULL);
			if (PrevWorld)
			{
				UWorld::DestroyWorld(PrevWorld);
			}
			WorldList.RemoveAt(idx);
			break;
		}
	}
}

void UEngine::SetClientTravel(UWorld* InWorld, const FString& NextURL)
{
	for (FWorldContext& WorldContext : WorldList)
	{
		if (WorldContext.World() == InWorld)
		{
			WorldContext.TravelURL = NextURL;
			break;
		}
	}
}

void UEngine::TickWorldTravel()
{
	for (FWorldContext& WorldContext : WorldList)
	{
		if (WorldContext.TravelURL.empty())
		{
			continue;
		}

		UWorld* NewWorld = UWorld::CreateWorld(WorldContext.WorldType, true);
		if (!FJsonArchive::LoadWorld(NewWorld, WorldContext.TravelURL))
		{
			HTR_LOG(Warning, "Failed to load scene: {}", WorldContext.TravelURL);
			UWorld::DestroyWorld(NewWorld);
		}
		else
		{
			UWorld* PrevWorld = WorldContext.World();
			WorldContext.SetCurrentWorld(NewWorld);
			if (PrevWorld)
			{
				UWorld::DestroyWorld(PrevWorld);
			}
			NewWorld->BeginPlay();
		}

		WorldContext.TravelURL.clear();
	}
}

void FWorldContext::SetCurrentWorld(UWorld* World)
{
	if (ThisCurrentWorld != World)
	{
		OnWorldChanged.Broadcast(ThisCurrentWorld, World);
	}

	ThisCurrentWorld = World;
}
