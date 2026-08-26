#pragma once

#include <initializer_list>
#include <unordered_map>
#include <vector>

#include "CommandTypes.h"

class CommandRegistry
{
public:
	CommandRegistry() = default;
	explicit CommandRegistry(
		std::initializer_list<CommandDefinition> definitions);

	bool Register(CommandDefinition definition);
	CommandResult Execute(
		CommandContext& context,
		std::string_view commandLine);

	[[nodiscard]]
	const CommandDefinition* Find(
		std::string_view name) const;

	[[nodiscard]]
	std::vector<const CommandDefinition*>
		ListCommands() const;

private:
	std::unordered_map <std::string, CommandDefinition>
		commands_;

};
