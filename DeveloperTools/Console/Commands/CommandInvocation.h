#pragma once

#include <span>
#include <string_view>

class CommandRegistry;

// Command handlers can emit additional lines without depending on ImGui or a
// particular console implementation. The final status still comes back in the
// handler's CommandResult.
class ICommandOutput
{
public:
    virtual ~ICommandOutput() = default;

    virtual void WriteInfo(std::string_view message) = 0;
    virtual void WriteWarning(std::string_view message) = 0;
    virtual void WriteError(std::string_view message) = 0;
    virtual void Clear() = 0;
};

// Per-execution command infrastructure. Engine dependencies belong in the
// individual handler's capture rather than in this shared invocation object.
struct CommandInvocation
{
    std::span<const std::string_view> arguments;
    const CommandRegistry& registry;
    ICommandOutput& output;
};
