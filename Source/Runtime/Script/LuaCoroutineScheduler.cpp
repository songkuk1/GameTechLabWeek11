#include "EnginePCH.h"
#include "LuaCoroutineScheduler.h"
#include <span>

bool FWaitCondition::IsComplete(float DeltaTime) { return true; }

FWaitForSeconds::FWaitForSeconds(double Seconds) : Seconds(Seconds) {}

bool FWaitForSeconds::IsComplete(float DeltaTime)
{
	Seconds -= DeltaTime;
	return Seconds <= 0;
}

FWaitUntil::FWaitUntil(sol::protected_function Predicate) : Predicate(Predicate) {}

bool FWaitUntil::IsComplete(float DeltaTime)
{
	sol::protected_function_result Result = Predicate();

	if (!Result.valid())
	{
		// 함수 실행에 문제.
		throw std::runtime_error("Predicate error!");
	}
	else if (Result.return_count() == 1 && Result.get_type() == sol::type::boolean)
	{
		return Result.get<bool>();
	}

	return false;
}

bool FWaitNextFrame::IsComplte(float DeltaTime) { return true; }

FWaitForRealSeconds::FWaitForRealSeconds(double Seconds)
	: EndTime(FClock::now() + std::chrono::duration_cast<FClock::duration>(std::chrono::duration<double>(Seconds)))
{
}

bool FWaitForRealSeconds::IsComplete(float)
{
	return FClock::now() >= EndTime;
}

FLuaCoroutineScheduler::FLuaCoroutineScheduler(sol::state& Lua)
	: Lua(Lua)
{
}

FLuaCoroutineScheduler::~FLuaCoroutineScheduler()
{
	for (FCoroutineTask* Task : PendingTasks)
	{
		delete Task;
	}

	for (FCoroutineTask* Task : Tasks)
	{
		delete Task->Condition;
		delete Task;
	}
}

FLuaCoroutineHandle FLuaCoroutineScheduler::StartCoroutine(sol::protected_function Func)
{
	FCoroutineTask* NewTask = new FCoroutineTask();
	NewTask->Handle = NextHandle++;
	NewTask->Runner = sol::thread::create(Lua.lua_state());

	sol::state_view ThreadState = NewTask->Runner.state();
	NewTask->Coroutine = sol::coroutine(ThreadState.lua_state(), Func);

	PendingTasks.Add(NewTask);

	return NewTask->Handle;
}

FLuaCoroutineHandle FLuaCoroutineScheduler::StartCoroutine(sol::protected_function Func, sol::variadic_args Args)
{
	assert(Args.size() < MaxCoroutineArgumentCount);

	FCoroutineTask* NewTask = new FCoroutineTask();
	NewTask->Handle = NextHandle++;
	NewTask->Runner = sol::thread::create(Lua.lua_state());

	for (sol::object Obj : Args)
	{
		NewTask->Arguments[NewTask->ArgumentCount++] = Obj;
	}

	sol::state_view ThreadState = NewTask->Runner.state();
	NewTask->Coroutine = sol::coroutine(ThreadState.lua_state(), Func);

	PendingTasks.Add(NewTask);

	return NewTask->Handle;
}

void FLuaCoroutineScheduler::StopCoroutine(FLuaCoroutineHandle Handle)
{
	PendingStopped.Add(Handle);
}

void FLuaCoroutineScheduler::StopAllCoroutines()
{
	// Update 도중에 불려도 안전하도록 즉시 지우지 않고 정지 예약만 한다.
	for (FCoroutineTask* Task : Tasks)
	{
		PendingStopped.Add(Task->Handle);
	}

	for (FCoroutineTask* Task : PendingTasks)
	{
		PendingStopped.Add(Task->Handle);
	}
}

void FLuaCoroutineScheduler::Update(float DeltaTime)
{
	// 지연된 Task를 등록한다.
	for (FCoroutineTask* PendingTask : PendingTasks)
	{
		Tasks.Emplace(PendingTask);
	}
	PendingTasks.Reset();

	// 이번 프레임에 호출해야하는 Task를 호출한다.
	for (int32 i = 0; i < Tasks.Num(); i++)
	{
		FCoroutineTask* Task = Tasks[i];

		if (PendingStopped.Contains(Task->Handle))
		{
			continue;
		}

		if (Task->Condition && !Task->Condition->IsComplete(DeltaTime))
		{
			continue;
		}

		sol::protected_function_result Result;
		if (Task->ArgumentCount)
		{
			auto Args = std::span<sol::object>(Task->Arguments, static_cast<size_t>(Task->ArgumentCount));
			Result = Task->Coroutine(sol::as_args(Args));
			for (int32 i = 0; i < Task->ArgumentCount; i++)
			{
				Task->Arguments[i] = sol::object();
			}
			Task->ArgumentCount = 0;
		}
		else
		{
			Result = Task->Coroutine();
		}

		if (!Result.valid())
		{
			sol::error Error = Result;
			HTR_LOG(Error, "[Lua Coroutine] {}", Error.what());
		}

		if (Task->Coroutine.status() == sol::call_status::yielded && Result.return_count() > 0)
		{
			// Yield가 호출이 되었고 리턴값이 있는 상황.
			delete Task->Condition;

			Task->Condition = GenerateConditonFromResult(Result);
		}
		else
		{
			StopCoroutine(Task->Handle);
		}
	}

	// 제거된 Task 정리
	for (int32 i = 0; i < Tasks.Num(); i++)
	{
		FCoroutineTask* Task = Tasks[i];
		if (!PendingStopped.Contains(Task->Handle))
		{
			continue;
		}

		delete Task->Condition;
		delete Task;

		Tasks[i] = Tasks.Last();
		Tasks.Pop();

		i--;
	}
	PendingStopped.Reset();
}

FWaitCondition* FLuaCoroutineScheduler::GenerateConditonFromResult(sol::protected_function_result& Result)
{
	sol::object Value = Result.get<sol::object>();

	FWaitCondition* NewCondition = nullptr;
	if (Value.is<FWaitForSeconds>())
	{
		NewCondition = new FWaitForSeconds(Value.as<FWaitForSeconds>());
	}
	else if (Value.is<FWaitNextFrame>())
	{
		NewCondition = new FWaitNextFrame(Value.as<FWaitNextFrame>());
	}
	else if (Value.is<FWaitUntil>())
	{
		NewCondition = new FWaitUntil(Value.as<FWaitUntil>());
	}
	else if (Value.is<FWaitForRealSeconds>())
	{
		NewCondition = new FWaitForRealSeconds(Value.as<FWaitForRealSeconds>());
	}

	return NewCondition;
}
