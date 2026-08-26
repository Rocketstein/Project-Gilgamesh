#include "OutputLogPanel.h"

#include <cassert>
#include <string_view>
#include <utility>
#include <vector>

#include "imgui.h"

namespace
{
    constexpr std::size_t ToIndex(LogLevel level)
    {
        return static_cast<std::size_t>(level);
    }

    ImVec4 ColorFor(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::Trace:
            return { 0.65f, 0.65f, 0.65f, 1.0f };

        case LogLevel::Debug:
            return { 0.55f, 0.75f, 1.0f, 1.0f };

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

    bool MatchesSearch(
        const LogEntry& entry,
        std::string_view search)
    {
        if (search.empty())
            return true;

        return entry.message.find(search) != std::string::npos
            || ToString(entry.category).find(search)
            != std::string_view::npos
            || ToString(entry.level).find(search)
            != std::string_view::npos;
    }
} // Anonymous Namespace

OutputLogPanel::OutputLogPanel(
    std::shared_ptr<ConsoleLogSink> sink)
    : sink_(std::move(sink))
{
    assert(sink_ != nullptr);
    visibleLevels_.fill(true);
}

void OutputLogPanel::Draw(bool* open)
{
    if (sink_ == nullptr)
        return;

    if (!ImGui::Begin("Output Log", open))
    {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Clear"))
        sink_->Clear();

    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &autoScroll_);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(240.0f);
    ImGui::InputText(
        "Search",
        search_.data(),
        search_.size());

    for (std::size_t index = 0;
        index < LevelCount;
        ++index)
    {
        if (index != 0)
            ImGui::SameLine();

        const auto level =
            static_cast<LogLevel>(index);

        ImGui::Checkbox(
            ToString(level).data(),
            &visibleLevels_[index]);
    }

    ImGui::Separator();

    const std::vector<LogEntry> entries =
        sink_->Snapshot();

    const std::string_view search(search_.data());

    std::vector<const LogEntry*> filteredEntries;
    filteredEntries.reserve(entries.size());

    for (const LogEntry& entry : entries)
    {
        if (!visibleLevels_[ToIndex(entry.level)])
            continue;

        if (!MatchesSearch(entry, search))
            continue;

        filteredEntries.push_back(&entry);
    }

    constexpr ImGuiTableFlags tableFlags =
        ImGuiTableFlags_BordersInnerV
        | ImGuiTableFlags_RowBg
        | ImGuiTableFlags_Resizable
        | ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable(
        "OutputLogTable",
        3,
        tableFlags,
        ImVec2(0.0f, 0.0f)))
    {
        ImGui::TableSetupColumn(
            "Category",
            ImGuiTableColumnFlags_WidthFixed,
            90.0f);

        ImGui::TableSetupColumn(
            "Level",
            ImGuiTableColumnFlags_WidthFixed,
            80.0f);

        ImGui::TableSetupColumn(
            "Message",
            ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        const bool shouldScroll =
            autoScroll_
            && ImGui::GetScrollY()
            >= ImGui::GetScrollMaxY();

        ImGuiListClipper clipper;
        clipper.Begin(
            static_cast<int>(filteredEntries.size()));

        while (clipper.Step())
        {
            for (int index = clipper.DisplayStart;
                index < clipper.DisplayEnd;
                ++index)
            {
                const LogEntry& entry =
                    *filteredEntries[
                        static_cast<std::size_t>(index)];

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(
                    ToString(entry.category).data());

                ImGui::TableSetColumnIndex(1);
                ImGui::PushStyleColor(
                    ImGuiCol_Text,
                    ColorFor(entry.level));

                ImGui::TextUnformatted(
                    ToString(entry.level).data());

                ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(
                    entry.message.c_str());

                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip(
                        "%s:%u\n%s",
                        entry.source.file_name(),
                        static_cast<unsigned>(
                            entry.source.line()),
                        entry.source.function_name());
                }
            }
        }
        if (shouldScroll)
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndTable();
    }

    ImGui::End();
}
