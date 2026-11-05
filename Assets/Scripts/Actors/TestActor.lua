
--- Test

local elapsedTime = 0.0

function Init()
    Log("[TestActor] Init")

    if owner ~= nil then
        Log("[TestActor] Owner connected")
    end
end

function BeginPlay()
    Log("[TestActor] BeginPlay")
end

function Tick(deltaTime)
    elapsedTime = elapsedTime + deltaTime

    if elapsedTime >= 1.0 then
        Log("[TestActor] Tick is running")
        elapsedTime = 0.0
    end
end

function EndPlay()
    Log("[TestActor] EndPlay")
end

function OnComponentBeginOverlap(primitvie)
    Log("BeginOverlapTest")
end

function OnComponentEndOverlap(primitvie)
    Log("EndOverlapTest")
end

function OnComponentHit(primitvie, result, bselfmoved)
Log(string.format("Location %.2f, %.2f, %.2f", result.Location.x, result.Location.y, result.Location.z))
    
end