-- CharacterFallReset.lua
-- 캐릭터가 Z <= -15.0 이하로 낙하할 경우 초기 스폰 위치로 리셋(리스폰)시키는 스크립트

local FallThresholdZ = -15.0

state.SpawnLocation = nil
state.bHasSpawnLocation = false

function Init()
end

function BeginPlay()
    if owner then
        local loc = owner:GetActorLocation()
        state.SpawnLocation = Vector.new(loc.x, loc.y, loc.z)
        state.bHasSpawnLocation = true
        Log(string.format("[CharacterFallReset] Spawn location recorded: (%.2f, %.2f, %.2f)", loc.x, loc.y, loc.z))
    end
end

function Tick(deltaTime)
    if not owner then
        return
    end

    if not state.bHasSpawnLocation then
        local loc = owner:GetActorLocation()
        state.SpawnLocation = Vector.new(loc.x, loc.y, loc.z)
        state.bHasSpawnLocation = true
        return
    end

    local loc = owner:GetActorLocation()
    if loc.z <= FallThresholdZ then
        Log(string.format("[CharacterFallReset] Fell below %.1f (z=%.2f)! Resetting to (%.2f, %.2f, %.2f)",
            FallThresholdZ, loc.z, state.SpawnLocation.x, state.SpawnLocation.y, state.SpawnLocation.z))
        owner:ResetToLocation(state.SpawnLocation)
    end
end

function EndPlay()
end
