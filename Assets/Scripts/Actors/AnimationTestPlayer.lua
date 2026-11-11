local WalkMoveSpeed = 5.0
local RunMoveSpeed = 10.0
local AnimationSpeedScale = 60.0
local AnimationSpeedInterp = 6.0
local MouseSensitivity = 0.1

local RollMoveSpeed = 9.0
local RollMoveDuration = 0.42
local RollInvulnerabilityDuration = 0.55
local RollCooldownDuration = 1.0

local HitCooldownDuration = 0.45
local HitKnockbackStrength = 6.0
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
    AnimationComponent:SetAnimationBool("bIsRolling", CurrentState == "Roll")

    if CurrentState == "Jab" and state.bComboRequested then
        QueueAnimationState("Straight")
    end

    if not state.PendingAnimationState then
        if CurrentState == "Roll" and AnimationComponent:IsCurrentAnimationStateFinished() then
            QueueAnimationState("Locomotion")
        elseif (CurrentState == "JumpStart" or CurrentState == "JumpLoop") and owner:IsGrounded() then
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
        elseif NextState == "Locomotion" then
            state.bRolling = false
        end
    end
end

local function IsInputLocked(CurrentState)
    if CurrentState == "Roll" or CurrentState == "JumpLand" or CurrentState == "Jab" or CurrentState == "Straight" or CurrentState == "Hit" then
        return true
    end

    local PendingState = state.PendingAnimationState
    if PendingState == "Roll" or PendingState == "JumpLand" or PendingState == "Jab" or PendingState == "Straight" or PendingState == "Hit" then
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

local function ProcessPendingMeleeDamage()
    local Damage = state.PendingMeleeDamage or 0.0
    if Damage <= 0.0 then
        return
    end

    local DamageSource = state.PendingMeleeSource
    state.PendingMeleeDamage = 0.0
    state.PendingMeleeSource = nil

    if state.bIsInvulnerable then
        return
    end

    state.CurrentHp = math.max(0.0, state.CurrentHp - Damage)

    if state.AnimationReady then
        QueueAnimationState("Hit")
    end

    owner:PlayDamageEffect()
    if DamageSource ~= nil then
        owner:ApplyKnockback(DamageSource, HitKnockbackStrength)
    end

    Log("[AnimationTest] Player hit: -" .. tostring(Damage) .. " HP: " .. tostring(state.CurrentHp) .. "/" .. tostring(state.MaxHp))

    if state.CurrentHp <= 0.0 then
        state.bIsDead = true
        Log("[AnimationTest] Player defeated")
    end
end

local function GetMoveInput()
    local x = 0.0
    local y = 0.0

    if Input.IsKeyDown(KeyCode.W) then x = x + 1.0 end
    if Input.IsKeyDown(KeyCode.S) then x = x - 1.0 end
    if Input.IsKeyDown(KeyCode.D) then y = y + 1.0 end
    if Input.IsKeyDown(KeyCode.A) then y = y - 1.0 end

    local Length = math.sqrt(x * x + y * y)
    if Length > 1.0 then
        x = x / Length
        y = y / Length
    end

    return Vector.new(x, y, 0.0)
end

local function StartRoll(MoveInput)
    local x = MoveInput.x
    local y = MoveInput.y

    if math.abs(x) + math.abs(y) < 0.001 then
        x = 1.0
        y = 0.0
    end

    local Length = math.sqrt(x * x + y * y)
    state.RollDirection = Vector.new(x / Length, y / Length, 0.0)
    state.RollMoveRemaining = RollMoveDuration
    state.RollInvulnerabilityRemaining = RollInvulnerabilityDuration
    state.RollCooldown = RollCooldownDuration
    if not AnimationComponent or not AnimationComponent:RequestAnimationState("Roll") then
        Log("[AnimationTest] Roll state or Locomotion -> Roll transition is missing")
        return false
    end

    state.PendingAnimationState = nil
    state.bRolling = true
    state.bIsInvulnerable = true
    Log("[AnimationTest] Roll started")
    return true
end

