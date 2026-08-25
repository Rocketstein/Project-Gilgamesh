#pragma once

#include "Engine/Core/Logging/ILogSink.h"

// Writes to the debugger attached to this Windows process. In Visual Studio,
// these messages appear in the Output window even though Gilgamesh is a WIN32
// application without a terminal of its own.

// In the long term, this feature should evolve into a dump file.
class DebugOutputSink final : public ILogSink
{
public:
    void Write(const LogEntry& entry) override;
};
