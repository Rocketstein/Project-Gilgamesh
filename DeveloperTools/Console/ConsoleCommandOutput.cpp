#include "ConsoleCommandOutput.h"

#include <cassert>
#include <utility>

#include "DeveloperTools/Console/ConsoleLogSink.h"
#include "Engine/Core/Logging/Logger.h"

ConsoleCommandOutput::ConsoleCommandOutput(
    std::shared_ptr<ConsoleLogSink> sink)
    : sink_(std::move(sink))
{
    assert(sink_ != nullptr);
}

void ConsoleCommandOutput::WriteInfo(std::string_view message)
{
    GILGAMESH_LOG(Tools, Info, "{}", message);
}

void ConsoleCommandOutput::WriteWarning(std::string_view message)
{
    GILGAMESH_LOG(Tools, Warning, "{}", message);
}

void ConsoleCommandOutput::WriteError(std::string_view message)
{
    GILGAMESH_LOG(Tools, Error, "{}", message);
}

void ConsoleCommandOutput::Clear()
{
    if (sink_ != nullptr)
        sink_->Clear();
}

void ConsoleCommandOutput::WriteCommand(
    std::string_view commandLine)
{
    GILGAMESH_LOG(Tools, Info, "> {}", commandLine);
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
