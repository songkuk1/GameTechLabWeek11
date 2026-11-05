-- 네모네모 월드 메인 게임 컨트롤러
-- 규칙: 제한 시간 안에 슬라임을 맞혀 쏟아지는 코인을 최대한 많이 모은다.
-- 끝: 시간이 다 되거나 플레이어 체력이 0이 되면 결과를 보여 주고 타이틀로 돌아간다.

local GameTime = 90.0          -- 제한 시간(초)
local ResultShowTime = 4.0     -- 결과 화면을 보여 주는 시간(초)

-- UI 좌표: Anchor/Pivot의 y는 0이 화면 위, 1이 화면 아래
-- 네모네모 월드 느낌을 내려고 모든 패널을 "네모 블록"처럼 쌓아서 그린다.
--   그림자 → 진한 나무색 테두리 → 크림색 면 → 위쪽 밝은 띠 → 아래쪽 어두운 띠

-- 색상표 (R, G, B, A)
local Pal = {
    Shadow    = { 0.0, 0.0, 0.0, 0.28 },
    Outline   = { 0.25, 0.16, 0.09, 1 },
    Face      = { 0.98, 0.93, 0.78, 1 },
    FaceLight = { 1.0, 0.98, 0.9, 1 },
    FaceDark  = { 0.86, 0.74, 0.53, 1 },
    Ink       = { 0.33, 0.2, 0.1, 1 },
    InkSoft   = { 0.58, 0.42, 0.26, 1 },
    White     = { 1, 1, 1, 1 },
    Grass     = { 0.33, 0.66, 0.24, 1 },
    GrassLt   = { 0.47, 0.8, 0.34, 1 },
    GrassDk   = { 0.22, 0.5, 0.16, 1 },
    Track     = { 0.36, 0.25, 0.16, 1 },
    Gold      = { 1, 0.78, 0.15, 1 },
    GoldLt    = { 1, 0.93, 0.55, 1 },
    GoldDk    = { 0.82, 0.52, 0.06, 1 },
    GoldEdge  = { 0.47, 0.26, 0.05, 1 },
    Red       = { 0.9, 0.25, 0.25, 1 },
    RedLt     = { 1, 0.6, 0.6, 1 },
    RedDk     = { 0.62, 0.12, 0.14, 1 },
    Leaf      = { 0.36, 0.75, 0.25, 1 },
    LeafLt    = { 0.56, 0.88, 0.4, 1 },
}

-- 위젯 JSON 조각을 만드는 도우미들. ID를 직접 쓰지 않는 장식 위젯은 1000번부터 자동으로 붙인다.
local NextAutoID = 1000
local function AutoID()
    NextAutoID = NextAutoID + 1
    return NextAutoID
end

local function L(anchorMin, anchorMax, pivot, pos, size)
    return string.format('{ "AnchorMin": [%g, %g], "AnchorMax": [%g, %g], "Pivot": [%g, %g], "AnchoredPosition": [%g, %g], "SizeDelta": [%g, %g] }',
        anchorMin[1], anchorMin[2], anchorMax[1], anchorMax[2], pivot[1], pivot[2], pos[1], pos[2], size[1], size[2])
end

local function Stretch(pos, size)
    return L({ 0, 0 }, { 1, 1 }, { 0.5, 0.5 }, pos or { 0, 0 }, size or { 0, 0 })
end

-- 부모의 왼쪽 위 기준으로 놓는 고정 크기 사각형
local function TopLeft(x, y, w, h)
    return L({ 0, 0 }, { 0, 0 }, { 0, 0 }, { x, y }, { w, h })
end

local function ColorJson(c)
    return string.format("[%g, %g, %g, %g]", c[1], c[2], c[3], c[4])
end

local function Img(id, layout, color)
    return string.format('{ "Type": "Image", "ID": %d, "Layout": %s, "Color": %s }', id or AutoID(), layout, ColorJson(color))
end

local function Txt(id, layout, text, color)
    return string.format('{ "Type": "Text", "ID": %d, "Layout": %s, "Text": "%s", "Color": %s }', id or AutoID(), layout, text, ColorJson(color))
end

local function Canvas(id, layout, children)
    return string.format('{ "Type": "Canvas", "ID": %d, "Layout": %s, "Children": [\n%s\n] }', id or AutoID(), layout, table.concat(children, ",\n"))
end

