#pragma once

#include "LogTypes.h"

// The logger will dispatch entries, while sinks decide where they go. 
// No logging code should know anything about ImGui.
class ILogSink
{
public:
    virtual ~ILogSink() = default;

    virtual void Write(const LogEntry& entry) = 0;
};