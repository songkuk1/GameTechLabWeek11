local MouseSensitivity = 0.1
local JumpSpeed = 9.0
local AttackInterval = 0.7
local HitCooldown = 0.8
local WalkMoveSpeed = 5.0
local RunMoveSpeed = 10.0
local AnimationSpeedScale = 60.0
local AnimationGraphPath = "AnimationTest.animgraph"
local AnimationComponent = nil

local function QueueAnimationState(Name)
    if Name == "Hit" then
        state.PendingAnimationState = Name
    elseif state.PendingAnimationState ~= "Hit" then
        state.PendingAnimationState = Name
    end
end

local function UpdateAnimationState()
    if not AnimationComponent then
        return
    end

    local CurrentState = AnimationComponent:GetCurrentAnimationState()

    AnimationComponent:SetAnimationBool("bIsAttacking", CurrentState == "Attack")
    AnimationComponent:SetAnimationBool("bIsHit", CurrentState == "Hit")
    AnimationComponent:SetAnimationBool("bIsJumping", CurrentState == "JumpStart" or CurrentState == "JumpLoop")
    AnimationComponent:SetAnimationBool("bIsLanded", CurrentState == "JumpLand")

    if not state.PendingAnimationState then
        if (CurrentState == "JumpStart" or CurrentState == "JumpLoop") and owner:IsGrounded() then
            QueueAnimationState("JumpLand")
        elseif CurrentState == "JumpStart" and AnimationComponent:IsCurrentAnimationStateFinished() then
            QueueAnimationState("JumpLoop")
        elseif (CurrentState == "JumpLand" or CurrentState == "Attack" or CurrentState == "Hit") and AnimationComponent:IsCurrentAnimationStateFinished() then
            QueueAnimationState("Locomotion")
        end
    end

    local NextState = state.PendingAnimationState

    if not NextState or AnimationComponent:IsAnimationBlending() then
        return
    end

    if AnimationComponent:RequestAnimationState(NextState) then
        state.PendingAnimationState = nil
    end
end

function Init()
    state.PendingAnimationState = nil
    state.AnimationReady = false
    state.AttackCooldown = 0.0
    state.HitCooldown = 0.0
end

function BeginPlay()
    AnimationComponent = owner:FindComponentByClass(SkeletalMeshComponent.StaticClass())

    if not AnimationComponent then
        Log("[AnimationTest] SkeletalMeshComponent not found")
        return
    end

    if not AnimationComponent:SetAnimationGraphByPath(AnimationGraphPath, true) then
        Log("[AnimationTest] Failed to initialize Animation Graph")
        AnimationComponent = nil
        return
    end

    state.AnimationReady = true
    owner:SetMovementSpeed(WalkMoveSpeed)

    Log("[AnimationTest] Animation Graph ready")
end

function Tick(deltaTime)
    state.AttackCooldown = math.max(0.0, state.AttackCooldown - deltaTime)
    state.HitCooldown = math.max(0.0, state.HitCooldown - deltaTime)

    if state.AnimationReady then
        AnimationComponent:SetAnimationFloat("Speed", owner:GetMovementSpeed2D() * AnimationSpeedScale)
        UpdateAnimationState()
    end

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

    owner:SetMovementSpeed(Input.IsKeyDown(KeyCode.Shift) and RunMoveSpeed or WalkMoveSpeed)
    owner:Move(Vector.new(x, y, 0.0))

    if Input.IsKeyPressed(KeyCode.Space) and owner:IsGrounded() then
        local bCanPlayJump = not state.AnimationReady or AnimationComponent:GetCurrentAnimationState() == "Locomotion"

        if bCanPlayJump then
            owner:Jump(JumpSpeed)

            if state.AnimationReady then
                QueueAnimationState("JumpStart")
            end
        end
    end

    if Input.IsMousePressed(MouseButton.Left) and state.AttackCooldown <= 0.0 then
        if state.AnimationReady and AnimationComponent:GetCurrentAnimationState() == "Locomotion" then
            QueueAnimationState("Attack")
        end

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

    if state.AnimationReady then
        QueueAnimationState("Hit")
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
    AnimationComponent = nil
end
