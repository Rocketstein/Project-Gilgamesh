#include "ConsoleLogSink.h"

#include <cassert>
#include <utility>

ConsoleLogSink::ConsoleLogSink(
    std::shared_ptr<ConsoleBuffer> buffer)
    : buffer_(std::move(buffer))
{
    assert(buffer_ != nullptr);
}

void ConsoleLogSink::Write(const LogEntry& entry)
{
    if (buffer_ == nullptr)
        return;

    buffer_->Push({
        .metadata = ConsoleLogMetadata{
            .category = entry.category,
            .level = entry.level,
            .source = entry.source
        },
        .message = entry.message,
        .timestamp = entry.timestamp
    });
}
