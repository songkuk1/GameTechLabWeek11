-- 슬라임: 체력이 없다(죽지 않는다). 맞을 때마다 코인을 쏟아내고, 주기적으로 플레이어에게 탄을 쏜다.

local CoinsPerHit = 3          -- 한 번 맞을 때 쏟아내는 코인 수
local CoinBurstSpeed = 7.0     -- 코인이 튀어 오르는 속도
local CoinBurstHeight = 3.0    -- 슬라임 중심에서 코인이 나오는 높이
local AttackInterval = 0.3

local ShockwaveProjectileCount = 50
local ShockwaveProjectileSpeed = 25.0
local ShockwaveSpawnDistance = 2.0
local ShockwaveSpawnHeight = 1.0

local function SingleAttackLoop()
    while true do
        if _G.GamePhase == "Playing" and not state.IsShockwaveAttacking then
            local TargetPlayer = GamePlayStatics.GetActorOfClass(owner, Player.StaticClass())

            if TargetPlayer ~= nil then
                local Forward = TargetPlayer:GetActorLocation() - owner:GetActorLocation()
                Forward = Forward:Normalized()

                local Location = owner:GetActorLocation() + Forward * 3.0 + Vector.new(0.0, 0.0, 1.5)
                local Rotation = Rotator.new(0, 0, 0)
                local Scale = Vector.new(0.5, 0.5, 0.5)

                local Bullet = owner:GetWorld():SpawnActor(Bullet.StaticClass(), Transform.new(Location, Scale, Rotation))
                Bullet:SetTarget(TargetPlayer)
            end
            GamePlayStatics.PlaySound("Assets/Sounds/NPC_Hit_22.wav", 0.05)
        end
        coroutine.yield(WaitForSeconds.new(AttackInterval))
    end
end

local function SpawnShockwave()
    local TargetPlayer = GamePlayStatics.GetActorOfClass(owner, Player.StaticClass())
    if TargetPlayer == nil then
        return
    end

    local OwnerLocation = owner:GetActorLocation()
    local PlayerDirection = (TargetPlayer:GetActorLocation() - OwnerLocation):Normalized()

    for i = 0, ShockwaveProjectileCount - 1 do
        local Angle = (2.0 * math.pi * i) / ShockwaveProjectileCount

        local RadialDirection = Vector.new(math.cos(Angle), math.sin(Angle), 0.0)
        local Direction = Vector.new(RadialDirection.x, RadialDirection.y, PlayerDirection.z):Normalized()
        local Location = OwnerLocation + RadialDirection * ShockwaveSpawnDistance + Vector.new(0.0, 0.0, ShockwaveSpawnHeight)

        local Transform = Transform.new(Location, Vector.new(0.5, 0.5, 0.5), Rotator.new(0.0, 0.0, 0.0))

        local BulletActor = owner:GetWorld():SpawnActor(Bullet.StaticClass(), Transform)
        if BulletActor ~= nil then
            BulletActor:SetDirection(Direction, ShockwaveProjectileSpeed)
        end
    end
end

local function ShockwaveAttackLoop()
	 while true do
        if _G.GameOver then
            return
        end

        state.IsShockwaveAttacking = true
        owner:Jump(13.0)

        coroutine.yield(WaitUntil.new(
            function()
                return not owner:IsGrounded()
            end
        ))

        coroutine.yield(WaitUntil.new(
            function()
                return owner:IsGrounded()
            end
        ))
        if _G.GameOver then
            return
        end

        owner:PlayHitSquash()
        owner:PlaySlamCameraShake()
        SpawnShockwave()
        state.IsShockwaveAttacking = false
        GamePlayStatics.PlaySound("Assets/Sounds/Shockwave.wav", 1.0)

        coroutine.yield(WaitForSeconds.new(10.0))

    end
end


local function JumpLoop()
    while true do
        if _G.GameOver then
            return
        end

        if state.IsShockwaveAttacking then
            coroutine.yield(WaitForSeconds.new(0.1))
        else
            owner:Jump(5.0)

            coroutine.yield(WaitUntil.new(
                function()
                    return not owner:IsGrounded()
                end
            ))

            coroutine.yield(WaitUntil.new(
                function()
                    return owner:IsGrounded()
                end
            ))
            owner:PlayHitSquash()

            coroutine.yield(WaitForSeconds.new(3.0))
        end
    end
end

function Init()
    state.IsShockwaveAttacking = false
end

function BeginPlay()
    if state.ShockwaveAttackCoroutine ~= nil then
        StopCoroutine(state.ShockwaveAttackCoroutine)
    end
    if state.SingleAttackCoroutine ~= nil then
        StopCoroutine(state.SingleAttackCoroutine)
    end
    if state.JumpCoroutine ~= nil then
        StopCoroutine(state.JumpCoroutine)
    end

    state.SingleAttackCoroutine = StartCoroutine(SingleAttackLoop)
    state.JumpCoroutine = StartCoroutine(JumpLoop)
    state.ShockwaveAttackCoroutine = StartCoroutine(ShockwaveAttackLoop)
end

function Tick(deltaTime)

end

function OnComponentBeginOverlap(other)
    if _G.GamePhase ~= "Playing" then
        return
    end

    -- 플레이어 탄에 맞으면 체력 대신 코인을 쏟아낸다
    if other:GetName() == "PlayerProjectileCollision" then
        owner:PlayHitSquash()
        CoinLauncher.Burst(owner, CoinsPerHit, CoinBurstSpeed, CoinBurstHeight)
        GamePlayStatics.PlaySound("Assets/Sounds/EnemyHit.wav", 0.5)
    end
end

function OnComponentEndOverlap(other)

end

function EndPlay()
    if state.SingleAttackCoroutine ~= nil then
        StopCoroutine(state.SingleAttackCoroutine)
        state.SingleAttackCoroutine = nil
    end
    if state.JumpCoroutine ~= nil then
        StopCoroutine(state.JumpCoroutine)
        state.JumpCoroutine = nil
    end
    if state.ShockwaveAttackCoroutine ~= nil then
        StopCoroutine(state.ShockwaveAttackCoroutine)
        state.ShockwaveAttackCoroutine = nil
    end
end
