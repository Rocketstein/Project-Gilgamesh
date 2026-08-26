#include "ConsoleLogSink.h"

#include <algorithm>
#include <limits>

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
    ++revision_;
}

bool ConsoleLogSink::Snapshot(
    std::uint64_t& lastSeenRevision,
    std::vector<LogEntry>& output) const
{
    std::scoped_lock lock(mutex_);

    if (lastSeenRevision == revision_)
        return false;

    output.assign(entries_.begin(), entries_.end());
    lastSeenRevision = revision_;

    return true;
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
    ++revision_;
}

void ConsoleLogSink::UpdateRevision()
{
    // Check if revision_ is at the maximum limit for size_t
    if (revision_ == std::numeric_limits<size_t>::max())
    {
        revision_ = 1;
    }
    else
    {
        revision_++;
    }
}