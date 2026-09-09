#pragma once

#include <memory>

#include "ConsoleBuffer.h"
#include "Engine/Core/Logging/ILogSink.h"

// Adapts engine log entries into the shared console history.
class ConsoleLogSink final : public ILogSink
{
public:
    explicit ConsoleLogSink(
        std::shared_ptr<ConsoleBuffer> buffer);

    void Write(const LogEntry& entry) override;

private:
    std::shared_ptr<ConsoleBuffer> buffer_;
};
