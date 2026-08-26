#include "OutputLogPanel.h"

#include <cassert>
#include <string_view>
#include <utility>

#include "DeveloperTools/Console/ConsoleConfiguration.h"
#include "imgui.h"

namespace
{
    ImVec4 ColorFor(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::Trace:
            return { 0.65f, 0.65f, 0.65f, 1.0f };

        case LogLevel::Debug:
            return { 0.55f, 0.75f, 1.0f, 1.0f };

        case LogLevel::Info:
            return { 0.55f, 0.9f, 0.65f, 1.0f };

        case LogLevel::Warning:
            return { 1.0f, 0.75f, 0.2f, 1.0f };

        case LogLevel::Error:
            return { 1.0f, 0.35f, 0.3f, 1.0f };

        case LogLevel::Critical:
            return { 1.0f, 0.2f, 0.5f, 1.0f };

        default:
            return { 1.0f, 1.0f, 1.0f, 1.0f };
        }
    }

    ImVec4 ColorFor(LogCategory category)
    {
        switch (category)
        {
        case LogCategory::Core:
            return { 0.65f, 0.8f, 1.0f, 1.0f };

        case LogCategory::Platform:
            return { 0.45f, 0.85f, 0.9f, 1.0f };

        case LogCategory::Runtime:
            return { 0.65f, 0.9f, 0.65f, 1.0f };

        case LogCategory::Renderer:
            return { 0.95f, 0.7f, 0.4f, 1.0f };

        case LogCategory::Tools:
            return { 0.8f, 0.65f, 1.0f, 1.0f };

        case LogCategory::Misc:
        default:
            return { 0.75f, 0.75f, 0.75f, 1.0f };
        }
    }

    ImVec4 ColorFor(ConsoleEntryTone tone)
    {
        switch (tone)
        {
        case ConsoleEntryTone::Warning:
            return { 1.0f, 0.75f, 0.2f, 1.0f };

        case ConsoleEntryTone::Error:
            return { 1.0f, 0.35f, 0.3f, 1.0f };

        case ConsoleEntryTone::Normal:
        default:
            return { 1.0f, 1.0f, 1.0f, 1.0f };
        }
    }

    bool MatchesSearch(
        const ConsoleEntry& entry,
        std::string_view search)
    {
        if (search.empty())
            return true;

        if (entry.message.find(search) != std::string::npos)
            return true;

        const auto* log =
            std::get_if<ConsoleLogMetadata>(&entry.metadata);

        if (log == nullptr)
            return false;

        if (std::string_view(ToString(log->category)).find(search)
            != std::string_view::npos)
        {
            return true;
        }

        return std::string_view(ToString(log->level)).find(search)
                != std::string_view::npos;
    }
} // Anonymous Namespace

OutputLogPanel::OutputLogPanel(
    std::shared_ptr<ConsoleBuffer> buffer,
    ConsoleConfiguration& configuration)
    : buffer_(std::move(buffer)),
      configuration_(configuration)
{
    assert(buffer_ != nullptr);
}

void OutputLogPanel::RebuildFilteredIndices()
{
    filteredIndices_.clear();
    filteredIndices_.reserve(entries_.size());

    const std::string_view search(search_.data());

    for (std::size_t index = 0;
        index < entries_.size();
        ++index)
    {
        const ConsoleEntry& entry = entries_[index];

        const auto* log =
            std::get_if<ConsoleLogMetadata>(&entry.metadata);

        if (log
            && !configuration_.IsLogLevelVisible(log->level))
        {
            continue;
        }

        if (!MatchesSearch(entry, search))
            continue;

        filteredIndices_.push_back(index);
    }
}

