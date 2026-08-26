#include "CommandRegistry.h"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <utility>
#include <vector>

#include "CommandInvocation.h"
#include "CommandLineTokenizer.h"

namespace
{
    std::string NormalizeCommandName(std::string_view name)
    {
        std::string normalized(name);

        for (char& character : normalized)
        {
            if (character >= 'A' && character <= 'Z')
            {
                character =
                    static_cast<char>(
                        character - 'A' + 'a');
            }
        }

        return normalized;
    }

    bool IsValidCommandName(std::string_view name)
    {
        return !name.empty()
            && std::all_of(
                name.begin(),
                name.end(),
                [](char character)
                {
                    return (character >= 'a'
                            && character <= 'z')
                        || (character >= '0'
                            && character <= '9')
                        || character == '_'
                        || character == '-'
                        || character == '.';
                });
    }
} // Anonymous Namespace

CommandRegistry::CommandRegistry(
    std::initializer_list<CommandDefinition> definitions)
{
    for (const CommandDefinition& definition : definitions)
    {
        if (!Register(definition))
        {
            throw std::invalid_argument(
                std::format(
                    "Invalid or duplicate command '{}'",
                    definition.name));
        }
    }
}

bool CommandRegistry::Register(CommandDefinition definition)
{
    definition.name =
        NormalizeCommandName(definition.name);

    if (!IsValidCommandName(definition.name)
        || definition.handler == nullptr)
    {
        return false;
    }

    std::string key = definition.name;

    const bool inserted = commands_.emplace(
        std::move(key),
        std::move(definition)).second;

    return inserted;
}

CommandResult CommandRegistry::Execute(
    ICommandOutput& output,
    std::string_view commandLine)
{
    CommandLineTokenizeResult parsed =
        TokenizeCommandLine(commandLine);

    if (!parsed.Succeeded())
    {
        return CommandResult::Error(std::string(ToString(parsed.error)));
    }

    if (parsed.tokens.empty())
    {
        return CommandResult::Success();
    }

    std::string commandName =
        NormalizeCommandName(parsed.tokens.front());

    const auto found = commands_.find(commandName);

    if (found == commands_.end())
    {
        return CommandResult::Error(
            std::format(
                "Unknown command '{}'. Type 'help' for a list of available commands.",
                commandName));
    }

    std::vector<std::string_view> arguments;
    arguments.reserve(parsed.tokens.size() - 1);

    for (std::size_t index = 1;
        index < parsed.tokens.size();
        ++index)
    {
        arguments.emplace_back(parsed.tokens[index]);
    }

    const CommandDefinition& definition =
        found->second;

    const CommandInvocation invocation{
        .arguments = arguments,
        .registry = *this,
        .output = output
    };

    CommandResult result =
        definition.handler(invocation);

    if (result.status == CommandStatus::UsageError
        && result.message.empty())
    {
        result.message = std::format(
            "Usage: {}",
            definition.usage);
    }

    return result;
}

const CommandDefinition* CommandRegistry::Find(
    std::string_view name) const
{
    const auto found = commands_.find(
        NormalizeCommandName(name));

    if (found == commands_.end())
        return nullptr;

    return &found->second;
}

std::vector<const CommandDefinition*>
CommandRegistry::ListCommands() const
{
    std::vector<const CommandDefinition*> definitions;
    definitions.reserve(commands_.size());

    for (const auto& entry : commands_)
        definitions.push_back(&entry.second);

    std::sort(
        definitions.begin(),
        definitions.end(),
        [](const CommandDefinition* left,
           const CommandDefinition* right)
        {
            return left->name < right->name;
        });

    return definitions;
}
