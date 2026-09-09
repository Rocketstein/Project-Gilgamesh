#pragma once

class EditorWorkspace
{
public:
    void DrawDockSpace();

private:
    void BuildDefaultLayout(
        unsigned int dockspaceId,
        float width,
        float height);
};
