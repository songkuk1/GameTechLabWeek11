local AnimationGraphPath = "AnimationTest.animgraph"
local ChaseSpeed = 3.5
local AttackRange = 1.85
local AttackStopDistance = 1.55
local AttackWindup = 0.32
local AttackInterval = 1.15
local AttackDamage = 20.0
local AnimationSpeedScale = 60.0
local AnimationSpeedInterp = 7.0

local AnimationComponent = nil

local function FindPlayer()
    return GamePlayStatics.GetActorOfClass(owner, Player.StaticClass())
end

local function GetHorizontalDistanceAndDirection(TargetPlayer)
    local Direction = TargetPlayer:GetActorLocation() - owner:GetActorLocation()
    Direction.z = 0.0

    local Distance = math.sqrt(Direction.x * Direction.x + Direction.y * Direction.y)
    if Distance <= 0.001 then
        return 0.0, Vector.new(0.0, 0.0, 0.0)
    end

    return Distance, Direction * (1.0 / Distance)
end

local function StopMoving()
    owner:MoveToward(Vector.new(0.0, 0.0, 0.0), 0.0)
end

local function ProcessPendingDamage()
    local Damage = state.PendingDamage or 0.0
    if Damage <= 0.0 or state.bIsDead then
        return
    end

    state.PendingDamage = 0.0
    state.CurrentHp = math.max(0.0, state.CurrentHp - Damage)

    state.AttackResolveTimer = 0.0
    state.bAttackDamageResolved = true

    Log("[AnimationTestEnemy] Hit: -" .. tostring(Damage) .. " HP: " .. tostring(state.CurrentHp) .. "/" .. tostring(state.MaxHp))

    if state.CurrentHp <= 0.0 then
        state.bIsDead = true
        state.bDeathAnimationPending = true
        Log("[AnimationTestEnemy] Defeated")
    else
        state.bHitAnimationPending = true
    end
end

local function BeginAttack()
    if not AnimationComponent or AnimationComponent:IsAnimationBlending() then
        return false
    end

    if AnimationComponent:GetCurrentAnimationState() ~= "Locomotion" then
        return false
    end

    if not AnimationComponent:RequestAnimationState("Jab") then
        state.AttackCooldown = 0.25
        Log("[AnimationTestEnemy] Could not request Jab state. Check Locomotion -> Jab transition.")
        return false
    end

    state.AttackCooldown = AttackInterval
    state.AttackResolveTimer = AttackWindup
    state.bAttackDamageResolved = false
    Log("[AnimationTestEnemy] Attack windup")
    return true
end

local function ResolveAttack(TargetPlayer)
    if state.bAttackDamageResolved then
        return
    end

    state.bAttackDamageResolved = true

    if not TargetPlayer then
        return
    end

    local Distance = GetHorizontalDistanceAndDirection(TargetPlayer)
    if Distance > AttackRange then
        Log("[AnimationTestEnemy] Attack missed: out of range")
        return
    end

    local bApplied = GamePlayStatics.ApplyMeleeDamage(TargetPlayer, owner, AttackDamage)
    if bApplied then
        Log("[AnimationTestEnemy] Player hit")
    else
        Log("[AnimationTestEnemy] Attack avoided or damage cooldown active")
    end
end

function Init()
    state.AnimationReady = false
    state.AttackCooldown = 0.0
    state.AttackResolveTimer = 0.0
    state.bAttackDamageResolved = true
    state.AnimationSpeed = 0.0

    state.MaxHp = 100.0
    state.CurrentHp = state.MaxHp
    state.PendingDamage = 0.0
    state.bIsDead = false
    state.bDeathAnimationPending = false
    state.bHitAnimationPending = false
end

function BeginPlay()
    AnimationComponent = owner:FindComponentByClass(SkeletalMeshComponent.StaticClass())
    if not AnimationComponent then
        Log("[AnimationTestEnemy] SkeletalMeshComponent not found; add one to AEnemyActor")
        return
    end

    if not AnimationComponent:SetAnimationGraphByPath(AnimationGraphPath, true) then
        Log("[AnimationTestEnemy] Failed to initialize Animation Graph")
        AnimationComponent = nil
        return
    end

    state.AnimationReady = true
    Log("[AnimationTestEnemy] Animation Graph ready: " .. AnimationComponent:GetCurrentAnimationState())
end

function Tick(deltaTime)
    state.AttackCooldown = math.max(0.0, state.AttackCooldown - deltaTime)

    ProcessPendingDamage()

    if state.bIsDead then
        StopMoving()

        if state.AnimationReady and state.bDeathAnimationPending and not AnimationComponent:IsAnimationBlending() then
            if AnimationComponent:RequestAnimationState("Death") then
                state.bDeathAnimationPending = false
            end
        end

        return
    end

    if state.bHitAnimationPending and state.AnimationReady and not AnimationComponent:IsAnimationBlending() then
        if AnimationComponent:RequestAnimationState("Hit") then
            state.bHitAnimationPending = false
            StopMoving()
            return
        end
    end

    local TargetPlayer = FindPlayer()
    if not TargetPlayer then
        StopMoving()
        return
    end

    local PlayerScript = TargetPlayer:FindComponentByName("UScriptComponent")
    if PlayerScript then
        local PlayerState = PlayerScript:GetLuaTable()

        if PlayerState["bIsDead"] then
            StopMoving()

            if state.AnimationReady and AnimationComponent:GetCurrentAnimationState() ~= "Dance" and not AnimationComponent:IsAnimationBlending() then
                AnimationComponent:RequestAnimationState("Dance")
            end

            return
        end
    end

    local Distance, Direction = GetHorizontalDistanceAndDirection(TargetPlayer)
    local CurrentState = state.AnimationReady and AnimationComponent:GetCurrentAnimationState() or "Locomotion"
    if state.LastLoggedAnimationState ~= CurrentState then
        Log("[AnimationTestEnemy] State: " .. CurrentState)
        state.LastLoggedAnimationState = CurrentState
    end
    if state.AnimationReady then
        local TargetSpeed = (CurrentState == "Locomotion" and Distance > AttackStopDistance) and 300.0 or 0.0
        local SpeedAlpha = math.min(deltaTime * AnimationSpeedInterp, 1.0)
        state.AnimationSpeed = state.AnimationSpeed + (TargetSpeed - state.AnimationSpeed) * SpeedAlpha
        AnimationComponent:SetAnimationFloat("Speed", state.AnimationSpeed)
    end

    if CurrentState == "Hit" then
        StopMoving()

        if AnimationComponent:IsCurrentAnimationStateFinished() and not AnimationComponent:IsAnimationBlending() then
            AnimationComponent:RequestAnimationState("Locomotion")
        end

        return
    end

    if CurrentState == "Jab" then
        StopMoving()

        if state.AttackResolveTimer > 0.0 then
            state.AttackResolveTimer = math.max(0.0, state.AttackResolveTimer - deltaTime)
            if state.AttackResolveTimer <= 0.0 then
                ResolveAttack(FindPlayer())
            end
        end

        if AnimationComponent and state.AttackResolveTimer <= 0.0 and AnimationComponent:IsCurrentAnimationStateFinished() and not AnimationComponent:IsAnimationBlending() then
            AnimationComponent:RequestAnimationState("Locomotion")
        end
        return
    end

    if Distance > AttackStopDistance then
        owner:MoveToward(Direction, ChaseSpeed)
        return
    end

    StopMoving()

    if state.AnimationReady and Distance <= AttackRange and state.AttackCooldown <= 0.0 then
        BeginAttack()
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
