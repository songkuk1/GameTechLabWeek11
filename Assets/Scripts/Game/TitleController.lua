-- 네모네모 월드 타이틀
-- 하늘, 네모 구름, 잔디와 흙, 네모 나무, 슬라임, 네모 코인을 사각형 이미지로만 그린다.
-- UI 좌표: Anchor/Pivot의 y는 0이 화면 위, 1이 화면 아래

local TitleUIJson = [[
{
    "Type": "Canvas",
    "ID": 1,
    "Layout": { "AnchorMin": [0, 0], "AnchorMax": [1, 1], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [0, 0] },
    "Children": [
        { "Type": "Image", "ID": 10, "Layout": { "AnchorMin": [0, 0], "AnchorMax": [1, 1], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [0, 0] }, "Color": [0.36, 0.66, 0.93, 1] },
        { "Type": "Image", "ID": 11, "Layout": { "AnchorMin": [0, 0.42], "AnchorMax": [1, 0.62], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [0, 0] }, "Color": [0.47, 0.75, 0.96, 1] },
        { "Type": "Image", "ID": 12, "Layout": { "AnchorMin": [0, 0.62], "AnchorMax": [1, 0.78], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [0, 0] }, "Color": [0.6, 0.83, 0.98, 1] },

        { "Type": "Image", "ID": 13, "Layout": { "AnchorMin": [0.12, 0.14], "AnchorMax": [0.12, 0.14], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [230, 54] }, "Color": [1, 1, 1, 0.95] },
        { "Type": "Image", "ID": 14, "Layout": { "AnchorMin": [0.12, 0.14], "AnchorMax": [0.12, 0.14], "Pivot": [0.5, 0.5], "AnchoredPosition": [-30, -40], "SizeDelta": [120, 40] }, "Color": [1, 1, 1, 0.95] },
        { "Type": "Image", "ID": 15, "Layout": { "AnchorMin": [0.86, 0.2], "AnchorMax": [0.86, 0.2], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [280, 60] }, "Color": [1, 1, 1, 0.95] },
        { "Type": "Image", "ID": 16, "Layout": { "AnchorMin": [0.86, 0.2], "AnchorMax": [0.86, 0.2], "Pivot": [0.5, 0.5], "AnchoredPosition": [40, -44], "SizeDelta": [150, 44] }, "Color": [1, 1, 1, 0.95] },
        { "Type": "Image", "ID": 17, "Layout": { "AnchorMin": [0.62, 0.07], "AnchorMax": [0.62, 0.07], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [150, 36] }, "Color": [1, 1, 1, 0.7] },

        { "Type": "Image", "ID": 20, "Layout": { "AnchorMin": [0, 0.78], "AnchorMax": [1, 0.85], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [0, 0] }, "Color": [0.33, 0.7, 0.29, 1] },
        { "Type": "Image", "ID": 21, "Layout": { "AnchorMin": [0, 0.84], "AnchorMax": [1, 0.855], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [0, 0] }, "Color": [0.24, 0.53, 0.21, 1] },
        { "Type": "Image", "ID": 22, "Layout": { "AnchorMin": [0, 0.855], "AnchorMax": [1, 1], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [0, 0] }, "Color": [0.5, 0.31, 0.16, 1] },
        { "Type": "Image", "ID": 23, "Layout": { "AnchorMin": [0.06, 0.91], "AnchorMax": [0.06, 0.91], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [40, 40] }, "Color": [0.41, 0.25, 0.13, 1] },
        { "Type": "Image", "ID": 24, "Layout": { "AnchorMin": [0.27, 0.95], "AnchorMax": [0.27, 0.95], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [32, 32] }, "Color": [0.41, 0.25, 0.13, 1] },
        { "Type": "Image", "ID": 25, "Layout": { "AnchorMin": [0.73, 0.9], "AnchorMax": [0.73, 0.9], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [44, 44] }, "Color": [0.41, 0.25, 0.13, 1] },
        { "Type": "Image", "ID": 26, "Layout": { "AnchorMin": [0.95, 0.96], "AnchorMax": [0.95, 0.96], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [30, 30] }, "Color": [0.41, 0.25, 0.13, 1] },
        { "Type": "Image", "ID": 27, "Layout": { "AnchorMin": [0.42, 0.93], "AnchorMax": [0.42, 0.93], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [26, 26] }, "Color": [0.62, 0.62, 0.6, 1] },

        { "Type": "Image", "ID": 30, "Layout": { "AnchorMin": [0.1, 0.78], "AnchorMax": [0.1, 0.78], "Pivot": [0.5, 1], "AnchoredPosition": [0, 0], "SizeDelta": [46, 150] }, "Color": [0.45, 0.25, 0.1, 1] },
        { "Type": "Image", "ID": 31, "Layout": { "AnchorMin": [0.1, 0.78], "AnchorMax": [0.1, 0.78], "Pivot": [0.5, 1], "AnchoredPosition": [0, -120], "SizeDelta": [200, 120] }, "Color": [0.15, 0.5, 0.2, 1] },
        { "Type": "Image", "ID": 32, "Layout": { "AnchorMin": [0.1, 0.78], "AnchorMax": [0.1, 0.78], "Pivot": [0.5, 1], "AnchoredPosition": [0, -230], "SizeDelta": [150, 90] }, "Color": [0.2, 0.58, 0.24, 1] },
        { "Type": "Image", "ID": 33, "Layout": { "AnchorMin": [0.1, 0.78], "AnchorMax": [0.1, 0.78], "Pivot": [0.5, 1], "AnchoredPosition": [0, -310], "SizeDelta": [90, 60] }, "Color": [0.26, 0.66, 0.29, 1] },
        { "Type": "Image", "ID": 34, "Layout": { "AnchorMin": [0.1, 0.78], "AnchorMax": [0.1, 0.78], "Pivot": [0.5, 0.5], "AnchoredPosition": [60, -175], "SizeDelta": [20, 20] }, "Color": [0.9, 0.2, 0.15, 1] },

        { "Type": "Image", "ID": 35, "Layout": { "AnchorMin": [0.9, 0.78], "AnchorMax": [0.9, 0.78], "Pivot": [0.5, 1], "AnchoredPosition": [0, 0], "SizeDelta": [40, 120] }, "Color": [0.45, 0.25, 0.1, 1] },
        { "Type": "Image", "ID": 36, "Layout": { "AnchorMin": [0.9, 0.78], "AnchorMax": [0.9, 0.78], "Pivot": [0.5, 1], "AnchoredPosition": [0, -95], "SizeDelta": [170, 100] }, "Color": [0.15, 0.5, 0.2, 1] },
        { "Type": "Image", "ID": 37, "Layout": { "AnchorMin": [0.9, 0.78], "AnchorMax": [0.9, 0.78], "Pivot": [0.5, 1], "AnchoredPosition": [0, -185], "SizeDelta": [120, 76] }, "Color": [0.2, 0.58, 0.24, 1] },
        { "Type": "Image", "ID": 38, "Layout": { "AnchorMin": [0.9, 0.78], "AnchorMax": [0.9, 0.78], "Pivot": [0.5, 0.5], "AnchoredPosition": [-50, -140], "SizeDelta": [18, 18] }, "Color": [0.9, 0.2, 0.15, 1] },

        { "Type": "Image", "ID": 40, "Layout": { "AnchorMin": [0.73, 0.78], "AnchorMax": [0.73, 0.78], "Pivot": [0.5, 1], "AnchoredPosition": [0, 0], "SizeDelta": [210, 120] }, "Color": [0.1, 0.25, 0.12, 1] },
        { "Type": "Image", "ID": 41, "Layout": { "AnchorMin": [0.73, 0.78], "AnchorMax": [0.73, 0.78], "Pivot": [0.5, 1], "AnchoredPosition": [0, -118], "SizeDelta": [150, 40] }, "Color": [0.1, 0.25, 0.12, 1] },
        { "Type": "Image", "ID": 42, "Layout": { "AnchorMin": [0.73, 0.78], "AnchorMax": [0.73, 0.78], "Pivot": [0.5, 1], "AnchoredPosition": [-46, -156], "SizeDelta": [20, 34] }, "Color": [0.62, 0.62, 0.7, 1] },
        { "Type": "Image", "ID": 43, "Layout": { "AnchorMin": [0.73, 0.78], "AnchorMax": [0.73, 0.78], "Pivot": [0.5, 1], "AnchoredPosition": [0, -158], "SizeDelta": [20, 44] }, "Color": [0.62, 0.62, 0.7, 1] },
        { "Type": "Image", "ID": 44, "Layout": { "AnchorMin": [0.73, 0.78], "AnchorMax": [0.73, 0.78], "Pivot": [0.5, 1], "AnchoredPosition": [46, -156], "SizeDelta": [20, 34] }, "Color": [0.62, 0.62, 0.7, 1] },
        { "Type": "Image", "ID": 45, "Layout": { "AnchorMin": [0.73, 0.78], "AnchorMax": [0.73, 0.78], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, -82], "SizeDelta": [66, 66] }, "Color": [0.88, 0.88, 0.92, 1] },
        { "Type": "Image", "ID": 46, "Layout": { "AnchorMin": [0.73, 0.78], "AnchorMax": [0.73, 0.78], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, -86], "SizeDelta": [28, 28] }, "Color": [0.05, 0.05, 0.08, 1] },
        { "Type": "Image", "ID": 47, "Layout": { "AnchorMin": [0.73, 0.78], "AnchorMax": [0.73, 0.78], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, -36], "SizeDelta": [44, 12] }, "Color": [0.05, 0.12, 0.06, 1] },

        { "Type": "Image", "ID": 50, "Layout": { "AnchorMin": [0.27, 0.66], "AnchorMax": [0.27, 0.66], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [46, 46] }, "Color": [0.85, 0.55, 0.05, 1] },
        { "Type": "Image", "ID": 51, "Layout": { "AnchorMin": [0.27, 0.66], "AnchorMax": [0.27, 0.66], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [32, 32] }, "Color": [1, 0.8, 0.15, 1] },
        { "Type": "Image", "ID": 52, "Layout": { "AnchorMin": [0.34, 0.72], "AnchorMax": [0.34, 0.72], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [34, 34] }, "Color": [0.85, 0.55, 0.05, 1] },
        { "Type": "Image", "ID": 53, "Layout": { "AnchorMin": [0.34, 0.72], "AnchorMax": [0.34, 0.72], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [22, 22] }, "Color": [1, 0.8, 0.15, 1] },
        { "Type": "Image", "ID": 54, "Layout": { "AnchorMin": [0.62, 0.62], "AnchorMax": [0.62, 0.62], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [40, 40] }, "Color": [0.85, 0.55, 0.05, 1] },
        { "Type": "Image", "ID": 55, "Layout": { "AnchorMin": [0.62, 0.62], "AnchorMax": [0.62, 0.62], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [26, 26] }, "Color": [1, 0.8, 0.15, 1] },

        { "Type": "Text", "ID": 60, "Layout": { "AnchorMin": [0.5, 0.25], "AnchorMax": [0.5, 0.25], "Pivot": [0.5, 0.5], "AnchoredPosition": [7, 7], "SizeDelta": [700, 130] }, "Text": "네모네모 월드", "Color": [0.07, 0.2, 0.4, 1] },
        { "Type": "Text", "ID": 61, "Layout": { "AnchorMin": [0.5, 0.25], "AnchorMax": [0.5, 0.25], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [700, 130] }, "Text": "네모네모 월드", "Color": [1, 1, 1, 1] },
        { "Type": "Text", "ID": 62, "Layout": { "AnchorMin": [0.5, 0.38], "AnchorMax": [0.5, 0.38], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [500, 36] }, "Text": "슬라임을 무찌르고 코인을 모아라!", "Color": [1, 0.93, 0.45, 1] },

        { "Type": "Image", "ID": 63, "Layout": { "AnchorMin": [0.5, 0.5], "AnchorMax": [0.5, 0.5], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [488, 88] }, "Color": [1, 1, 1, 0.95] },
        { "Type": "Image", "ID": 64, "Layout": { "AnchorMin": [0.5, 0.5], "AnchorMax": [0.5, 0.5], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [476, 76] }, "Color": [0.13, 0.27, 0.5, 1] },
        { "Type": "Text", "ID": 2, "Layout": { "AnchorMin": [0.5, 0.5], "AnchorMax": [0.5, 0.5], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [400, 34] }, "Text": "스페이스바를 눌러 시작", "Color": [1, 1, 1, 1] },

        { "Type": "Text", "ID": 65, "Layout": { "AnchorMin": [0.5, 0.585], "AnchorMax": [0.5, 0.585], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [400, 26] }, "Text": "", "Color": [1, 1, 1, 0.95] },
        { "Type": "Text", "ID": 66, "Layout": { "AnchorMin": [0.5, 0.925], "AnchorMax": [0.5, 0.925], "Pivot": [0.5, 0.5], "AnchoredPosition": [0, 0], "SizeDelta": [700, 24] }, "Text": "WASD 이동   SPACE 점프   좌클릭 발사   우클릭 시점", "Color": [1, 0.93, 0.82, 1] },
        { "Type": "Text", "ID": 67, "Layout": { "AnchorMin": [0, 0], "AnchorMax": [0, 0], "Pivot": [0, 0], "AnchoredPosition": [24, 20], "SizeDelta": [420, 30] }, "Text": "TEAM3 이재호 조혁 이동헌 권혜원", "Color": [1, 1, 1, 1] }
    ]
}
]]

