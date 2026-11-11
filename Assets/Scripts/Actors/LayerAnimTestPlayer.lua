local JumpSpeed = 9.0
local HitCooldown = 0.8
local WalkMoveSpeed = 5.0
local RunMoveSpeed = 10.0
local WalkAnimSpeedScale = 30.0 -- 5.0 * 30.0 = 150.0 (Walk Sample)
local RunAnimSpeedScale = 45.0  -- 10.0 * 45.0 = 450.0 (Jog Sample)
local AnimationSpeedInterp = 8.0
local AnimationGraphPath = "LayerAnimationGraph.animgraph"
local AnimationComponent = nil

local function IsInputLocked(CurrentState)
    -- 피격(Hit)이나 착지(Jump_Land) 중에는 이동 입력을 잠금
    -- 공격(Attack)은 상/하체 분리(Layered Bone Blend)이므로 이동 가능!
    if CurrentState == "Hit" or CurrentState == "Jump_Land" then
        return true
    end
    return false
end

function Init()
    state.AnimationReady = false
    state.HitCooldown = 0.0
    state.AnimationSpeed = 0.0
end

function BeginPlay()
    AnimationComponent = owner:FindComponentByClass(SkeletalMeshComponent.StaticClass())

    if not AnimationComponent then
        Log("[LayerAnimTest] SkeletalMeshComponent not found")
        return
    end

    -- false: 애님 그래프에 설정된 트랜지션 조건(bIsAttacking, bIsJumping, bIsGrounded, State Finished 등)으로 자동 전이
    if not AnimationComponent:SetAnimationGraphByPath(AnimationGraphPath, false) then
        Log("[LayerAnimTest] Failed to initialize LayerAnimationGraph")
        AnimationComponent = nil
        return
    end

    state.AnimationReady = true
    owner:SetMovementSpeed(WalkMoveSpeed)

    Log("[LayerAnimTest] LayerAnimationGraph ready")
end

function Tick(deltaTime)
    state.HitCooldown = math.max(0.0, state.HitCooldown - deltaTime)

    local bRunning = Input.IsKeyDown(KeyCode.Shift)

    if state.AnimationReady and AnimationComponent then
        local CurrentState = AnimationComponent:GetCurrentAnimationState()

        -- 1. 이동 속도(Speed) 파라미터 업데이트 (Walk: 150, Jog: 450)
        local SpeedScale = bRunning and RunAnimSpeedScale or WalkAnimSpeedScale
        local TargetAnimationSpeed = owner:GetMovementSpeed2D() * SpeedScale
        local SpeedAlpha = math.min(deltaTime * AnimationSpeedInterp, 1.0)
        state.AnimationSpeed = state.AnimationSpeed + (TargetAnimationSpeed - state.AnimationSpeed) * SpeedAlpha
        AnimationComponent:SetAnimationFloat("Speed", state.AnimationSpeed)

        -- 2. 착지 여부(bIsGrounded) 실시간 업데이트 (Jump_Loop -> Jump_Land 전이용)
        AnimationComponent:SetAnimationBool("bIsGrounded", owner:IsGrounded())

        -- 3. 이미 해당 상태에 진입했다면 트리거용 Bool 파라미터를 false로 리셋하여 무한 루프 방지
        if CurrentState == "Jump_Start" or CurrentState == "Jump_Loop" then
            AnimationComponent:SetAnimationBool("bIsJumping", false)
        end
        if CurrentState == "Attack" then
            AnimationComponent:SetAnimationBool("bIsAttacking", false)
        end
        if CurrentState == "Hit" then
            AnimationComponent:SetAnimationBool("bIsHit", false)
        end
    end

    if not owner:IsInputEnabled() then
        owner:MoveRelativeToActor(Vector.new(0.0, 0.0, 0.0))
        return
    end

    local CurrentState = (state.AnimationReady and AnimationComponent) and AnimationComponent:GetCurrentAnimationState() or "Locomotion"

    -- 스페이스바: 점프 (Locomotion -> Jump_Start -> Jump_Loop -> Jump_Land -> Locomotion)
    if state.AnimationReady and Input.IsKeyPressed(KeyCode.Space) and CurrentState == "Locomotion" and owner:IsGrounded() then
        owner:Jump(JumpSpeed)
        AnimationComponent:SetAnimationBool("bIsJumping", true)
    end

    -- 마우스 좌클릭: 상체 레이어 공격 (Locomotion -> Attack -> Locomotion)
    -- 이동 중에도 공격 가능하며, 공격 중에도 하체는 계속 이동 애니메이션 재생!
    if state.AnimationReady and Input.IsMousePressed(MouseButton.Left) then
        if CurrentState == "Locomotion" then
            AnimationComponent:SetAnimationBool("bIsAttacking", true)
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

    owner:SetMovementSpeed(bRunning and RunMoveSpeed or WalkMoveSpeed)
    owner:MoveRelativeToActor(Vector.new(x, y, 0.0))
end

function OnComponentBeginOverlap(other)
    if other == nil or other:GetName() ~= "HitTriggerCollision" or state.HitCooldown > 0.0 then
        return
    end

    local TriggerActor = other:GetOwner()
    if TriggerActor == nil then
        return
    end

    if state.AnimationReady and AnimationComponent then
        AnimationComponent:SetAnimationBool("bIsHit", true)
    end
    owner:PlayDamageEffect()
    owner:ApplyKnockback(TriggerActor, 6.0)
    state.HitCooldown = HitCooldown
    Log("[LayerAnimTest] Hit Triggered")
end

function OnComponentEndOverlap(other)
end

function OnComponentHit(other, result, bSelfMoved)
end

function EndPlay()
    AnimationComponent = nil
end