std::optional<std::string> OutputLogPanel::Draw(bool* open)
{
    if (buffer_ == nullptr)
        return {};

    if (!ImGui::Begin(WindowName, open))
    {
        ImGui::End();
        return {};
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(240.0f);
    bool filterChanged = ImGui::InputText(
        "Search",
        search_.data(),
        search_.size());

    ImGui::Separator();

    std::uint64_t discardBeforeSequence = 0;

    const bool entriesChanged = buffer_->ReadDelta(
        lastSeenSequence_,
        discardBeforeSequence,
        pendingEntries_);

    if (entriesChanged)
    {
        while (!entries_.empty()
            && entries_.front().sequence
                < discardBeforeSequence)
        {
            entries_.pop_front();
        }

        for (ConsoleEntry& entry : pendingEntries_)
            entries_.push_back(std::move(entry));

        pendingEntries_.clear();
    }

    const std::uint64_t configurationRevision =
        configuration_.LogLevelVisibilityRevision();

    if (entriesChanged
        || filterChanged
        || configurationRevision != lastConfigurationRevision_)
    {
        RebuildFilteredIndices();

        lastConfigurationRevision_ = configurationRevision;
    }

    constexpr ImGuiTableFlags tableFlags =
        ImGuiTableFlags_RowBg
        | ImGuiTableFlags_ScrollY;

    const float commandInputHeight =
        ImGui::GetFrameHeightWithSpacing();

    if (ImGui::BeginTable(
        "OutputLogTable",
        1,
        tableFlags,
        ImVec2(0.0f, -commandInputHeight)))
    {
        ImGui::TableSetupColumn(
            "Message",
            ImGuiTableColumnFlags_WidthStretch);

        const bool shouldScroll =
            entriesChanged
            && ImGui::GetScrollY()
            >= ImGui::GetScrollMaxY();

        ImGuiListClipper clipper;
        clipper.Begin(
            static_cast<int>(filteredIndices_.size()));

        while (clipper.Step())
        {
            for (int index = clipper.DisplayStart;
                index < clipper.DisplayEnd;
                ++index)
            {
                const ConsoleEntry& entry =
                    entries_[filteredIndices_[
                        static_cast<std::size_t>(index)]];

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                ImGui::BeginGroup();

                const auto* log =
                    std::get_if<ConsoleLogMetadata>(
                        &entry.metadata);

                if (log != nullptr)
                {
                    ImGui::TextColored(
                        ColorFor(log->category),
                        "[%s]",
                        ToString(log->category));

                    ImGui::SameLine(0.0f, 0.0f);
                    ImGui::TextColored(
                        ColorFor(log->level),
                        "[%s] ",
                        ToString(log->level));

                    ImGui::SameLine(0.0f, 0.0f);
                    ImGui::TextUnformatted(entry.message.c_str());
                }
                else if (std::holds_alternative<
                    ConsoleCommandInputMetadata>(entry.metadata))
                {
                    ImGui::TextColored(
                        { 0.55f, 0.75f, 1.0f, 1.0f },
                        "> %s",
                        entry.message.c_str());
                }
                else
                {
                    const auto& commandOutput =
                        std::get<ConsoleCommandOutputMetadata>(
                            entry.metadata);

                    ImGui::PushStyleColor(
                        ImGuiCol_Text,
                        ColorFor(commandOutput.tone));

                    ImGui::TextUnformatted(entry.message.c_str());
                    ImGui::PopStyleColor();
                }

                ImGui::EndGroup();

                if (log != nullptr && ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip(
                        "%s:%u\n%s",
                        log->source.file_name(),
                        static_cast<unsigned>(
                            log->source.line()),
                        log->source.function_name());
                }
            }
        }
        if (shouldScroll)
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(">");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);

    const bool commandSubmitted = ImGui::InputTextWithHint(
        "##CommandInput",
        "Enter command...",
        commandInput_.data(),
        commandInput_.size(),
        ImGuiInputTextFlags_EnterReturnsTrue);

    std::optional<std::string> submittedCommand;

    if (commandSubmitted)
    {
        const std::string_view command(commandInput_.data());
        const std::size_t first = command.find_first_not_of(" \t");

        if (first != std::string_view::npos)
        {
            const std::size_t last =
                command.find_last_not_of(" \t");

            submittedCommand.emplace(
                command.substr(first, last - first + 1));
        }

        commandInput_.fill('\0');

        // Keep keyboard-driven command entry fast after submission.
        ImGui::SetKeyboardFocusHere(-1);
    }

    ImGui::End();
    return submittedCommand;
}
