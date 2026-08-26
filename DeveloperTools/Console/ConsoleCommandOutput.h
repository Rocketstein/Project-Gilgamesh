#pragma once

#include <memory>
#include <string_view>

#include "DeveloperTools/Console/Commands/CommandContext.h"
#include "DeveloperTools/Console/Commands/CommandTypes.h"

class ConsoleLogSink;

// Adapts command responses to the existing logging-backed console.
class ConsoleCommandOutput final : public ICommandOutput
{
public:
    explicit ConsoleCommandOutput(
        std::shared_ptr<ConsoleLogSink> sink);

    void WriteInfo(std::string_view message) override;
    void WriteWarning(std::string_view message) override;
    void WriteError(std::string_view message) override;
    void Clear() override;

    void WriteCommand(std::string_view commandLine);
    void WriteResult(const CommandResult& result);

private:
    std::shared_ptr<ConsoleLogSink> sink_;
};
