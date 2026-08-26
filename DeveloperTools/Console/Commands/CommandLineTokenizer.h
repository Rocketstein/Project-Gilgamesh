#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

enum class CommandLineTokenizeError
{
    None,
    UnterminatedQuote
};

struct CommandLineTokenizeResult
{
    std::vector<std::string> tokens;
    CommandLineTokenizeError error =
        CommandLineTokenizeError::None;

    // Character offset in the original line at which the error began.
    std::size_t errorOffset = std::string_view::npos;

    [[nodiscard]]
    bool Succeeded() const noexcept
    {
        return error == CommandLineTokenizeError::None;
    }

    explicit operator bool() const noexcept
    {
        return Succeeded();
    }
};

// Splits a command line into owned tokens. Whitespace separates tokens except
// inside double quotes. Backslash escapes a double quote or another backslash.
[[nodiscard]]
CommandLineTokenizeResult TokenizeCommandLine(
    std::string_view commandLine);

[[nodiscard]]
constexpr std::string_view ToString(
    CommandLineTokenizeError error) noexcept
{
    switch (error)
    {
    case CommandLineTokenizeError::None:
        return "No error";

    case CommandLineTokenizeError::UnterminatedQuote:
        return "Unterminated quoted argument";

    default:
        return "Unknown command-line parsing error";
    }
}
