#include "EnginePCH.h"
#include "LuaScriptEditor.h"

#include <shellapi.h>
#include <commdlg.h>

#include "Editor/HitoriEd/EditorFileUtils.h"

UScriptComponent* FLuaScriptEditor::CreateScript(AActor* Actor, FName Name)
{
	if (!Actor)
	{
		return nullptr;
	}

	if (FEditorFileUtils::GetCurrentScenePath().empty())
	{
		if (!FEditorFileUtils::SaveSceneAs(Actor->GetWorld()))
		{
			return nullptr;
		}
	}
	const fs::path ScenePath = FEditorFileUtils::GetCurrentScenePath();
	const FString SceneName = ScenePath.stem().string();

	const fs::path TemplatePath = "Assets/Scripts/Templates/Template.lua";
	const fs::path ScriptDirectory = "Assets/Scripts/Actors";
	const FString FileName = SceneName + "_" + Actor->GetName() + "_" + std::to_string(Actor->GetUUID()) + ".lua";
	const fs::path ScriptPath = ScriptDirectory / FileName;

	std::error_code Error;
	fs::create_directories(ScriptDirectory, Error);
	fs::copy_file(TemplatePath, ScriptPath, fs::copy_options::none, Error);
	if (Error)
	{
		return nullptr;
	}

	UScriptComponent* Script = Cast<UScriptComponent>(Actor->AddComponent(UScriptComponent::StaticClass(), Name));
	Script->SetScriptPath(ScriptPath.generic_string());
	return Script;
}

bool FLuaScriptEditor::CreateScriptFileAndOpen()
{
    const fs::path TemplatePath = fs::absolute("Assets/Scripts/Templates/Template.lua").lexically_normal();
    const fs::path ScriptDirectory = fs::absolute("Assets/Scripts").lexically_normal();

    std::error_code Error;
    fs::create_directories(ScriptDirectory, Error);

    if (Error)
    {
        MessageBoxA(
            GetActiveWindow(),
            "Failed to create the script directory.",
            "Add Script",
            MB_OK | MB_ICONERROR
        );
        return false;
    }

    if (!fs::exists(TemplatePath))
    {
        MessageBoxA(
            GetActiveWindow(),
            "Template.lua was not found.",
            "Add Script",
            MB_OK | MB_ICONERROR
        );
        return false;
    }

    char FilePath[MAX_PATH] = "NewScript";
    const std::string InitialDirectory = ScriptDirectory.string();

    OPENFILENAMEA Ofn = {};
    Ofn.lStructSize = sizeof(OPENFILENAMEA);
    Ofn.hwndOwner = GetActiveWindow();
    Ofn.lpstrFile = FilePath;
    Ofn.nMaxFile = MAX_PATH;

    Ofn.lpstrFilter = 
        "Lua Script (*.lua)\0*.lua\0" 
        "All Files (*.*)\0*.*\0";

    Ofn.lpstrDefExt = "lua";
    Ofn.lpstrInitialDir = InitialDirectory.c_str();

    Ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (!GetSaveFileNameA(&Ofn))
    {
        return false;
    }

    fs::path SelectedPath = fs::path(FilePath).lexically_normal();

    Error.clear();
    fs::copy_file(TemplatePath, SelectedPath, fs::copy_options::overwrite_existing, Error);

    if (Error)
    {
        MessageBoxA(
            GetActiveWindow(),
            Error.message().c_str(),
            "Failed to create script",
            MB_OK | MB_ICONERROR
        );
        return false;
    }

    EditScript(SelectedPath.generic_string());
    return true;
}

void FLuaScriptEditor::EditScript(const FString& ScriptPath)
{
	const fs::path Path = fs::absolute(ScriptPath).lexically_normal();

	HINSTANCE hInst = ShellExecute(
		NULL, // 부모 윈도우 핸들 (NULL 사용 가능)
		L"open", // 동작(Verb). "open"이면 등록된 기본 프로그램으로 열기
		Path.c_str(), // 열고자 하는 파일 경로
		NULL, // 명령줄 인자 (필요 없다면 NULL)
		NULL, // 작업 디렉터리 (필요 없다면 NULL)
		SW_SHOWNORMAL // 열리는 창의 상태
	);
	if ((INT_PTR)hInst <= 32)
	{
		MessageBox(NULL, L"Failed to open file", L"Error", MB_OK | MB_ICONERROR);
	}
}

void FLuaScriptEditor::EditScript(UScriptComponent* ScriptComponent)
{
	EditScript(ScriptComponent->GetScriptPath());
}