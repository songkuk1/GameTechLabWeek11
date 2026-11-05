#pragma once

#include <vector>
#include <utility>

template<typename T>
class TSparseArray
{
public:
	class Iterator;
	class ConstIterator;

	TSparseArray() = default;
	~TSparseArray() = default;

	T& operator[](uint32 index)
	{
		assert(IsValidIndex(index));
		return mDatas[index].Data.Element;
	}

	const T& operator[](uint32 index) const
	{
		assert(IsValidIndex(index));
		return mDatas[index].Data.Element;
	}

	Iterator begin()
	{
		return Iterator(this, 0);
	}

	Iterator end()
	{
		return Iterator(this, mDatas.size());
	}

	ConstIterator begin() const
	{
		return ConstIterator(this, 0);
	}

	ConstIterator end() const
	{
		return ConstIterator(this, mDatas.size());
	}

	uint32 Add(const T& data)
	{
		return Emplace(data);
	}

	template <typename... Args>
	uint32 Emplace(Args&&... args)
	{
		if (mFreeIndex == -1)
		{
			AddUninitilize();
		}

		uint32 Index = mFreeIndex;

		FSlot& FreeSlot = mDatas[Index];

		mFreeIndex = FreeSlot.Data.Link.PrevFreeIndex;
		mNumElements++;

		T* ElementPtr = &FreeSlot.Data.Element;
		new (ElementPtr) T(std::forward<Args>(args)...);

		FreeSlot.bValid = true;

		return Index;
	}

	inline int32 Num() const
	{
		return mNumElements;
	}

	void Reserve(uint32 capacity)
	{
		if (mDatas.size() >= capacity)
		{
			return;
		}

		uint32 Reserved = capacity - mDatas.size();
		for (int32 i = 0; i < Reserved; i++)
		{
			AddUninitilize();
		}
	}

	int32 Max() const
	{
		return mDatas.capacity();
	}

	bool IsEmpty() const
	{
		return mNumElements == 0;
	}

	bool IsValidIndex(uint32 index) const
	{
		if (index >= mDatas.size() || index < 0)
		{
			return false;
		}

		return mDatas[index].bValid;
	}

	void Reset()
	{
		mDatas.clear();
		mFreeIndex = -1;
		mNumElements = 0;
	}

	void Remove(const Iterator& Itr)
	{
		assert(IsValidIndex(Itr.Index));
		RemoveAt(Itr.Index);
	}

	void RemoveAt(uint32 Index)
	{
		assert(IsValidIndex(Index));

		FSlot& Slot = mDatas[Index];
		if (!Slot.bValid)
		{
			return;
		}

		Slot.bValid = false;
		Slot.Data.Element.~T();
		Slot.Data.Link.PrevFreeIndex = mFreeIndex != -1 ? mFreeIndex : -1;
		Slot.Data.Link.NextFreeIndex = -1;

		mFreeIndex = Index;
		mNumElements--;
	}

	class Iterator
	{
		friend class TSparseArray<T>;

		TSparseArray* Owner;
		std::size_t Index;

	public:
		Iterator(TSparseArray* owner, std::size_t index)
			: Owner(owner)
			, Index(index)
		{
			while (Index < Owner->mDatas.size() && !Owner->IsValidIndex(Index)) 
			{
				Index++;
			}
		}

		T& operator*() const
		{
			assert(Owner->IsValidIndex(Index));
			return (*Owner)[Index];
		}

		T* operator->() const
		{
			assert(Owner->IsValidIndex(Index));
			return &(*Owner)[Index];
		}

		Iterator& operator++()
		{
			while (Index < Owner->mDatas.size() && !Owner->IsValidIndex(++Index)) {}
			return *this;
		}

		bool operator==(const Iterator& other) const
		{
			return Owner == other.Owner && Index == other.Index;
		}

		bool operator!=(const Iterator& other) const
		{
			return !((*this) == other);
		}
	};

	class ConstIterator
	{
		friend class TSparseArray<T>;

		const TSparseArray* Owner;
		std::size_t Index;

	public:
		ConstIterator(const TSparseArray* owner, std::size_t index)
			: Owner(owner)
			, Index(index)
		{
			while (Index < Owner->mDatas.size() && !Owner->IsValidIndex(Index))
			{
				Index++;
			}
		}

		const T& operator*() const
		{
			assert(Owner->IsValidIndex(Index));
			return (*Owner)[Index];
		}

		const T* operator->() const
		{
			assert(Owner->IsValidIndex(Index));
			return &(*Owner)[Index];
		}

		ConstIterator& operator++()
		{
			while (Index < Owner->mDatas.size() && !Owner->IsValidIndex(++Index)) {}
			return *this;
		}

		bool operator==(const ConstIterator& other) const
		{
			return Owner == other.Owner && Index == other.Index;
		}

		bool operator!=(const ConstIterator& other) const
		{
			return !((*this) == other);
		}
	};

private:
	struct FSlot
	{
		bool bValid;

		union ElementOrFreeListLink
		{
			T Element;

			struct FreeListLink {
				int32 PrevFreeIndex;
				int32 NextFreeIndex;
			} Link;

			ElementOrFreeListLink()
			{
				Link.PrevFreeIndex = -1;
				Link.NextFreeIndex = -1;
			}

			~ElementOrFreeListLink()
			{
				// Do nothing
			}
		} Data;

		FSlot()
			: bValid(false)
		{
		}

		FSlot(const FSlot& Other)
		{
			if (Other.bValid)
			{
				T* ElementPtr = &Data.Element;
				new (ElementPtr) T(Other.Data.Element);

				bValid = true;
			}
			else
			{
				Data.Link = Other.Data.Link;
				bValid = false;
			}
		}

		FSlot(FSlot&& Other)
		{
			bValid = Other.bValid;

			if (Other.bValid)
			{
				T* ElementPtr = &Data.Element;
				new (ElementPtr) T(std::move(Other.Data.Element));
				Other.Data.Element.~T();
			}
			else
			{
				Data.Link = Other.Data.Link;
			}

			Other.bValid = false;
			Other.Data.Link.PrevFreeIndex = -1;
			Other.Data.Link.NextFreeIndex = -1;
		}

		~FSlot()
		{
			if (!bValid)
			{
				return;
			}

			Data.Element.~T();
		}
	};

	void AddUninitilize()
	{
		int32 NewIndex = mDatas.size();
		FSlot& NewSlot = mDatas.emplace_back();

		if (mFreeIndex != -1)
		{
			FSlot& FreeSlot = mDatas[mFreeIndex];
			FreeSlot.Data.Link.NextFreeIndex = NewIndex;

			NewSlot.Data.Link.PrevFreeIndex = mFreeIndex;
			NewSlot.Data.Link.NextFreeIndex = -1;
		}

		mFreeIndex = NewIndex;
	}

private:
	std::vector<FSlot> mDatas;
	int32 mFreeIndex = -1;
	int32 mNumElements = 0;
};


