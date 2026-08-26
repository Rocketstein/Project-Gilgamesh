#pragma once

#include <span>
#include <string>
#include <string_view>

struct CommandContext;

enum class CommandStatus
{
    Success,
    UsageError,
    ExecutionError
};

struct CommandResult
{
    CommandStatus status = CommandStatus::Success;
    std::string message;

    static CommandResult Success(std::string message = {});

    // Leave the message empty to have the registry fill in the usage text from
    // the command's own CommandDefinition, which is the source of truth.
    static CommandResult Usage(std::string message = {});

    static CommandResult Error(std::string message);
};

using CommandHandler = CommandResult(*)(
    CommandContext& context,
    std::span<const std::string_view> arguments);

struct CommandDefinition
{
    std::string name;
    std::string description;
    std::string usage;

    CommandHandler handler = nullptr;
};