local Tree
local StartText
local StartTextAnimationCoroutine

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
    local boxHeight = hasWide and 1.03 or 0.875
    local layout = widget:GetLayout()
    layout.SizeDelta.X = height * em / boxHeight
    layout.SizeDelta.Y = height
    widget:SetLayout(layout)
end

function Init()

end

function FadeInOut(T)
    return T * T * (3.0 - 2.0 * T)
end

function Lerp(Start, End, T)
    return Start * (1.0 - T) + End * T
end

function StartTextAnimation(TargetObject, StartColor, EndColor, CycleTime)
    local DeltaTime = 0.01
    local ElapsedTime = 0
    while (true) do
        local T = ElapsedTime / CycleTime
        local Ease = FadeInOut(T)

        TargetObject:SetColor(Lerp(StartColor, EndColor, Ease))

        ElapsedTime = ElapsedTime + DeltaTime
        if (ElapsedTime >= CycleTime) then
            local Temp = StartColor
            StartColor = EndColor
            EndColor = Temp
            ElapsedTime = 0.0
        end

        coroutine.yield(WaitForSeconds.new(DeltaTime))
    end
end

function BeginPlay()
    Tree = UI.CreateWidgetTree(TitleUIJson)
    if Tree then
        UI.AddToViewport(Tree)

        FitText(Tree:FindWidget(60), "네모네모 월드", 130)
        FitText(Tree:FindWidget(61), "네모네모 월드", 130)
        FitText(Tree:FindWidget(62), "슬라임을 무찌르고 코인을 모아라!", 36)
        FitText(Tree:FindWidget(66), "WASD 이동   SPACE 점프   좌클릭 발사   우클릭 시점", 24)
        FitText(Tree:FindWidget(67), "TEAM3 이재호 조혁 이동헌 권혜원", 40)

        -- 이전 판 기록 (메인 게임이 끝날 때 남긴다)
        if _G.LastScore ~= nil then
            FitText(Tree:FindWidget(65),
                "지난 기록 " .. _G.LastScore .. "개   최고 기록 " .. (_G.BestScore or _G.LastScore) .. "개", 26)
        else
            FitText(Tree:FindWidget(65), "제한 시간 안에 코인을 최대한 많이 모으세요", 26)
        end

        StartText = Tree:FindWidget(2)
        FitText(StartText, "스페이스바를 눌러 시작", 34)
        StartTextAnimationCoroutine = StartCoroutine(StartTextAnimation, StartText, LinearColor.new(1.0, 1.0, 1.0, 1.0),
            LinearColor.new(1.0, 1.0, 1.0, 0.3), 1.0)
    end

    GamePlayStatics.PlayMusic("Assets/Sounds/Title.wav", 0.7)
end

function Tick(deltaTime)
    if (Input.IsKeyPressed(KeyCode.Space)) then
        GamePlayStatics.StopMusic()
        GamePlayStatics.OpenLevel(owner, "Assets/Scenes/Main_.scene")
    end
end

function EndPlay()
    if Tree then
        StopCoroutine(StartTextAnimationCoroutine)
        UI.RemoveFromViewport(Tree)
        UI.DestroyWidgetTree(Tree)
    end
end