-- 부모 크기에 맞춰 늘어나는 네모 블록 패널 레이어를 children 뒤에 붙인다
local function AddBlockLayers(children, face, light, dark)
    table.insert(children, Img(nil, Stretch({ 6, 6 }), Pal.Shadow))
    table.insert(children, Img(nil, Stretch(), Pal.Outline))
    table.insert(children, Img(nil, Stretch({ 0, 0 }, { -8, -8 }), face or Pal.Face))
    table.insert(children, Img(nil, L({ 0, 0 }, { 1, 0 }, { 0.5, 0 }, { 0, 4 }, { -8, 6 }), light or Pal.FaceLight))
    table.insert(children, Img(nil, L({ 0, 1 }, { 1, 1 }, { 0.5, 1 }, { 0, -4 }, { -8, 8 }), dark or Pal.FaceDark))
end

-- 칸으로 나뉜 블록 게이지. fillCanvasID의 너비를 줄여 비율을 표현한다.
local function Bar(id, layout, fillCanvasID, fillID, fillLightID, fill, fillLight, segments)
    local fillChildren = {
        Img(fillID, Stretch(), fill),
        Img(fillLightID, L({ 0, 0 }, { 1, 0 }, { 0.5, 0 }, { 0, 0 }, { 0, 4 }), fillLight),
    }
    local children = {
        Img(nil, Stretch({ 0, 0 }, { 6, 6 }), Pal.Outline),
        Img(nil, Stretch(), Pal.Track),
        Canvas(fillCanvasID, L({ 0, 0 }, { 1, 1 }, { 0, 0.5 }, { 0, 0 }, { 0, 0 }), fillChildren),
    }
    -- 칸 사이 홈: 게이지가 네모 블록을 이어 붙인 것처럼 보이게 한다
    for i = 1, segments - 1 do
        local x = i / segments
        table.insert(children, Img(nil, L({ x, 0 }, { x, 1 }, { 0.5, 0.5 }, { 0, 0 }, { 3, 0 }), Pal.Outline))
    end
    return Canvas(id, layout, children)
end

-- 네모 코인 아이콘 (왼쪽 위 x, y 기준, 크기 size)
local function AddCoinIcon(children, x, y, size)
    local inner = size - 8
    table.insert(children, Img(nil, TopLeft(x, y, size, size), Pal.GoldEdge))
    table.insert(children, Img(nil, TopLeft(x + 4, y + 4, inner, inner), Pal.Gold))
    table.insert(children, Img(nil, TopLeft(x + 4, y + 4 + inner * 0.78, inner, inner * 0.22), Pal.GoldDk))
    table.insert(children, Img(nil, TopLeft(x + 8, y + 8, inner * 0.25, inner * 0.25), Pal.GoldLt))
    table.insert(children, Img(nil, TopLeft(x + size * 0.5 - 4, y + size * 0.28, 8, size * 0.44), Pal.GoldDk))
end

-- 네모 하트 아이콘 (왼쪽 위 x, y 기준)
local function AddHeartIcon(children, x, y)
    table.insert(children, Img(nil, TopLeft(x, y, 12, 10), Pal.Red))
    table.insert(children, Img(nil, TopLeft(x + 18, y, 12, 10), Pal.Red))
    table.insert(children, Img(nil, TopLeft(x, y + 10, 30, 12), Pal.Red))
    table.insert(children, Img(nil, TopLeft(x + 4, y + 22, 22, 8), Pal.RedDk))
    table.insert(children, Img(nil, TopLeft(x + 9, y + 30, 12, 6), Pal.RedDk))
    table.insert(children, Img(nil, TopLeft(x + 3, y + 3, 5, 5), Pal.RedLt))
end

