#include "Utf8.h"

#include <Windows.h>

#include <limits>
#include <stdexcept>
#include <system_error>

namespace
{
    int CheckedInputSize(std::size_t size)
    {
        if (size > static_cast<std::size_t>(
            std::numeric_limits<int>::max()))
        {
            throw std::length_error(
                "UTF conversion input exceeds the Win32 size limit");
        }

        return static_cast<int>(size);
    }

    [[noreturn]]
    void ThrowConversionError(const char* operation)
    {
        throw std::system_error(
            static_cast<int>(GetLastError()),
            std::system_category(),
            operation);
    }
}

namespace Gilgamesh::Text
{
    std::string WideToUtf8(std::wstring_view input)
    {
        if (input.empty())
            return {};

        const int inputSize = CheckedInputSize(input.size());

        const int requiredSize = WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            input.data(),
            inputSize,
            nullptr,
            0,
            nullptr,
            nullptr);

        if (requiredSize == 0)
            ThrowConversionError("WideCharToMultiByte");

        std::string output(
            static_cast<std::size_t>(requiredSize),
            '\0');

        const int written = WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            input.data(),
            inputSize,
            output.data(),
            requiredSize,
            nullptr,
            nullptr);

        if (written != requiredSize)
            ThrowConversionError("WideCharToMultiByte");

        return output;
    }

    std::wstring Utf8ToWide(std::string_view input)
    {
        if (input.empty())
            return {};

        const int inputSize = CheckedInputSize(input.size());

        const int requiredSize = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            input.data(),
            inputSize,
            nullptr,
            0);

        if (requiredSize == 0)
            ThrowConversionError("MultiByteToWideChar");

        std::wstring output(
            static_cast<std::size_t>(requiredSize),
            L'\0');

        const int written = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            input.data(),
            inputSize,
            output.data(),
            requiredSize);

        if (written != requiredSize)
            ThrowConversionError("MultiByteToWideChar");

        return output;
    }

    std::string PathToUtf8(const std::filesystem::path& path)
    {
        const std::u8string utf8 = path.generic_u8string();

        return {
            reinterpret_cast<const char*>(utf8.data()),
            utf8.size()
        };
    }
}
