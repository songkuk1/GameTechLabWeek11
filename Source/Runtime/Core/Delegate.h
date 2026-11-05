#pragma once

#include "Core/Types.h"
#include "Containers/Array.h"
#include "Containers/SparseArray.h"

using FDelegateKey = int32;

template<typename... Params>
class TMulticastDelegate
{
public:
	using HandlerType = std::function<void(Params...)>;

	// 일반 함수나 람다 등록
	FDelegateKey Add(const HandlerType& Handler)
	{
		FDelegateKey Key = NextID++;

		FHandlerEntry Entry;
		Entry.Key = Key;
		Entry.Instance = nullptr;
		Entry.Handler = Handler;

		if (!bLock)
		{
			Entries.Add(Entry);
		}
		else
		{
			PendingAdds.Add(Entry);
		}

		return Key;
	}

	// 클래스 멤버 함수 바인딩
	template<typename T>
	FDelegateKey AddRaw(T* Instance, void (T::* Func)(Params...))
	{
		FDelegateKey Key = NextID++;

		FHandlerEntry Entry;
		Entry.Key = Key;
		Entry.Instance = Instance;
		Entry.Handler = [Instance, Func](Params... Args) { (Instance->*Func)(Args...); };

		if (!bLock)
		{
			Entries.Add(Entry);
		}
		else
		{
			PendingAdds.Add(Entry);
		}

		return Key;
	}

	inline void Remove(FDelegateKey Key)
	{
		if (bLock)
		{
			PendingRemoves.Add(Key);
		}
		else
		{
			for (auto Itr = Entries.begin(); Itr != Entries.end(); ++Itr)
			{
				if (Itr->Key == Key)
				{
					Entries.Remove(Itr);
					break;
				}
			}
		}
	}

	inline int32 RemoveAll(void* Instance)
	{
		int32 RemovedCount = 0;

		if (bLock)
		{
			for (int32 i = 0; i < Entries.Num(); i++)
			{
				PendingRemoves.Add(Entries[i].Key);
				RemovedCount++;
			}
		}
		else
		{
			for (int32 i = Entries.Num() - 1; i > -1; i--)
			{
				if (!Entries.IsValidIndex(i))
				{
					continue;
				}

				const FHandlerEntry& Entry = Entries[i];

				if (Instance == Entry.Instance)
				{
					Entries.RemoveAt(i);
					RemovedCount++;
				}
			}
		}

		return RemovedCount;
	}

	inline void Clear()
	{
		assert(!bLock);

		NextID = 0;
		PendingAdds.Reset();
		PendingRemoves.Reset();
		Entries.Reset();
	}

	inline bool IsBound() const
	{
		return Entries.Num();
	}

	inline void Broadcast(Params... Args)
	{
		bLock = true;
		for (const FHandlerEntry& Entry : Entries)
		{
			Entry.Handler(Args...);
		}
		bLock = false;

		if (HasInvocationLists())
		{
			ResolveInvocationLists();
		}
	}

private:
	inline bool HasInvocationLists()
	{
		return PendingAdds.Num() || PendingRemoves.Num();
	}

	inline void ResolveInvocationLists()
	{
		for (FHandlerEntry& Entry : PendingAdds)
		{
			Entries.Add(Entry);
		}
		PendingAdds.Reset();

		for (FDelegateKey Key : PendingRemoves)
		{
			Entries.RemoveAt(Key);
		}
		PendingRemoves.Reset();
	}

private:
	struct FHandlerEntry
	{
		FDelegateKey Key;
		void* Instance;
		HandlerType Handler;
	};

	uint32 NextID = 0;
	TSparseArray<FHandlerEntry> Entries;

	bool bLock = false;
	TArray<FHandlerEntry> PendingAdds;
	TArray<FDelegateKey> PendingRemoves;
};