local function BuildHUDJson()
    -- 제한 시간: 화면 위 가운데, 잔디색 머리띠가 달린 블록
    local timer = {}
    AddBlockLayers(timer)
    table.insert(timer, Img(3, L({ 0, 0 }, { 1, 0 }, { 0.5, 0 }, { 0, 4 }, { -8, 30 }), Pal.Grass))
    table.insert(timer, Img(7, L({ 0, 0 }, { 1, 0 }, { 0.5, 0 }, { 0, 4 }, { -8, 5 }), Pal.GrassLt))
    table.insert(timer, Img(nil, L({ 0, 0 }, { 1, 0 }, { 0.5, 0 }, { 0, 34 }, { -8, 4 }), Pal.GrassDk))
    table.insert(timer, Txt(4, L({ 0.5, 0 }, { 0.5, 0 }, { 0.5, 0 }, { 0, 10 }, { 100, 18 }), "남은 시간", Pal.White))
    table.insert(timer, Txt(5, L({ 0.5, 0 }, { 0.5, 0 }, { 0.5, 0 }, { 0, 42 }, { 140, 42 }), "01:30", Pal.Ink))
    table.insert(timer, Bar(6, L({ 0, 1 }, { 1, 1 }, { 0.5, 1 }, { 0, -18 }, { -36, 12 }), 8, 9, nil, Pal.Gold, Pal.GoldLt, 10))

    -- 코인: 화면 왼쪽 위
    local coin = {}
    AddBlockLayers(coin)
    AddCoinIcon(coin, 18, 16, 52)
    table.insert(coin, Txt(16, TopLeft(86, 14, 40, 18), "코인", Pal.InkSoft))
    table.insert(coin, Txt(17, TopLeft(86, 32, 60, 38), "0", Pal.Ink))

    -- 체력: 화면 왼쪽 아래
    local hp = {}
    AddBlockLayers(hp)
    AddHeartIcon(hp, 18, 14)
    table.insert(hp, Txt(27, TopLeft(60, 14, 40, 20), "체력", Pal.InkSoft))
    table.insert(hp, Txt(28, L({ 1, 0 }, { 1, 0 }, { 1, 0 }, { -18, 14 }, { 90, 20 }), "100 / 100", Pal.Ink))
    table.insert(hp, Bar(29, L({ 0, 1 }, { 1, 1 }, { 0.5, 1 }, { 0, -18 }, { -36, 16 }), 31, 32, 33, Pal.Leaf, Pal.LeafLt, 10))

    local root = {
        Canvas(1, L({ 0.5, 0 }, { 0.5, 0 }, { 0.5, 0 }, { 0, 20 }, { 300, 120 }), timer),
        Canvas(10, TopLeft(24, 20, 220, 86), coin),
        Canvas(20, L({ 0, 1 }, { 0, 1 }, { 0, 1 }, { 24, -24 }, { 400, 86 }), hp),
        -- 안내 문구: 어두운 글자를 살짝 비껴 깔아 그림자처럼 보이게 한다
        Txt(41, L({ 1, 0 }, { 1, 0 }, { 1, 0 }, { -25, 35 }, { 400, 24 }), "슬라임을 맞혀서 코인을 모으세요!", { 0.25, 0.16, 0.09, 0.7 }),
        Txt(40, L({ 1, 0 }, { 1, 0 }, { 1, 0 }, { -28, 32 }, { 400, 24 }), "슬라임을 맞혀서 코인을 모으세요!", Pal.White),
    }
    return Canvas(100, Stretch(), root)
end

-- 결과 화면. 게임이 끝날 때 만든다.
local function BuildResultJson()
    local panel = {}
    AddBlockLayers(panel)
    table.insert(panel, Img(3, L({ 0, 0 }, { 1, 0 }, { 0.5, 0 }, { 0, 4 }, { -8, 72 }), Pal.Grass))
    table.insert(panel, Img(4, L({ 0, 0 }, { 1, 0 }, { 0.5, 0 }, { 0, 4 }, { -8, 6 }), Pal.GrassLt))
    table.insert(panel, Img(5, L({ 0, 0 }, { 1, 0 }, { 0.5, 0 }, { 0, 76 }, { -8, 5 }), Pal.GrassDk))
    AddCoinIcon(panel, 26, 22, 34)
    table.insert(panel, Txt(14, L({ 0.5, 0 }, { 0.5, 0 }, { 0.5, 0 }, { 3, 21 }, { 300, 44 }), "시간 종료!", { 0.12, 0.3, 0.08, 0.8 }))
    table.insert(panel, Txt(10, L({ 0.5, 0 }, { 0.5, 0 }, { 0.5, 0 }, { 0, 18 }, { 300, 44 }), "시간 종료!", Pal.White))
    table.insert(panel, Txt(11, L({ 0.5, 0 }, { 0.5, 0 }, { 0.5, 0 }, { 0, 112 }, { 300, 44 }), "획득한 코인 0개", Pal.Ink))
    table.insert(panel, Txt(12, L({ 0.5, 0 }, { 0.5, 0 }, { 0.5, 0 }, { 0, 174 }, { 300, 28 }), "최고 기록 0개", Pal.InkSoft))
    table.insert(panel, Img(nil, L({ 0.5, 0 }, { 0.5, 0 }, { 0.5, 0 }, { 0, 228 }, { 420, 4 }), Pal.FaceDark))
    table.insert(panel, Txt(13, L({ 0.5, 1 }, { 0.5, 1 }, { 0.5, 1 }, { 0, -32 }, { 300, 22 }), "잠시 후 타이틀로 돌아갑니다", Pal.InkSoft))

    local root = {
        Img(1, Stretch(), { 0.05, 0.08, 0.03, 0.5 }),
        Canvas(2, L({ 0.5, 0.5 }, { 0.5, 0.5 }, { 0.5, 0.5 }, { 0, 0 }, { 560, 330 }), panel),
    }
    return Canvas(200, Stretch(), root)
