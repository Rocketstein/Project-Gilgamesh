#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "DeveloperTools/Console/ConsoleBuffer.h"

class ConsoleConfiguration;

class OutputLogPanel
{
public:
    static constexpr const char* WindowName =
        "Output Log###OutputLog";

    explicit OutputLogPanel(
        std::shared_ptr<ConsoleBuffer> buffer,
        ConsoleConfiguration& configuration);

    // Draws the console and returns a command when the user presses Enter.
    std::optional<std::string> Draw(bool* open = nullptr);

private:
    void RebuildFilteredIndices();

    std::shared_ptr<ConsoleBuffer> buffer_;
    ConsoleConfiguration& configuration_;

    std::array<char, 128> search_{};
    std::array<char, 256> commandInput_{};

    std::uint64_t lastSeenSequence_ = 0;
    std::deque<ConsoleEntry> entries_;
    std::vector<ConsoleEntry> pendingEntries_;
    std::vector<std::size_t> filteredIndices_;

    std::uint64_t lastConfigurationRevision_ = 0;
};
