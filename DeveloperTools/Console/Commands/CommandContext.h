#pragma once

#include <string_view>

class Renderer;
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

// Services made available to command handlers. Add another service here only
// when a command genuinely needs it; this keeps handler dependencies visible.
struct CommandContext
{
    Renderer& renderer;
    CommandRegistry& registry;
    ICommandOutput& output;
};
