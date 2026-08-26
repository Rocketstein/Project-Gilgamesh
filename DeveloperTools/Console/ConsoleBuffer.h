#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <source_location>
#include <string>
#include <vector>

#include "Engine/Core/Logging/LogTypes.h"

enum class ConsoleEntryKind
{
    Log,
    CommandInput,
    CommandOutput
};

enum class ConsoleEntryTone
{
    Normal,
    Warning,
    Error
};

struct ConsoleEntry
{
    ConsoleEntryKind kind = ConsoleEntryKind::Log;
    ConsoleEntryTone tone = ConsoleEntryTone::Normal;

    std::string message;
    std::chrono::system_clock::time_point timestamp;

    // Present only for entries originating from Logger.
    std::optional<LogCategory> logCategory;
    std::optional<LogLevel> logLevel;
    std::source_location source;
    bool hasSource = false;
};

// Thread-safe bounded history shared by log and command producers and consumed
// by the in-process console UI.
class ConsoleBuffer
{
public:
    explicit ConsoleBuffer(std::size_t capacity = 5000);

    void Push(ConsoleEntry entry);

    [[nodiscard]]
    bool Snapshot(
        std::uint64_t& lastSeenRevision,
        std::vector<ConsoleEntry>& output) const;

    [[nodiscard]]
    std::size_t Size() const;

    [[nodiscard]]
    std::size_t Capacity() const noexcept;

    void Clear();

private:
    mutable std::mutex mutex_;
    std::deque<ConsoleEntry> entries_;
    std::size_t capacity_;
    std::uint64_t revision_ = 0;
};
