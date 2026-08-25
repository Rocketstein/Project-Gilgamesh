#include "ConsoleLogSink.h"

#include <algorithm>

ConsoleLogSink::ConsoleLogSink(std::size_t capacity)
    : capacity_(std::max<std::size_t>(capacity, 1))
{
}

void ConsoleLogSink::Write(const LogEntry& entry)
{
    std::scoped_lock lock(mutex_);

    if (entries_.size() == capacity_)
        entries_.pop_front();

    entries_.push_back(entry);
}

std::vector<LogEntry> ConsoleLogSink::Snapshot() const
{
    std::scoped_lock lock(mutex_);
    return { entries_.begin(), entries_.end() };
}

std::size_t ConsoleLogSink::Size() const
{
    std::scoped_lock lock(mutex_);
    return entries_.size();
}

std::size_t ConsoleLogSink::Capacity() const noexcept
{
    return capacity_;
}

void ConsoleLogSink::Clear()
{
    std::scoped_lock lock(mutex_);
    entries_.clear();
}
