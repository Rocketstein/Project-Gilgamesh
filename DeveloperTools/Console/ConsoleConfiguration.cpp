#include "ConsoleConfiguration.h"

namespace
{
    constexpr std::size_t ToIndex(LogLevel level)
    {
        return static_cast<std::size_t>(level);
    }
}

ConsoleConfiguration::ConsoleConfiguration()
{
    visibleLogLevels_.fill(true);
}

void ConsoleConfiguration::SetLogLevelVisible(
    LogLevel level,
    bool visible)
{
    const std::size_t index = ToIndex(level);

    if (index >= LevelCount
        || visibleLogLevels_[index] == visible)
    {
        return;
    }

    visibleLogLevels_[index] = visible;
    ++logLevelVisibilityRevision_;
}

bool ConsoleConfiguration::IsLogLevelVisible(
    LogLevel level) const
{
    const std::size_t index = ToIndex(level);

    return index < LevelCount
        && visibleLogLevels_[index];
}

std::uint64_t ConsoleConfiguration::LogLevelVisibilityRevision()
    const noexcept
{
    return logLevelVisibilityRevision_;
}
