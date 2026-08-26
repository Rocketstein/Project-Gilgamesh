#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "DeveloperTools/Console/ConsoleLogSink.h"

class OutputLogPanel
{
public:
    explicit OutputLogPanel(
        std::shared_ptr<ConsoleLogSink> sink);

    // Draws the console and returns a command when the user presses Enter.
    // Command parsing and execution belong to the future command system.
    std::optional<std::string> Draw(bool* open = nullptr);

private:
    void RebuildFilteredIndices();

    static constexpr std::size_t LevelCount =
        static_cast<std::size_t>(LogLevel::Count);

    std::shared_ptr<ConsoleLogSink> sink_;

    std::array<bool, LevelCount> visibleLevels_{};
    std::array<char, 128> search_{};
    std::array<char, 256> commandInput_{};

    std::size_t snapshotRevision_ = 0;
    std::vector<LogEntry> entries_;
    std::vector<std::size_t> filteredIndices_;

    bool autoScroll_ = true;
};