end

local HUDTree
local ResultTree

local TimeText
local TimerTab
local TimerTabLight
local TimerBarFill
local TimerBarWidth
local CoinText
local HpText
local HpBarFill
local HpBarFillImage
local HpBarFillLight
local HpBarWidth
local bTimerWarned = false
local bHpWarned = false

local function LinearColorOf(c)
    return LinearColor.new(c[1], c[2], c[3], c[4])
end

local PlayerTable

-- 텍스트 위젯은 사각형 높이에 맞춰 글자 크기가 정해지므로, 글자 수에 맞게 너비를 잡아 준다.
-- Pretendard 기준 대략적인 글자 폭(em): 한글 0.86, 숫자 0.6, 영문 대문자 0.68, 소문자 0.55, 공백 0.25
local function MeasureTextEm(text)
    local width = 0.0
    local hasWide = false
    local i = 1
    local n = #text
    while i <= n do
        local b = string.byte(text, i)
        if b >= 0xF0 then
            width = width + 1.0; hasWide = true; i = i + 4
        elseif b >= 0xE0 then
            width = width + 0.864; hasWide = true; i = i + 3
        elseif b >= 0xC0 then
            width = width + 0.6; i = i + 2
        else
            local c = string.char(b)
            if c == " " then width = width + 0.25
            elseif c:match("%d") then width = width + 0.6
            elseif c:match("%u") then width = width + 0.68
            elseif c:match("%l") then width = width + 0.55
            else width = width + 0.3 end
            i = i + 1
        end
    end
    return width, hasWide
end

local function FitText(widget, text, height)
    if widget == nil then
        return
    end
    widget:SetText(text)
    local em, hasWide = MeasureTextEm(text)
    -- 글자 묶음의 실제 높이(em). 한글은 아래로 조금 더 내려간다
    local boxHeight = hasWide and 1.03 or 0.875
    local layout = widget:GetLayout()
    layout.SizeDelta.X = height * em / boxHeight
    layout.SizeDelta.Y = height
    widget:SetLayout(layout)
end

local function SetBarRatio(bar, maxWidth, ratio)
    if bar == nil or maxWidth == nil then
        return
    end
    ratio = math.max(0.0, math.min(1.0, ratio))
    local layout = bar:GetLayout()
    layout.SizeDelta.X = -maxWidth * (1.0 - ratio)
    bar:SetLayout(layout)
end

local function FormatTime(seconds)
    local total = math.max(0, math.ceil(seconds))
    return string.format("%02d:%02d", math.floor(total / 60), total % 60)
end

local function GetActorTable(actorClass)
    local actor = GamePlayStatics.GetActorOfClass(owner, actorClass)
    if actor == nil then
        return nil
    end

    local script = actor:FindComponentByClass(ScriptComponent.StaticClass())
    if script == nil then
        return nil
    end

    return script:GetLuaTable()
end

local function CollectedCoins()
    if PlayerTable == nil then
        return 0
    end
    return PlayerTable.collectedCoinCount or 0
end

local function ShowResult(reason)
    ResultTree = UI.CreateWidgetTree(BuildResultJson())
    if ResultTree == nil then
        return
    end
    UI.AddToViewport(ResultTree)

    local score = CollectedCoins()
    local title = (reason == "Dead") and "쓰러졌다!" or "시간 종료!"
    -- 쓰러졌을 때는 머리띠를 빨간 블록으로
    if reason == "Dead" then
        ResultTree:FindWidget(3):SetColor(LinearColorOf(Pal.Red))
        ResultTree:FindWidget(4):SetColor(LinearColorOf(Pal.RedLt))
        ResultTree:FindWidget(5):SetColor(LinearColorOf(Pal.RedDk))
    end
    FitText(ResultTree:FindWidget(14), title, 44)
    FitText(ResultTree:FindWidget(10), title, 44)
    FitText(ResultTree:FindWidget(11), "획득한 코인 " .. score .. "개", 44)
    FitText(ResultTree:FindWidget(12), "최고 기록 " .. (_G.BestScore or score) .. "개", 28)
    FitText(ResultTree:FindWidget(13), "잠시 후 타이틀로 돌아갑니다", 22)
