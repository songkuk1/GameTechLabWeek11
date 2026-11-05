local function DestroyAfterDelay()
    coroutine.yield(WaitForSeconds.new(8.0))
    owner:Destroy()
end

function Init()
    state.destroyCoroutine = StartCoroutine(DestroyAfterDelay)
end

function BeginPlay()

end

function Tick(deltaTime)

end

-- 슬라임 탄을 막는 지형 콜리전 (플랫폼, 나무, 집, 언덕, 경사로, 바깥 벽, 바닥)
local function IsWorldBlocker(name)
    return name == "TreeCollision"
        or name == "TreeCrownCollision"
        or string.find(name, "HouseCollision", 1, true) ~= nil
        or name == "Stage_Floor_Collision"
        or name == "RampCollision"
        or name == "PlateauCollision"
        or string.find(name, "FloatingPlatform", 1, true) ~= nil
        or string.find(name, "Boundary_", 1, true) ~= nil
end

function OnComponentBeginOverlap(other)
    local name = other:GetName()
    if name == "PlayerCollision" or IsWorldBlocker(name) then
        owner:Destroy()
    end
end

function EndPlay()
    if state.destroyCoroutine ~= nil then
        StopCoroutine(state.destroyCoroutine)
        state.destroyCoroutine = nil
    end
end
