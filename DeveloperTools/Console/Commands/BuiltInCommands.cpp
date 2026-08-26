#include "BuiltInCommands.h"

#include <format>

#include "CommandInvocation.h"

namespace
{
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
            .usage = "echo <message>",
            .handler = &EchoCommand
        },
    };
}
