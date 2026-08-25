#include "BufferedLogSink.h"

#include <algorithm>

BufferedLogSink::BufferedLogSink(std::size_t capacity)
    : capacity_(std::max<std::size_t>(capacity, 1))
{
}

void BufferedLogSink::Write(const LogEntry& entry)
{
    std::scoped_lock lock(mutex_);

    if (entries_.size() == capacity_)
        entries_.pop_front();

    entries_.push_back(entry);
}

std::vector<LogEntry> BufferedLogSink::Snapshot() const
{
    std::scoped_lock lock(mutex_);
    return { entries_.begin(), entries_.end() };
}

std::size_t BufferedLogSink::Size() const
{
    std::scoped_lock lock(mutex_);
    return entries_.size();
}

std::size_t BufferedLogSink::Capacity() const noexcept
{
    return capacity_;
}

void BufferedLogSink::Clear()
{
    std::scoped_lock lock(mutex_);
    entries_.clear();
}
