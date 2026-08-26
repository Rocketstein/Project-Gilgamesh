#include "ConsoleCommandOutput.h"

#include <cassert>
#include <chrono>
#include <utility>

#include "DeveloperTools/Console/ConsoleBuffer.h"

ConsoleCommandOutput::ConsoleCommandOutput(
    std::shared_ptr<ConsoleBuffer> buffer)
    : buffer_(std::move(buffer))
{
    assert(buffer_ != nullptr);
}

void ConsoleCommandOutput::WriteInfo(std::string_view message)
{
    Push(
        ConsoleEntryKind::CommandOutput,
        ConsoleEntryTone::Normal,
        message);
}

void ConsoleCommandOutput::WriteWarning(std::string_view message)
{
    Push(
        ConsoleEntryKind::CommandOutput,
        ConsoleEntryTone::Warning,
        message);
}

void ConsoleCommandOutput::WriteError(std::string_view message)
{
    Push(
        ConsoleEntryKind::CommandOutput,
        ConsoleEntryTone::Error,
        message);
}

void ConsoleCommandOutput::Clear()
{
    if (buffer_ != nullptr)
        buffer_->Clear();
}

void ConsoleCommandOutput::WriteCommand(
    std::string_view commandLine)
{
    Push(
        ConsoleEntryKind::CommandInput,
        ConsoleEntryTone::Normal,
        commandLine);
}

void ConsoleCommandOutput::WriteResult(
    const CommandResult& result)
{
    if (result.message.empty())
        return;

    switch (result.status)
    {
    case CommandStatus::Success:
        WriteInfo(result.message);
        break;

    case CommandStatus::UsageError:
        WriteWarning(result.message);
        break;

    case CommandStatus::ExecutionError:
        WriteError(result.message);
        break;
    }
}

void ConsoleCommandOutput::Push(
    ConsoleEntryKind kind,
    ConsoleEntryTone tone,
    std::string_view message)
{
    if (buffer_ == nullptr)
        return;

    buffer_->Push({
        .kind = kind,
        .tone = tone,
        .message = std::string(message),
        .timestamp = std::chrono::system_clock::now()
    });
}
