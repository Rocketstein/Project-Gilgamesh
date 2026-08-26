#pragma once

#include "CommandRegistry.h"

class ConsoleConfiguration;

// Constructs a registry pre-populated with the commands that are always
// available in the developer console.
[[nodiscard]]
CommandRegistry CreateBuiltInCommandRegistry(
    ConsoleConfiguration& configuration);
