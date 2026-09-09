#include "CommandTypes.h"

#include <utility>

CommandResult CommandResult::Success(std::string message)
{
    return {
        .status = CommandStatus::Success,
        .message = std::move(message)
    };
}

CommandResult CommandResult::Usage(std::string message)
{
    return {
        .status = CommandStatus::UsageError,
        .message = std::move(message)
    };
}

CommandResult CommandResult::Error(std::string message)
{
    return {
        .status = CommandStatus::ExecutionError,
        .message = std::move(message)
    };
}
