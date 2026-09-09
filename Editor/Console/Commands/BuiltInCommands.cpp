#include "BuiltInCommands.h"

#include <cstddef>
#include <format>
#include <optional>
#include <string_view>

#include "CommandInvocation.h"
#include "Editor/Console/ConsoleConfiguration.h"

namespace
{
    bool EqualsIgnoreCase(
        std::string_view left,
        std::string_view right)
    {
        if (left.size() != right.size())
            return false;

        for (std::size_t index = 0;
            index < left.size();
            ++index)
        {
            char lhs = left[index];
            char rhs = right[index];

            if (lhs >= 'A' && lhs <= 'Z')
                lhs = static_cast<char>(lhs - 'A' + 'a');

            if (rhs >= 'A' && rhs <= 'Z')
                rhs = static_cast<char>(rhs - 'A' + 'a');

            if (lhs != rhs)
                return false;
        }

        return true;
    }

    std::optional<LogLevel> ParseLogLevel(
        std::string_view value)
    {
        for (std::size_t index = 0;
            index < static_cast<std::size_t>(LogLevel::Count);
            ++index)
        {
            const auto level = static_cast<LogLevel>(index);

            if (EqualsIgnoreCase(value, ToString(level)))
                return level;
        }

        return std::nullopt;
    }

    CommandResult ClearCommand(
        const CommandInvocation& invocation)
    {
        if (!invocation.arguments.empty())
            return CommandResult::Usage();

        invocation.output.Clear();
        return CommandResult::Success();
    }

    CommandResult HelpCommand(
        const CommandInvocation& invocation)
    {
        if (invocation.arguments.size() > 1)
            return CommandResult::Usage();

        if (invocation.arguments.size() == 1)
        {
            const CommandDefinition* definition =
                invocation.registry.Find(
                    invocation.arguments.front());

            if (definition == nullptr)
            {
                return CommandResult::Error(
                    std::format(
                        "Unknown command '{}'.",
                        invocation.arguments.front()));
            }

            invocation.output.WriteInfo(
                std::format(
                    "{} - {}",
                    definition->name,
                    definition->description));

            invocation.output.WriteInfo(
                std::format(
                    "Usage: {}",
                    definition->usage));

            return CommandResult::Success();
        }

        invocation.output.WriteInfo("Available commands:");

        for (const CommandDefinition* definition
             : invocation.registry.ListCommands())
        {
            invocation.output.WriteInfo(
                std::format(
                    "  {:<12} {}",
                    definition->name,
                    definition->description));
        }

        invocation.output.WriteInfo(
            "Type 'help <command>' for detailed usage.");

        return CommandResult::Success();
    }

    CommandResult EchoCommand(
        const CommandInvocation& invocation)
    {
        if (invocation.arguments.empty())
            return CommandResult::Usage();

        std::string message;

        for (std::size_t index = 0;
            index < invocation.arguments.size();
            ++index)
        {
            if (index != 0)
                message += ' ';

            message += invocation.arguments[index];
        }

        return CommandResult::Success(
            std::move(message));
    }

    CommandHandler MakeLogLevelVisibilityCommand(
        ConsoleConfiguration& configuration,
        bool visible)
    {
        return [&configuration, visible](
            const CommandInvocation& invocation)
        {
            if (invocation.arguments.size() != 1)
                return CommandResult::Usage();

            const std::optional<LogLevel> level =
                ParseLogLevel(invocation.arguments.front());

            if (!level)
            {
                return CommandResult::Error(
                    "Unknown log level. Expected: trace, debug, "
                    "info, warning, error, or critical.");
            }

            configuration.SetLogLevelVisible(*level, visible);

            return CommandResult::Success(
                std::format(
                    "{} log level {}.",
                    ToString(*level),
                    visible ? "shown" : "hidden"));
        };
    }
}

CommandRegistry CreateBuiltInCommandRegistry(
    ConsoleConfiguration& configuration)
{
    return CommandRegistry{
        {
            .name = "clear",
            .description = "Clears the console output.",
            .usage = "clear",
            .handler = &ClearCommand
        },
        {
            .name = "help",
            .description = "Displays available commands.",
            .usage = "help [command]",
            .handler = &HelpCommand
        },
        {
            .name = "echo",
            .description = "Mirrors the command line.",
            .usage = "echo <message>",
            .handler = &EchoCommand
        },
        {
            .name = "log.showlevel",
            .description = "Shows messages at a log level.",
            .usage = "log.showlevel <level>",
            .handler = MakeLogLevelVisibilityCommand(
                configuration,
                true)
        },
        {
            .name = "log.hidelevel",
            .description = "Hides messages at a log level.",
            .usage = "log.hidelevel <level>",
            .handler = MakeLogLevelVisibilityCommand(
                configuration,
                false)
        },
    };
}