end

local ValidPhases = { Cinematic = true, Playing = true, Finished = true }
local OnEnterPhase = {}

local function SetPhase(newPhase, ...)
    if not ValidPhases[newPhase] or _G.GamePhase == newPhase then
        return
    end
    _G.GamePhase = newPhase

    if OnEnterPhase[newPhase] then
        OnEnterPhase[newPhase](...)
    end
end

local function FinishDelay()
    coroutine.yield(WaitForSeconds.new(ResultShowTime))
    GamePlayStatics.OpenLevel(owner, "Assets/Scenes/Title.scene")
end

OnEnterPhase.Playing = function()
    -- 인트로 시퀀스의 마지막 컷 카메라는 "슬라임을 보는 플레이어 카메라" 자세로 끝난다.
    -- 같은 방향으로 ControlRotation을 맞춘 뒤 넘겨야 화면이 튀지 않는다.
    local player = GamePlayStatics.GetActorOfClass(owner, Player.StaticClass())
    local enemy = GamePlayStatics.GetActorOfClass(owner, Enemy.StaticClass())
    if player ~= nil and enemy ~= nil then
        local toEnemy = enemy:GetActorLocation() - player:GetActorLocation()
        GamePlayStatics.SetControlRotation(owner, Rotator.new(0.0, math.deg(math.atan(toEnemy.y, toEnemy.x)), 0.0))
    end
    GamePlayStatics.SetViewTarget(owner, player)

    GamePlayStatics.PlayMusic("Assets/Sounds/Battle.wav", 0.4)

    HUDTree = UI.CreateWidgetTree(BuildHUDJson())
    if HUDTree == nil then return end

    UI.AddToViewport(HUDTree)

    -- 정적 텍스트 크기 맞추기
    FitText(HUDTree:FindWidget(4), "남은 시간", 18)
    FitText(HUDTree:FindWidget(16), "코인", 18)
    FitText(HUDTree:FindWidget(27), "체력", 20)
    FitText(HUDTree:FindWidget(41), "슬라임을 맞혀서 코인을 모으세요!", 24)
    FitText(HUDTree:FindWidget(40), "슬라임을 맞혀서 코인을 모으세요!", 24)

    TimeText = HUDTree:FindWidget(5)
    TimerTab = HUDTree:FindWidget(3)
    TimerTabLight = HUDTree:FindWidget(7)
    TimerBarFill = HUDTree:FindWidget(8)
    TimerBarWidth = HUDTree:FindWidget(1):GetLayout().SizeDelta.X - 36

    CoinText = HUDTree:FindWidget(17)

    HpText = HUDTree:FindWidget(28)
    HpBarFill = HUDTree:FindWidget(31)
    HpBarFillImage = HUDTree:FindWidget(32)
    HpBarFillLight = HUDTree:FindWidget(33)
    HpBarWidth = HUDTree:FindWidget(20):GetLayout().SizeDelta.X - 36
end

OnEnterPhase.Finished = function(reason)
    -- 타이틀에서 보여 줄 기록
    local score = CollectedCoins()
    _G.LastScore = score
    _G.BestScore = math.max(_G.BestScore or 0, score)

    ShowResult(reason)
    state.FinishDelayCoroutine = StartCoroutine(FinishDelay)
end

function Init()
    _G.FinishGame = function(reason)
        SetPhase("Finished", reason)
    end
    state.remainingTime = GameTime
    SetPhase("Cinematic")
    bTimerWarned = false
    bHpWarned = false
end

local IntroDuration = 10.2 -- IntroCinematic 306프레임(30fps). 마지막 0.4초는 플레이어 카메라 자세로 멈춰 있다

local CinematicAspect = 21.0 / 9.0
local LetterboxBlendTime = 1.0

-- 현재 게임 뷰포트 비율. 아직 한 번도 그리지 않았으면 16:9로 가정한다
local function GetScreenAspect()
    local size = GamePlayStatics.GetViewportSize()
    if size.X <= 0 or size.Y <= 0 then
        return 16.0 / 9.0
    end
    return size.X / size.Y
end

local PlayerCamera
local LetterboxBlendElapsed -- nil이면 보간 중이 아님

