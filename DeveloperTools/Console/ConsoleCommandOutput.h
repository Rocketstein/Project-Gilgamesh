#pragma once

#include <memory>
#include <string_view>

#include "DeveloperTools/Console/Commands/CommandInvocation.h"
#include "DeveloperTools/Console/Commands/CommandTypes.h"
#include "DeveloperTools/Console/ConsoleBuffer.h"

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
