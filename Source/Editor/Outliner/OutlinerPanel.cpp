#include "EnginePCH.h"
#include "Editor/Outliner/OutlinerPanel.h"
#include "Editor/HitoriEd/EditorDragDrop.h"

#include "Engine/Level.h"
#include "Engine/World.h"

#include "Component/ActorComponent.h"
#include "Component/PrimitiveComponent.h"

bool FOutlinerPanel::Init()
{
    return true;
}

void FOutlinerPanel::Tick(float DeltaTime)
{
}

void FOutlinerPanel::OnRender()
{
    // World 참조 확인
    if (!World)
        return;

    // 현재 Level 가져오기
    ULevel* Level = World->GetCurrentLevel();
    if (!Level)
        return;

    // Level의 Actor 목록 Tree 출력
    ImGui::Begin("Outliner");

    // F2: 아웃라이너에 포커스가 있을 때 선택된(Primary) 액터 이름 바꾸기
    if (!RenamingActor && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
        && ImGui::IsKeyPressed(ImGuiKey_F2, false))
    {
        BeginRename(Selection->GetPrimary());
    }

    DrawActors(Level);

    ImGui::End();

    // Actors 순회가 완전히 끝난 뒤 삭제
    if (PendingDeleteActor)
    {
        if (DeleteCallback)
        {
            DeleteCallback(PendingDeleteActor);
        }

        PendingDeleteActor = nullptr;
    }
}

// Level이 가지고 있는 Actor 목록 순회하며 Draw TreeNode
void FOutlinerPanel::DrawActors(ULevel* Level)
{
    if (!Level)
        return;

    const TArray<AActor*>& Actors = Level->GetActors();

    struct FActorGroup
    {
        EPrimitiveType Type;
        const char* Name;
    };

    const FActorGroup Groups[] =
    {
        { EPrimitiveType::Sphere, "Sphere" },
        { EPrimitiveType::Cube,   "Cube" },
        { EPrimitiveType::Cone,   "Cone" },
        { EPrimitiveType::Plane,  "Plane" }
    };

    ImGuiListClipper Clipper;
	Clipper.Begin(Actors.Num(), ImGui::GetTextLineHeightWithSpacing());

    while (Clipper.Step())
    {
		for (int32 Index = Clipper.DisplayStart; Index < Clipper.DisplayEnd; ++Index)
		{
			AActor* Actor = Actors[Index];
			if (Actor)
				DrawActorNode(Actor);
		}
    }
}

// Actor를 ImGui TreeNode로 출력
void FOutlinerPanel::DrawActorNode(AActor* Actor)
{
    if (!Actor)
        return;

    const FString& ActorName = Actor->GetName();

    //ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_None;
    ImGuiTreeNodeFlags Flags = 
        ImGuiTreeNodeFlags_Leaf |
        ImGuiTreeNodeFlags_NoTreePushOnOpen |
        ImGuiTreeNodeFlags_SpanAvailWidth;

    // Actor 선택 시 UI 선택 상태
    if (Selection->IsSelected(Actor))
        Flags |= ImGuiTreeNodeFlags_Selected;

    // 이름을 바꾸는 중이면 같은 줄에 입력칸을 띄운다
    if (RenamingActor == Actor)
    {
        ImGui::TreeNodeEx(Actor, Flags, "%s", "");
        ImGui::SameLine();
        DrawRenameInput(Actor);
        return;
    }

    ImGui::TreeNodeEx(Actor, Flags, "%s", ActorName.c_str());

    // UI에서 Actor 클릭. Shift·Ctrl을 누르고 있으면 기존 선택에 더하거나 뺀다.
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        const ImGuiIO& IO = ImGui::GetIO();
        if (IO.KeyShift || IO.KeyCtrl)
            ToggleActorSelection(Actor);
        else
            SelectActor(Actor);
    }

    // 더블 클릭으로 이름 바꾸기
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        BeginRename(Actor);
    }

    if (ImGui::BeginDragDropSource())
    {
        const uint32 ActorUUID = Actor->GetUUID();
        ImGui::SetDragDropPayload(EditorDragDrop::Actor, &ActorUUID, sizeof(ActorUUID));
        ImGui::TextUnformatted(ActorName.c_str());
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Rename", "F2"))
        {
            BeginRename(Actor);
        }

        // 선택된 액터에서 열면 선택 전체를, 아니면 그 액터만 지운다 (삭제 콜백이 판단한다).
        if (ImGui::MenuItem("Delete"))
        {
            PendingDeleteActor = Actor; // 여기서는 바로 삭제 X
        }

        ImGui::EndPopup();
    }
}

// 파괴 예정 액터의 이름 변경 및 삭제 예약 참조 정리
void FOutlinerPanel::NotifyActorDestroyed(AActor* Actor)
{
    if (RenamingActor == Actor)
    {
        RenamingActor = nullptr;
        bFocusRenameInput = false;
        RenameBuffer[0] = '\0';
    }
    if (PendingDeleteActor == Actor)
    {
        PendingDeleteActor = nullptr;
    }
}

void FOutlinerPanel::BeginRename(AActor* Actor)
{
    if (!Actor)
        return;

    RenamingActor = Actor;
    strncpy_s(RenameBuffer, sizeof(RenameBuffer), Actor->GetName().c_str(), _TRUNCATE);
    bFocusRenameInput = true;
}

// Enter 또는 다른 곳 클릭으로 확정하고, Esc로 취소한다.
// 비어 있거나 같은 레벨에 이미 있는 이름이면 바꾸지 않는다.
void FOutlinerPanel::DrawRenameInput(AActor* Actor)
{
    if (bFocusRenameInput)
    {
        ImGui::SetKeyboardFocusHere();
        bFocusRenameInput = false;
    }

    ImGui::SetNextItemWidth(-1.0f);
    const bool bConfirmed = ImGui::InputText("##RenameActor", RenameBuffer, sizeof(RenameBuffer),
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
    const bool bCancelled = ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape);
    if (!bCancelled && !bConfirmed && !ImGui::IsItemDeactivated())
        return;

    const FString NewName = RenameBuffer;
    if (!bCancelled && !NewName.empty() && NewName != Actor->GetName())
    {
        bool bNameTaken = false;
        if (const ULevel* Level = Actor->GetLevel())
        {
            for (const AActor* Other : Level->GetActors())
            {
                if (Other && Other != Actor && Other->GetName() == NewName)
                {
                    bNameTaken = true;
                    break;
                }
            }
        }

        if (bNameTaken)
            HTR_LOG(Warning, "An actor named '{}' already exists in this level.", NewName);
        else
            Actor->SetName(FName(NewName));
    }
    RenamingActor = nullptr;
}

// 선택 Object 변경
void FOutlinerPanel::SelectActor(AActor* Actor)
{
    Selection->Select(Actor);

    if (Actor)
        HTR_LOG(Info, "{} UUID {} is selected", Actor->GetName(), Actor->GetUUID());

    NotifySelectionCallback();
}

void FOutlinerPanel::ToggleActorSelection(AActor* Actor)
{
    Selection->Toggle(Actor);
    NotifySelectionCallback();
}

void FOutlinerPanel::NotifySelectionCallback() const
{
    if (!Callback)
        return;

    AActor* Primary = Selection->GetPrimary();
    Callback(Primary ? Cast<USceneComponent>(Primary->GetRootComponent()) : nullptr);
}

AActor* FOutlinerPanel::GetSelectedActor() const
{
    return Selection->GetPrimary();
}
