#pragma once

#include "CommandRegistry.h"

// Constructs a registry pre-populated with the commands that are always
// available in the developer console.
[[nodiscard]]
CommandRegistry CreateBuiltInCommandRegistry();
