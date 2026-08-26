#include "BuiltInCommands.h"

#include <format>

#include "CommandContext.h"

namespace
{
    CommandResult ClearCommand(
        CommandContext& context,
        std::span<const std::string_view> arguments)
    {
        if (!arguments.empty())
            return CommandResult::Usage("clear");

        context.output.Clear();
        return CommandResult::Success();
    }

    CommandResult HelpCommand(
        CommandContext& context,
        std::span<const std::string_view> arguments)
    {
        if (arguments.size() > 1)
            return CommandResult::Usage("help [command]");

        if (arguments.size() == 1)
        {
            const CommandDefinition* definition =
                context.registry.Find(arguments.front());

            if (definition == nullptr)
            {
                return CommandResult::Error(
                    std::format(
                        "Unknown command '{}'.",
                        arguments.front()));
            }

            context.output.WriteInfo(
                std::format(
                    "{} - {}",
                    definition->name,
                    definition->description));

            context.output.WriteInfo(
                std::format(
                    "Usage: {}",
                    definition->usage));

            return CommandResult::Success();
        }

        context.output.WriteInfo("Available commands:");

        for (const CommandDefinition* definition
             : context.registry.ListCommands())
        {
            context.output.WriteInfo(
                std::format(
                    "  {:<12} {}",
                    definition->name,
                    definition->description));
        }

        context.output.WriteInfo(
            "Type 'help <command>' for detailed usage.");

        return CommandResult::Success();
    }

    CommandResult EchoCommand(
        CommandContext& context,
        std::span<const std::string_view> arguments)
    {
        if (arguments.empty())
        {
            return CommandResult::Usage(
                "Usage: echo <message>");
        }

        std::string message;

        for (std::size_t index = 0;
            index < arguments.size();
            ++index)
        {
            if (index != 0)
                message += ' ';

            message += arguments[index];
        }

        return CommandResult::Success(
            std::move(message));
    }
}

CommandRegistry CreateBuiltInCommandRegistry()
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
            .usage = "echo [command]",
            .handler = &EchoCommand
        },
    };
}
