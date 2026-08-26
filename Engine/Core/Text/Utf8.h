#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace Gilgamesh::Text
{
    // Gilgamesh uses UTF-8 for narrow engine text. These functions convert at
    // Windows UTF-16 API boundaries and throw std::system_error when input is
    // invalid or the platform conversion fails.
    [[nodiscard]]
    std::string WideToUtf8(std::wstring_view input);

    [[nodiscard]]
    std::wstring Utf8ToWide(std::string_view input);

    // Returns a normalized, forward-slash UTF-8 representation for display,
    // serialization, logging, and logical asset identifiers. Continue using
    // std::filesystem::path itself for physical filesystem access.
    [[nodiscard]]
    std::string PathToUtf8(const std::filesystem::path& path);
}
