#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <source_location>
#include <string>
#include <variant>
#include <vector>

#include "Engine/Core/Logging/LogTypes.h"

enum class ConsoleEntryTone
{
    Normal,
    Warning,
    Error
};

struct ConsoleLogMetadata
{
    LogCategory category;
    LogLevel level;
    std::source_location source;
};

struct ConsoleCommandInputMetadata
{
};

struct ConsoleCommandOutputMetadata
{
    ConsoleEntryTone tone;
};

using ConsoleEntryMetadata = std::variant<
    ConsoleLogMetadata,
    ConsoleCommandInputMetadata,
    ConsoleCommandOutputMetadata>;

struct ConsoleEntry
{
    ConsoleEntryMetadata metadata;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
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
