#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "Editor/HitoriEd/EditorSelection.h"

#include <functional>

class UWorld;
class ULevel;
class AActor;
class UActorComponent;
class UPrimitiveComponent;
class USceneComponent;

using SelectionCallback = std::function<void(USceneComponent*)>;
using DeleteActorCallback = std::function<void(AActor*)>;

class FOutlinerPanel : public IEditorPanel
{
public:
    FOutlinerPanel() = default;
    ~FOutlinerPanel() = default;

    // 아웃라이너에서 선택을 바꿀 때마다 Primary 액터의 루트를 넘긴다. 공유 선택을 쓰면 필요 없다.
    void SetSelectionCallback(SelectionCallback InCallback) { Callback = InCallback; }
    void SetDeleteActorCallback(DeleteActorCallback InCallback) { DeleteCallback = InCallback;}
    // 에디터 전체가 공유하는 선택 목록을 연결한다. 연결하지 않으면 패널 자체 목록을 쓴다.
    void SetSelection(FEditorSelection* InSelection) { Selection = InSelection ? InSelection : &OwnedSelection; }

public:
    bool Init() override;
    void Tick(float DeltaTime) override;
    void OnRender() override;
    const char* GetPanelName() const override { return "Outliner"; }

    void SetWorld(UWorld* InWorld) { World = InWorld; };

    void DrawActors(ULevel* Level);
    void DrawActorNode(AActor* Actor);
    // 기존 선택을 지우고 Actor만 선택한다. nullptr이면 선택을 비운다.
    void SelectActor(AActor* Actor);
    // Actor를 선택 목록에 넣거나 뺀다 (Shift·Ctrl 클릭).
    void ToggleActorSelection(AActor* Actor);

    // 파괴 예정 액터의 패널 내부 참조 정리
    void NotifyActorDestroyed(AActor* Actor);

    // Primary 액터 (마지막으로 선택한 액터)
    AActor* GetSelectedActor() const;


    // 액터 이름 바꾸기를 시작한다 (더블 클릭, F2, 우클릭 Rename).
    void BeginRename(AActor* Actor);

private:
    void NotifySelectionCallback() const;
    // 이름을 바꾸는 줄의 입력칸. 확정·취소되면 바꾸기 상태를 끝낸다.
    void DrawRenameInput(AActor* Actor);

    AActor* RenamingActor = nullptr;
    char RenameBuffer[256] = {};
    bool bFocusRenameInput = false;

    UWorld* World = nullptr;
    FEditorSelection OwnedSelection;
    FEditorSelection* Selection = &OwnedSelection;

    SelectionCallback Callback;
    DeleteActorCallback DeleteCallback;
    AActor* PendingDeleteActor = nullptr;
};
