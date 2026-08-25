#pragma once

#include <cstddef>
#include <deque>
#include <mutex>
#include <vector>

#include "ILogSink.h"

// Retains a bounded, in-memory history for consumers such as an ImGui log
// panel. This sink produces no output by itself.
class BufferedLogSink final : public ILogSink
{
public:
    explicit BufferedLogSink(std::size_t capacity = 5000);

    void Write(const LogEntry& entry) override;

    [[nodiscard]] std::vector<LogEntry> Snapshot() const;
    [[nodiscard]] std::size_t Size() const;
    [[nodiscard]] std::size_t Capacity() const noexcept;

    void Clear();

private:
    mutable std::mutex mutex_;
    std::deque<LogEntry> entries_;
    std::size_t capacity_;
};
