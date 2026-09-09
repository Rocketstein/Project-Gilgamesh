#include "EditorViewportPanel.h"

#include "Editor/EditorViewport/EditorViewport.h"

#include <cstdint>

#include <imgui.h>

EditorViewportPanelFrame EditorViewportPanel::Draw(
    EditorViewport& viewport)
{
    EditorViewportPanelFrame frame;

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleVar(
        ImGuiStyleVar_WindowPadding,
        ImVec2(0.0f, 0.0f));

    const bool visible =
        ImGui::Begin(WindowName, nullptr, flags);

    ImGui::PopStyleVar();

    if (visible)
    {
        frame.focused = ImGui::IsWindowFocused(
            ImGuiFocusedFlags_RootAndChildWindows);

        const ImVec2 availableSize =
            ImGui::GetContentRegionAvail();

        if (availableSize.x >= 1.0f &&
            availableSize.y >= 1.0f)
        {
            const Extent2D requestedExtent{
                static_cast<std::uint32_t>(availableSize.x),
                static_cast<std::uint32_t>(availableSize.y)
            };

            viewport.RequestResize(requestedExtent);
            viewport.ApplyPendingResize();

            if (viewport.IsReady())
            {
                ID3D11ShaderResourceView* srv =
                    viewport.GetShaderResourceView();

                const ImTextureRef texture{
                    static_cast<ImTextureID>(
                        reinterpret_cast<std::uintptr_t>(srv))
                };

                ImGui::Image(texture, availableSize);

                frame.shouldRender = true;
                frame.imageHovered = ImGui::IsItemHovered();
                frame.renderExtent = viewport.GetExtent();
            }
        }
    }

    ImGui::End();
    return frame;
}