function Init()
    state.PendingAnimationState = nil
    state.AnimationReady = false
    state.bComboWindow = false
    state.bComboRequested = false
    state.bRecoveryBlend = false
    state.AnimationSpeed = 0.0

    state.MaxHp = 100.0
    state.CurrentHp = 100.0
    state.bIsDead = false
    state.PendingMeleeDamage = 0.0
    state.PendingMeleeSource = nil
    state.HitCooldown = 0.0

    state.bRolling = false
    state.bIsInvulnerable = false
    state.RollDirection = Vector.new(0.0, 0.0, 0.0)
    state.RollMoveRemaining = 0.0
    state.RollInvulnerabilityRemaining = 0.0
    state.RollCooldown = 0.0
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
    state.HitCooldown = math.max(0.0, (state.HitCooldown or 0.0) - deltaTime)
    state.RollCooldown = math.max(0.0, state.RollCooldown - deltaTime)
    state.RollMoveRemaining = math.max(0.0, state.RollMoveRemaining - deltaTime)
    state.RollInvulnerabilityRemaining = math.max(0.0, state.RollInvulnerabilityRemaining - deltaTime)
    state.bIsInvulnerable = state.RollInvulnerabilityRemaining > 0.0

    ProcessPendingMeleeDamage()

    if state.bIsDead then
        owner:SetMovementSpeed(0.0)
        owner:Move(Vector.new(0.0, 0.0, 0.0))
        return
    end

    if state.AnimationReady then
        local TargetAnimationSpeed = owner:GetMovementSpeed2D() * AnimationSpeedScale
        local SpeedAlpha = math.min(deltaTime * AnimationSpeedInterp, 1.0)
        state.AnimationSpeed = state.AnimationSpeed + (TargetAnimationSpeed - state.AnimationSpeed) * SpeedAlpha
        AnimationComponent:SetAnimationFloat("Speed", state.AnimationSpeed)
        UpdateAnimationState()
    end

    if not owner:IsInputEnabled() then
        owner:SetMovementSpeed(0.0)
        owner:Move(Vector.new(0.0, 0.0, 0.0))
        return
    end

    -- 우클릭을 누르는 동안 카메라를 자유롭게 회전한다.
    -- SpringArmComponent의 bUsePawnControlRotation도 true여야 한다.
    if not state.bRolling and Input.IsMouseDown(MouseButton.Right) then
        local dx, dy = Input.GetMouseDelta()
        owner:AddYawInput(dx * MouseSensitivity)
        owner:AddPitchInput(dy * MouseSensitivity)
    end

    local CurrentState = state.AnimationReady and AnimationComponent:GetCurrentAnimationState() or "Locomotion"

    if state.AnimationReady and Input.IsKeyPressed(KeyCode.Space) and state.RollCooldown <= 0.0 and CurrentState == "Locomotion" and not state.PendingAnimationState and not AnimationComponent:IsAnimationBlending() and owner:IsGrounded() then
        StartRoll(GetMoveInput())
    end

    if state.AnimationReady and Input.IsMousePressed(MouseButton.Left) then
        if CurrentState == "Locomotion" and not state.bRolling and not IsInputLocked(CurrentState) and not state.PendingAnimationState and not AnimationComponent:IsAnimationBlending() then
            QueueAnimationState("Jab")
        elseif CurrentState == "Jab" and state.bComboWindow and not state.bComboRequested then
            state.bComboRequested = true
        end
    end

    if state.bRolling then
        if state.RollMoveRemaining > 0.0 then
            owner:SetMovementSpeed(RollMoveSpeed)
            owner:Move(state.RollDirection)
        else
            owner:SetMovementSpeed(0.0)
            owner:Move(Vector.new(0.0, 0.0, 0.0))
        end
        return
    end

    local bInputLocked = state.AnimationReady and IsInputLocked(CurrentState)
    local MoveInput = bInputLocked and Vector.new(0.0, 0.0, 0.0) or GetMoveInput()

    owner:SetMovementSpeed(Input.IsKeyDown(KeyCode.Shift) and RunMoveSpeed or WalkMoveSpeed)
    owner:Move(MoveInput)
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
end

function OnComponentEndOverlap(other)
end

function OnComponentHit(other, result, bSelfMoved)
end

function EndPlay()
    AnimationComponent = nil
end
