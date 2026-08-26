#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

#include "Engine/Core/Logging/ILogSink.h"

// Stores the bounded history consumed by the in-process developer console.
// It deliberately has no dependency on Dear ImGui or any other presentation.
class ConsoleLogSink final : public ILogSink
{
public:
    explicit ConsoleLogSink(std::size_t capacity = 5000);

    void Write(const LogEntry& entry) override;

    [[nodiscard]] bool Snapshot(
        std::uint64_t& lastSeenRevision,
        std::vector<LogEntry>& output) const;

    [[nodiscard]] std::size_t Size() const;
    [[nodiscard]] std::size_t Capacity() const noexcept;

    void Clear();

private:
    void UpdateRevision();

private:
    mutable std::mutex mutex_;
    std::deque<LogEntry> entries_;
    std::size_t capacity_;
    std::size_t revision_ = 0;
};
