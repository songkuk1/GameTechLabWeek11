local MouseSensitivity = 0.1
local JumpSpeed = 9.0
local AttackInterval = 0.7
local HitCooldown = 0.8

function Init()
    state.AttackCooldown = 0.0
    state.HitCooldown = 0.0
end

function BeginPlay()
    Log("[AnimationTest] Ready: WASD move / Space jump / RMB camera / LMB attack / red trigger hit")
end

function Tick(deltaTime)
    state.AttackCooldown = math.max(0.0, state.AttackCooldown - deltaTime)
    state.HitCooldown = math.max(0.0, state.HitCooldown - deltaTime)

    if not owner:IsInputEnabled() then
        return
    end

    if Input.IsMouseDown(MouseButton.Right) then
        local dx, dy = Input.GetMouseDelta()
        owner:AddYawInput(dx * MouseSensitivity)
        owner:AddPitchInput(dy * MouseSensitivity)
    end

    local x = 0.0
    local y = 0.0

    if Input.IsKeyDown(KeyCode.W) then x = x + 1.0 end
    if Input.IsKeyDown(KeyCode.S) then x = x - 1.0 end
    if Input.IsKeyDown(KeyCode.D) then y = y + 1.0 end
    if Input.IsKeyDown(KeyCode.A) then y = y - 1.0 end

    owner:Move(Vector.new(x, y, 0.0))

    if Input.IsKeyPressed(KeyCode.Space) then
        owner:Jump(JumpSpeed)
    end

    if Input.IsMousePressed(MouseButton.Left) and state.AttackCooldown <= 0.0 then
        owner:StartWandSwing()
        owner:PlayerShot()
        state.AttackCooldown = AttackInterval
    end
end

function OnComponentBeginOverlap(other)
    if other == nil or other:GetName() ~= "HitTriggerCollision" or state.HitCooldown > 0.0 then
        return
    end

    local TriggerActor = other:GetOwner()
    if TriggerActor == nil then
        return
    end

    owner:PlayDamageEffect()
    owner:ApplyKnockback(TriggerActor, 6.0)
    state.HitCooldown = HitCooldown
    Log("[AnimationTest] Hit Triggered")
end

function OnComponentEndOverlap(other)
end

function OnComponentHit(other, result, bSelfMoved)
end

function EndPlay()
end
