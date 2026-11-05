local LaunchInterval = 3.0
local LaunchAmount = 5
local LaunchSpeed = 8.0

local function LaunchLoop()
    while true do
        coroutine.yield(WaitForSeconds.new(LaunchInterval))
        CoinLauncher.Launch(owner, LaunchAmount, LaunchSpeed)
    end
end

local function StopLaunchLoop()
    if state.launchHandle ~= nil then
        StopCoroutine(state.launchHandle)
        state.launchHandle = nil
    end
end

local function StartLaunchLoop()
    StopLaunchLoop()
    state.launchHandle = StartCoroutine(LaunchLoop)
end

function Init()
    if state.launchHandle ~= nil then
        StartLaunchLoop()
    end
end

function BeginPlay()
    StartLaunchLoop()
end

function EndPlay()
    StopLaunchLoop()
end

function Tick(deltaTime)
end
