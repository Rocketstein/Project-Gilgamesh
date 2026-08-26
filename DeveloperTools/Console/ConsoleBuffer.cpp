#include "ConsoleBuffer.h"

#include <algorithm>
#include <utility>

ConsoleBuffer::ConsoleBuffer(std::size_t capacity)
    : capacity_(std::max<std::size_t>(capacity, 1))
{
}

void ConsoleBuffer::Push(ConsoleEntry entry)
{
    std::scoped_lock lock(mutex_);

    if (entries_.size() == capacity_)
        entries_.pop_front();

    entries_.push_back(std::move(entry));
    ++revision_;
}

bool ConsoleBuffer::Snapshot(
    std::uint64_t& lastSeenRevision,
    std::vector<ConsoleEntry>& output) const
{
    std::scoped_lock lock(mutex_);

    if (lastSeenRevision == revision_)
        return false;

    output.assign(entries_.begin(), entries_.end());
    lastSeenRevision = revision_;
    return true;
}

std::size_t ConsoleBuffer::Size() const
{
    std::scoped_lock lock(mutex_);
    return entries_.size();
}

std::size_t ConsoleBuffer::Capacity() const noexcept
{
    return capacity_;
}

void ConsoleBuffer::Clear()
{
    std::scoped_lock lock(mutex_);
    entries_.clear();
    ++revision_;
}
