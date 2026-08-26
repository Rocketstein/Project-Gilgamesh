#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "Engine/Core/Logging/LogTypes.h"

// UI-independent settings shared by the developer console and its commands.
class ConsoleConfiguration
{
public:
    ConsoleConfiguration();

    void SetLogLevelVisible(LogLevel level, bool visible);

    [[nodiscard]]
    bool IsLogLevelVisible(LogLevel level) const;

    // Consumers can cache this value and refresh derived state when it changes.
    [[nodiscard]]
    std::uint64_t LogLevelVisibilityRevision() const noexcept;

private:
    static constexpr std::size_t LevelCount =
        static_cast<std::size_t>(LogLevel::Count);

    std::array<bool, LevelCount> visibleLogLevels_{};
    std::uint64_t logLevelVisibilityRevision_ = 0;
};
