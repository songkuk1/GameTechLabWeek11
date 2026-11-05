#pragma once

#include <chrono>

#include "Core/Types.h"
#include "Containers/Array.h"
#include "Containers/SparseArray.h"
#include "sol/sol.hpp"

using FLuaCoroutineHandle = uint32;

constexpr FLuaCoroutineHandle InvalidLuaCoroutineHandle = UINT_MAX;
constexpr int32 MaxCoroutineArgumentCount = 10;

struct FWaitCondition
{
	virtual ~FWaitCondition() = default;

	virtual bool IsComplete(float DeltaTime);
};

struct FWaitForSeconds : public FWaitCondition
{
	double Seconds;

	explicit FWaitForSeconds(double Seconds);

	bool IsComplete(float DeltaTime);
};

struct FWaitUntil : public FWaitCondition
{
	sol::main_protected_function Predicate;

	explicit FWaitUntil(sol::protected_function Predicate);

	bool IsComplete(float DeltaTime);
};

struct FWaitNextFrame : public FWaitCondition
{
	explicit FWaitNextFrame() = default;

	bool IsComplte(float DeltaTime);
};

struct FWaitForRealSeconds : public FWaitCondition
{
	using FClock = std::chrono::steady_clock;
	FClock::time_point EndTime;

	explicit FWaitForRealSeconds(double Seconds);
	bool IsComplete(float DeltaTime);
};

class FLuaCoroutineScheduler
{
public:
	FLuaCoroutineScheduler(sol::state& Lua);
	~FLuaCoroutineScheduler();

	FLuaCoroutineHandle StartCoroutine(sol::protected_function Func);
	FLuaCoroutineHandle StartCoroutine(sol::protected_function Func, sol::variadic_args Args);
	void StopCoroutine(FLuaCoroutineHandle Handle);
	// 월드가 바뀔 때 이전 월드 액터를 참조하는 코루틴이 남지 않도록 전부 정지 예약한다.
	void StopAllCoroutines();

	void Update(float DeltaTime);

private:
	struct FCoroutineTask
	{
		sol::thread Runner;
		sol::coroutine Coroutine;
		sol::object Arguments[MaxCoroutineArgumentCount];
		int32 ArgumentCount = 0;
		FLuaCoroutineHandle Handle = InvalidLuaCoroutineHandle;
		FWaitCondition* Condition = nullptr;
	};

	void AddToTaskList(sol::protected_function Func);

	FWaitCondition* GenerateConditonFromResult(sol::protected_function_result& Result);

private:
	sol::state& Lua;
	FLuaCoroutineHandle NextHandle = 0;
	TArray<FCoroutineTask*> Tasks;

	TArray<FCoroutineTask*> PendingTasks;
	TSet<FLuaCoroutineHandle> PendingStopped;
};


