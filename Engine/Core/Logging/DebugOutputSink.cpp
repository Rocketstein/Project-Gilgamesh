#include "DebugOutputSink.h"

#include <Windows.h>

#include <format>
#include <string>

void DebugOutputSink::Write(const LogEntry& entry)
{
    const std::string output = std::format(
        "[{}][{}] {} ({}:{})\n",
        ToString(entry.category),
        ToString(entry.level),
        entry.message,
        entry.source.file_name(),
        entry.source.line());

    OutputDebugStringA(output.c_str());
}
