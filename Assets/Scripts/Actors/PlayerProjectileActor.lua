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

function OnComponentBeginOverlap(other)
    if other:GetName() == "EnemyCollision" then
        owner:Destroy()
    end
end

function EndPlay()
    if state.destroyCoroutine ~= nil then
        StopCoroutine(state.destroyCoroutine)
        state.destroyCoroutine = nil
    end
end