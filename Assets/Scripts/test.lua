local function Log(message)
    ConsoleLog(message)
end

local function Wait(seconds)
    coroutine.yield(WaitForSeconds.new(seconds))
end

local function NextFrame()
    coroutine.yield(WaitNextFrame.new())
end

-- 1. 첫 실행 전에 취소
local beforeStart = StartCoroutine(function()
    Log("[FAIL] Cancelled-before-start task executed")
end)

StopCoroutine(beforeStart)
Log("[Test 1] Cancelled before first execution")


-- 2. 자기 자신 취소
local selfHandle

selfHandle = StartCoroutine(function()
    Log("[Test 2] Self-stop requested")
    StopCoroutine(selfHandle)

    -- 취소 요청은 현재 실행을 즉시 끊지 않는다.
    Log("[Test 2] Current execution continues until yield")
    NextFrame()

    Log("[FAIL] Self-stopped task resumed")
end)


-- 3. 여러 작업을 한 번에 취소
local handles = {}

for index = 1, 3 do
    local label = index

    handles[label] = StartCoroutine(function()
        Log(string.format("[Test 3] Worker %d started", label))
        Wait(5.0)
        Log(string.format("[FAIL] Worker %d resumed", label))
    end)
end

StartCoroutine(function()
    Wait(1.0)

    for index = 1, 3 do
        StopCoroutine(handles[index])
    end

    Log("[Test 3] Three workers cancelled")
end)


-- 4. 조건 대기 중 취소
local conditionReady = false
local conditionChecks = 0

local conditionHandle = StartCoroutine(function()
    Log("[Test 4] Waiting for condition")

    coroutine.yield(WaitUntil.new(function()
        conditionChecks = conditionChecks + 1
        return conditionReady
    end))

    Log("[FAIL] Cancelled condition task resumed")
end)

StartCoroutine(function()
    Wait(1.5)
    StopCoroutine(conditionHandle)

    -- 취소가 반영될 시간을 둔다.
    NextFrame()
    NextFrame()

    local checksAfterStop = conditionChecks
    Wait(1.0)
    conditionReady = true
    NextFrame()

    if conditionChecks == checksAfterStop then
        Log("[PASS] Condition evaluation stopped after cancellation")
    else
        Log("[FAIL] Condition still evaluated after cancellation")
    end
end)


-- 5. 한 작업에서 여러 대기 객체 교체
StartCoroutine(function()
    for step = 1, 4 do
        Wait(0.5)
        Log(string.format("[Test 5] Timed step %d / 4", step))

        NextFrame()
        Log(string.format("[Test 5] Frame step %d / 4", step))
    end

    Log("[PASS] Repeated yields completed")
end)


-- 6. 이미 종료한 핸들 취소
local finishedHandle = StartCoroutine(function()
    Log("[Test 6] Short task finished")
end)

StartCoroutine(function()
    Wait(2.0)

    StopCoroutine(finishedHandle)
    StopCoroutine(finishedHandle)

    Log("[Test 6] Repeated stop requested for finished task")
end)


StartCoroutine(function()
    Wait(6.0)
    Log("[CHECK] Test window ended - no FAIL messages expected")
end)

Log("[Test] Additional tests registered")