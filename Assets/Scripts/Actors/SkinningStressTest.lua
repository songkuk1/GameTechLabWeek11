-- Each mesh owns its animation instance; only the immutable graph is shared.
-- All instances start the same looping clip in BeginPlay, without per-frame Lua work.
function Init()
end

function BeginPlay()
    local mesh = owner:FindComponentByClass(SkeletalMeshComponent.StaticClass())
    if not mesh or not mesh:SetAnimationGraphByPath("SkinningStressTest.animgraph", true) then
        Log("[SkinningStressTest] Failed to initialize a crowd animation")
    end
end

function Tick(deltaTime)
end

function EndPlay()
end
