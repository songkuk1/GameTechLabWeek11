#pragma once

#include "Core/Encoding.h"
#include <filesystem>

namespace PathUtils
{
    // 엔진 UTF-8 경로의 Windows native 경로 변환
    inline std::filesystem::path FromUtf8(std::string_view Path)
    {
        return std::filesystem::path(Encoding::Utf8ToWide(Path));
    }

    // native 경로의 UTF-8 및 '/' 구분자 변환
    inline FString ToUtf8(const std::filesystem::path& Path)
    {
        return Encoding::WideToUtf8(Path.generic_wstring());
    }
}   

