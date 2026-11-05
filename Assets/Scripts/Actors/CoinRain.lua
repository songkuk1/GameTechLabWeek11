local function SpawnLoop()
    while true do
        coroutine.yield(WaitForSeconds.new(CoinRain.GetInterval(owner)))
        CoinRain.Spawn(owner)
    end
end

local function StopSpawnLoop()
    if state.spawnHandle ~= nil then
        StopCoroutine(state.spawnHandle)
        state.spawnHandle = nil
    end
end

local function StartSpawnLoop()
    StopSpawnLoop()
    state.spawnHandle = StartCoroutine(SpawnLoop)
end

function Init()
    if state.spawnHandle ~= nil then
        StartSpawnLoop()
    end
end

function BeginPlay()
    StartSpawnLoop()
end

function EndPlay()
    StopSpawnLoop()
end

function Tick(deltaTime)
end
