#include "CommandLineTokenizer.h"

#include <utility>

namespace
{
    constexpr bool IsWhitespace(char character) noexcept
    {
        return character == ' '
            || character == '\t'
            || character == '\r'
            || character == '\n';
    }

    constexpr bool IsEscapable(char character) noexcept
    {
        return character == '"' || character == '\\';
    }
}

CommandLineTokenizeResult TokenizeCommandLine(
    std::string_view commandLine)
{
    CommandLineTokenizeResult result;
    std::size_t cursor = 0;

    while (cursor < commandLine.size())
    {
        while (cursor < commandLine.size()
            && IsWhitespace(commandLine[cursor]))
        {
            ++cursor;
        }

        if (cursor == commandLine.size())
            break;

        std::string token;
        bool inQuotes = false;
        std::size_t openingQuote = std::string_view::npos;

        while (cursor < commandLine.size())
        {
            const char character = commandLine[cursor];

            if (!inQuotes && IsWhitespace(character))
                break;

            if (character == '"')
            {
                if (inQuotes)
                {
                    inQuotes = false;
                    openingQuote = std::string_view::npos;
                }
                else
                {
                    inQuotes = true;
                    openingQuote = cursor;
                }

                ++cursor;
                continue;
            }

            if (character == '\\'
                && cursor + 1 < commandLine.size()
                && IsEscapable(commandLine[cursor + 1]))
            {
                token.push_back(commandLine[cursor + 1]);
                cursor += 2;
                continue;
            }

            token.push_back(character);
            ++cursor;
        }

        if (inQuotes)
        {
            result.error =
                CommandLineTokenizeError::UnterminatedQuote;
            result.errorOffset = openingQuote;
            return result;
        }

        // This also preserves an explicitly empty argument written as "".
        result.tokens.push_back(std::move(token));
    }

    return result;
}
