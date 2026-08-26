#include "DeveloperToolsWorkspace.h"

#include "DeveloperTools/Console/OutputLogPanel.h"
#include "imgui.h"
#include "imgui_internal.h"

void DeveloperToolsWorkspace::DrawDockSpace()
{
    const ImGuiViewport* viewport =
        ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    constexpr ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoDocking
        | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoNavFocus
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(
        ImGuiStyleVar_WindowRounding,
        0.0f);

    ImGui::PushStyleVar(
        ImGuiStyleVar_WindowBorderSize,
        0.0f);

    ImGui::PushStyleVar(
        ImGuiStyleVar_WindowPadding,
        ImVec2(0.0f, 0.0f));

    ImGui::Begin(
        "Developer Tools Workspace",
        nullptr,
        windowFlags);

    ImGui::PopStyleVar(3);

    const ImGuiID dockspaceId =
        ImGui::GetID("DeveloperToolsDockSpace");

    BuildDefaultLayout(
        dockspaceId,
        viewport->WorkSize.x,
        viewport->WorkSize.y);

    constexpr ImGuiDockNodeFlags dockspaceFlags =
        ImGuiDockNodeFlags_PassthruCentralNode;

    ImGui::DockSpace(
        dockspaceId,
        ImVec2(0.0f, 0.0f),
        dockspaceFlags);

    ImGui::End();
}

void DeveloperToolsWorkspace::BuildDefaultLayout(
    unsigned int dockspaceId,
    float width,
    float height)
{
    if (ImGui::DockBuilderGetNode(dockspaceId) != nullptr)
        return;

    ImGui::DockBuilderAddNode(
        dockspaceId,
        ImGuiDockNodeFlags_DockSpace);

    ImGui::DockBuilderSetNodeSize(
        dockspaceId,
        ImVec2(width, height));

    ImGuiID centralNode = dockspaceId;
    ImGuiID bottomNode = 0;

    ImGui::DockBuilderSplitNode(
        centralNode,
        ImGuiDir_Down,
        0.30f,
        &bottomNode,
        &centralNode);

    ImGui::DockBuilderDockWindow(
        OutputLogPanel::WindowName,
        bottomNode);

    ImGui::DockBuilderFinish(dockspaceId);
}
