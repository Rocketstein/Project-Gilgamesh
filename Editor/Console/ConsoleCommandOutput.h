#pragma once

#include <memory>
#include <string_view>

#include "Editor/Console/Commands/CommandInvocation.h"
#include "Editor/Console/Commands/CommandTypes.h"
#include "Editor/Console/ConsoleBuffer.h"

// Writes command traffic directly to the console without routing through the
// engine logger or its other sinks.
class ConsoleCommandOutput final : public ICommandOutput
{
public:
    explicit ConsoleCommandOutput(
        std::shared_ptr<ConsoleBuffer> buffer);

    void WriteInfo(std::string_view message) override;
    void WriteWarning(std::string_view message) override;
    void WriteError(std::string_view message) override;
    void Clear() override;

    void WriteCommand(std::string_view commandLine);
    void WriteResult(const CommandResult& result);

private:
    void Push(
        ConsoleEntryMetadata metadata,
        std::string_view message);

    std::shared_ptr<ConsoleBuffer> buffer_;
};
