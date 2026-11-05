#pragma once
#include <string_view>
#include <system_error>
#include <Windows.h>
#include "Core/EngineString.h"

namespace Encoding
{
    // UTF-8 문자열의 Windows UTF-16 변환
    inline std::wstring Utf8ToWide(std::string_view Text)
    {
        if (Text.empty()) return {};
        const int Length = static_cast<int>(Text.size());
        const int Count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Text.data(), Length, nullptr, 0);
        if (Count == 0) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Utf8ToWide");
        std::wstring Result(Count, L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Text.data(), Length, Result.data(), Count);
        return Result;
    }

    // Windows UTF-16 문자열의 UTF-8 변환
    inline FString WideToUtf8(std::wstring_view Text)
    {
        if (Text.empty()) return {};
        const int Length = static_cast<int>(Text.size());
        const int Count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, Text.data(), Length, nullptr, 0, nullptr, nullptr);
        if (Count == 0) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "WideToUtf8");
        FString Result(Count, '\0');
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, Text.data(), Length, Result.data(), Count, nullptr, nullptr);
        return Result;
    }
}

