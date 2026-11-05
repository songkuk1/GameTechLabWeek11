
local AttackInterval = 0.7

state.IsDead = false

local function PlayHitStop()
    GamePlayStatics.SetGlobalTimeDilation(owner, 0.05)
    coroutine.yield(WaitForRealSeconds.new(0.3))
    GamePlayStatics.SetGlobalTimeDilation(owner, 1.0)
    state.HitStopCoroutine = nil
end

local function StartHitStop()
    if state.HitStopCoroutine ~= nil then
        StopCoroutine(state.HitStopCoroutine)
    end

    state.HitStopCoroutine = StartCoroutine(PlayHitStop)
end

local function PlayDeathFall()
    coroutine.yield(WaitForRealSeconds.new(2.0))
    GamePlayStatics.SetGlobalTimeDilation(owner, 1.0)
    state.DeathFallCoroutine = nil
    FinishGame("Dead")
end

local function StartDeathFall()
    if state.IsDead then
        return
    end

    state.IsDead = true

    if state.HitStopCoroutine ~= nil then
        StopCoroutine(state.HitStopCoroutine)
        state.HitStopCoroutine = nil
    end

    owner:StartDeathFall()
    GamePlayStatics.SetGlobalTimeDilation(owner, 0.2)

    if state.DeathFallCoroutine ~= nil then
        StopCoroutine(state.DeathFallCoroutine)
    end

    state.DeathFallCoroutine = StartCoroutine(PlayDeathFall)
end

function Init()
    Log("Init PlayerActor")
    state.collectedCoinCount = state.collectedCoinCount or 0
    state.CurrentHp = state.CurrentHp or 100
    state.MaxHp = state.MaxHp or 100
    state.MagnetRadius = state.MagnetRadius or 6.0
end

function BeginPlay()
    Log("BeginPlay")
    state.attackCooldown = 0
    owner:SetMagnetRadius(state.MagnetRadius)
end

local MouseSensitivity = 0.1

function Tick(deltaTime)
    state.attackCooldown = math.max(0, state.attackCooldown - deltaTime)

    -- 게임이 끝나면(시간 종료, 체력 0) 조작을 막는다
    if state.IsDead or _G.GamePhase ~= "Playing" or not owner:IsInputEnabled() then
        return
    end

    if Input.IsMouseDown(MouseButton.Right) then
        local dx, dy = Input.GetMouseDelta()
        owner:AddYawInput(dx * MouseSensitivity)
        owner:AddPitchInput(dy * MouseSensitivity)
    end

    local x = 0.0
    local y = 0.0

    if Input.IsKeyDown(KeyCode.W) then
        x = x + 1.0
    end

    if Input.IsKeyDown(KeyCode.S) then
        x = x - 1.0
    end

    if Input.IsKeyDown(KeyCode.D) then
        y = y + 1.0
    end

    if Input.IsKeyDown(KeyCode.A) then
        y = y - 1.0
    end

    local moveInput = Vector.new(x, y, 0.0)
    owner:Move(moveInput)

    if Input.IsKeyPressed(KeyCode.Space) then
        owner:Jump(9.0)
    end

    if Input.IsMousePressed(MouseButton.Left) and state.attackCooldown <= 0 then
        owner:StartWandSwing()
        owner:PlayerShot()
        GamePlayStatics.PlaySound("Assets/Sounds/Magic.wav", 0.3)
        state.attackCooldown = AttackInterval
    end

end

local function CoinCollision(coinActor)
    if _G.GamePhase ~= "Playing" then
        return
    end

    local firstRequest = coinActor:Destroy()
    if not firstRequest then
        return
    end

    GamePlayStatics.PlaySound("Assets/Sounds/Coin.wav", 0.6)

    state.collectedCoinCount = (state.collectedCoinCount or 0) + 1
    Log(coinActor:GetName() .. " / Coin: " .. state.collectedCoinCount)
end

function OnComponentBeginOverlap(other)
    if state.IsDead or _G.GamePhase ~= "Playing" then
        return
    end

    local componentName = other:GetName()
    local otherActor = other:GetOwner()
    if otherActor == nil then
        return
    end

    if componentName == "CoinCollision" then
        CoinCollision(otherActor)
    elseif componentName == "EnemyProjectileCollision" then
        owner:PlayDamageEffect()
        owner:ApplyKnockback(otherActor, 10.0)
        GamePlayStatics.PlaySound("Assets/Sounds/Hit.wav", 1.0)
        StartHitStop()
        state.CurrentHp = math.max(0, state.CurrentHp - 20)
    end

    -- 체력이 다하면 게임 종료 (MainController가 정의한 전역 함수)
    if state.CurrentHp <= 0 and FinishGame ~= nil then
        StartDeathFall()
    end
end

function OnComponentHit(other, result, bSelfMoved)
    if other == nil then
        return
    end
    local otherActor = other:GetOwner()
    if otherActor == nil then
        return
    end
    if other:GetName() == "EnemyCollision" then
        Log("Enemy Hit: " .. otherActor:GetName())
    elseif other:GetName() == "CoinCollision" then
        CoinCollision(otherActor)
    end
end

function EndPlay()
    if state.HitStopCoroutine ~= nil then
        StopCoroutine(state.HitStopCoroutine)
        state.HitStopCoroutine = nil
    end
    if state.DeathFallCoroutine ~= nil then
        StopCoroutine(state.DeathFallCoroutine)
        state.DeathFallCoroutine = nil
    end
    GamePlayStatics.SetGlobalTimeDilation(owner, 1.0)
end