local function FindPlayerCamera()
    local player = GamePlayStatics.GetActorOfClass(owner, Player.StaticClass())
    if player == nil then
        return nil
    end
    return player:FindComponentByClass(CameraComponent.StaticClass())
end

local function UpdateLetterboxBlend(deltaTime)
    if LetterboxBlendElapsed == nil or PlayerCamera == nil then
        return
    end

    LetterboxBlendElapsed = LetterboxBlendElapsed + deltaTime
    local t = math.min(LetterboxBlendElapsed / LetterboxBlendTime, 1.0)
    local eased = t * t * (3.0 - 2.0 * t) -- smoothstep

    -- 블렌드 중 창 크기가 바뀌어도 따라가도록 매 프레임 다시 읽는다
    PlayerCamera:SetAspectRatio(CinematicAspect + (GetScreenAspect() - CinematicAspect) * eased)

    if t >= 1.0 then
        PlayerCamera:SetConstrainAspectRatio(false)
        LetterboxBlendElapsed = nil
    end
end

local function PlayIntroCinematic()
    GamePlayStatics.PlaySound("Assets/Sounds/Nemo.wav", 0.7)
    coroutine.yield(WaitForSeconds.new(IntroDuration))
    LetterboxBlendElapsed = 0.0
    SetPhase("Playing")
end

function BeginPlay()
    PlayerCamera = FindPlayerCamera()
    if PlayerCamera ~= nil then
        PlayerCamera:SetConstrainAspectRatio(true)
        PlayerCamera:SetAspectRatio(CinematicAspect)
    end

    state.IntroCinematicCoroutine = StartCoroutine(PlayIntroCinematic)
end

function Tick(deltaTime)
    if PlayerTable == nil then
        PlayerTable = GetActorTable(Player.StaticClass())
    end

    UpdateLetterboxBlend(deltaTime)

    -- 제한 시간
    if _G.GamePhase == "Playing" then
        state.remainingTime = math.max(0.0, state.remainingTime - deltaTime)
        if state.remainingTime <= 0.0 then
            SetPhase("Finished", "TimeUp")
        end
    end

    if HUDTree == nil then
        return
    end

    FitText(TimeText, FormatTime(state.remainingTime), 42)
    -- 10초 이하면 머리띠와 시간 글자를 빨갛게
    if state.remainingTime <= 10.0 and not bTimerWarned then
        bTimerWarned = true
        TimeText:SetColor(LinearColorOf(Pal.Red))
        TimerTab:SetColor(LinearColorOf(Pal.Red))
        TimerTabLight:SetColor(LinearColorOf(Pal.RedLt))
    end
    SetBarRatio(TimerBarFill, TimerBarWidth, state.remainingTime / GameTime)

    FitText(CoinText, tostring(CollectedCoins()), 38)

    if PlayerTable ~= nil and PlayerTable.CurrentHp ~= nil and PlayerTable.MaxHp ~= nil and PlayerTable.MaxHp > 0 then
        local hp = math.max(0, PlayerTable.CurrentHp)
        FitText(HpText, hp .. " / " .. PlayerTable.MaxHp, 20)
        SetBarRatio(HpBarFill, HpBarWidth, hp / PlayerTable.MaxHp)

        -- 체력이 30% 이하면 게이지를 빨간 블록으로
        local bLow = hp <= PlayerTable.MaxHp * 0.3
        if bLow ~= bHpWarned then
            bHpWarned = bLow
            HpBarFillImage:SetColor(LinearColorOf(bLow and Pal.Red or Pal.Leaf))
            HpBarFillLight:SetColor(LinearColorOf(bLow and Pal.RedLt or Pal.LeafLt))
        end
    end
end

function EndPlay()
    if state.IntroCinematicCoroutine ~= nil then
        StopCoroutine(state.IntroCinematicCoroutine)
        state.IntroCinematicCoroutine = nil
    end
    if state.FinishDelayCoroutine ~= nil then
        StopCoroutine(state.FinishDelayCoroutine)
        state.FinishDelayCoroutine = nil
    end

    GamePlayStatics.StopMusic()

    if HUDTree then
        UI.RemoveFromViewport(HUDTree)
        UI.DestroyWidgetTree(HUDTree)
        HUDTree = nil
    end

    if ResultTree then
        UI.RemoveFromViewport(ResultTree)
        UI.DestroyWidgetTree(ResultTree)
        ResultTree = nil
    end
end
