#pragma once

#include "ILogSink.h"

// Writes to the debugger attached to this process. In Visual Studio, these
// messages appear in the Output window even though Gilgamesh is a WIN32 app
// without a terminal of its own.
class DebugOutputSink final : public ILogSink
{
public:
    void Write(const LogEntry& entry) override;
};
