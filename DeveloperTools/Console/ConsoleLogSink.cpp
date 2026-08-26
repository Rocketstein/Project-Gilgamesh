#include "ConsoleLogSink.h"

#include <cassert>
#include <utility>

namespace
{
    ConsoleEntryTone ToneFor(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::Warning:
            return ConsoleEntryTone::Warning;

        case LogLevel::Error:
        case LogLevel::Critical:
            return ConsoleEntryTone::Error;

        default:
            return ConsoleEntryTone::Normal;
        }
    }
}

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
        .kind = ConsoleEntryKind::Log,
        .tone = ToneFor(entry.level),
        .message = entry.message,
        .timestamp = entry.timestamp,
        .logCategory = entry.category,
        .logLevel = entry.level,
        .source = entry.source,
        .hasSource = true
    });
}
