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

    entry.sequence = ++latestSequence_;

    if (entries_.size() == capacity_)
        entries_.pop_front();

    entries_.push_back(std::move(entry));
}

bool ConsoleBuffer::ReadDelta(
    std::uint64_t& lastSeenSequence,
    std::uint64_t& discardBeforeSequence,
    std::vector<ConsoleEntry>& appendedEntries) const
{
    std::scoped_lock lock(mutex_);

    appendedEntries.clear();

    if (lastSeenSequence == latestSequence_)
        return false;

    discardBeforeSequence = entries_.empty()
        ? latestSequence_ + 1
        : entries_.front().sequence;

    const auto firstNewEntry = std::upper_bound(
        entries_.begin(),
        entries_.end(),
        lastSeenSequence,
        [](std::uint64_t sequence, const ConsoleEntry& entry)
        {
            return sequence < entry.sequence;
        });

    appendedEntries.assign(firstNewEntry, entries_.end());
    lastSeenSequence = latestSequence_;
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

    if (entries_.empty())
        return;

    entries_.clear();

    // Reserve a sequence for the clear operation so a reader that already saw
    // the newest entry can still detect that its cached history is now stale.
    ++latestSequence_;
}
