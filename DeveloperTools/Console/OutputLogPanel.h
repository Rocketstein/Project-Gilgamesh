#pragma once

#include <array>
#include <cstddef>
#include <memory>

#include "DeveloperTools/Console/ConsoleLogSink.h"

class OutputLogPanel
{
public:
    explicit OutputLogPanel(
        std::shared_ptr<ConsoleLogSink> sink);

    // open can be controlled later by a developer-tools menu.
    void Draw(bool* open = nullptr);

private:
    static constexpr std::size_t LevelCount =
        static_cast<std::size_t>(LogLevel::Count);

    std::shared_ptr<ConsoleLogSink> sink_;

    std::array<bool, LevelCount> visibleLevels_{};
    std::array<char, 128> search_{};

    bool autoScroll_ = true;
};