#pragma once

class DeveloperToolsWorkspace
{
public:
    void DrawDockSpace();

private:
    void BuildDefaultLayout(
        unsigned int dockspaceId,
        float width,
        float height);
};