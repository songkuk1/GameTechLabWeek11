local JumpSpeed = 9.0
local HitCooldown = 0.8
local WalkMoveSpeed = 5.0
local RunMoveSpeed = 10.0
local AnimationSpeedScale = 60.0
local AnimationGraphPath = "AnimationTest.animgraph"
local AnimationComponent = nil

local function QueueAnimationState(Name)
    if Name == "Hit" then
        state.bComboWindow = false
        state.bComboRequested = false
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

    AnimationComponent:SetAnimationBool("bIsAttacking", CurrentState == "Jab" or CurrentState == "Straight")
    AnimationComponent:SetAnimationBool("bIsHit", CurrentState == "Hit")
    AnimationComponent:SetAnimationBool("bIsJumping", CurrentState == "JumpStart" or CurrentState == "JumpLoop")
    AnimationComponent:SetAnimationBool("bIsLanded", CurrentState == "JumpLand")

    if CurrentState == "Jab" and state.bComboRequested then
        QueueAnimationState("Straight")
    end

    if not state.PendingAnimationState then
        if (CurrentState == "JumpStart" or CurrentState == "JumpLoop") and owner:IsGrounded() then
            QueueAnimationState("JumpLand")
        elseif CurrentState == "JumpStart" and AnimationComponent:IsCurrentAnimationStateFinished() then
            QueueAnimationState("JumpLoop")
        elseif (CurrentState == "JumpLand" or CurrentState == "Jab" or CurrentState == "Straight" or CurrentState == "Hit") and AnimationComponent:IsCurrentAnimationStateFinished() then
            QueueAnimationState("Locomotion")
        end
    end

    local NextState = state.PendingAnimationState

    if not NextState or AnimationComponent:IsAnimationBlending() then
        return
    end

    if AnimationComponent:RequestAnimationState(NextState) then
        state.PendingAnimationState = nil
        state.bRecoveryBlend = NextState == "Locomotion"

        if NextState == "Straight" then
            state.bComboRequested = false
            state.bComboWindow = false
        end
    end
end

local function IsInputLocked(CurrentState)
    if CurrentState == "JumpLand" or CurrentState == "Jab" or CurrentState == "Straight" or CurrentState == "Hit" then
        return true
    end

    local PendingState = state.PendingAnimationState

    if PendingState == "JumpLand" or PendingState == "Jab" or PendingState == "Straight" or PendingState == "Hit" then
        return true
    end

    if state.bRecoveryBlend then
        if AnimationComponent:IsAnimationBlending() then
            return true
        end

        state.bRecoveryBlend = false
    end

    return false
end

function Init()
    state.PendingAnimationState = nil
    state.AnimationReady = false
    state.HitCooldown = 0.0
    state.bComboWindow = false
    state.bComboRequested = false
    state.bRecoveryBlend = false
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
    state.HitCooldown = math.max(0.0, state.HitCooldown - deltaTime)

    if state.AnimationReady then
        AnimationComponent:SetAnimationFloat("Speed", owner:GetMovementSpeed2D() * AnimationSpeedScale)
        UpdateAnimationState()
    end

    if not owner:IsInputEnabled() then
        owner:MoveRelativeToActor(Vector.new(0.0, 0.0, 0.0))
        return
    end

    local CurrentState = state.AnimationReady and AnimationComponent:GetCurrentAnimationState() or "Locomotion"

    if state.AnimationReady and Input.IsKeyPressed(KeyCode.Space) and CurrentState == "Locomotion" and not state.PendingAnimationState and not AnimationComponent:IsAnimationBlending() and owner:IsGrounded() then
        owner:Jump(JumpSpeed)
        QueueAnimationState("JumpStart")
    end

    if state.AnimationReady and Input.IsMousePressed(MouseButton.Left) then
        if CurrentState == "Locomotion" and not IsInputLocked(CurrentState) and not state.PendingAnimationState and not AnimationComponent:IsAnimationBlending() then
            QueueAnimationState("Jab")
        elseif CurrentState == "Jab" and state.bComboWindow and not state.bComboRequested then
            state.bComboRequested = true
        end
    end

    local bInputLocked = state.AnimationReady and IsInputLocked(CurrentState)

    local x = 0.0
    local y = 0.0

    if not bInputLocked then
        if Input.IsKeyDown(KeyCode.W) then x = x + 1.0 end
        if Input.IsKeyDown(KeyCode.S) then x = x - 1.0 end
        if Input.IsKeyDown(KeyCode.D) then y = y + 1.0 end
        if Input.IsKeyDown(KeyCode.A) then y = y - 1.0 end
    end

    owner:SetMovementSpeed(Input.IsKeyDown(KeyCode.Shift) and RunMoveSpeed or WalkMoveSpeed)
    owner:MoveRelativeToActor(Vector.new(x, y, 0.0))
end

function OnAnimNotifyState(NotifyName, EventType, deltaTime)
    if NotifyName ~= "ComboWindow" then
        return
    end

    if EventType == "Begin" then
        state.bComboWindow = true
        Log("[AnimationTest] Combo Window Open")
    elseif EventType == "End" then
        state.bComboWindow = false
        Log("[AnimationTest] Combo Window Closed")
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
