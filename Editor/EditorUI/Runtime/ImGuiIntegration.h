#pragma once

#include "Engine/Platform/Windows/Window.h"

struct ID3D11Device;
struct ID3D11DeviceContext;

class ImGuiIntegration
{
public:
    ImGuiIntegration() = default;
    ~ImGuiIntegration();

    ImGuiIntegration(const ImGuiIntegration&) = delete;
    ImGuiIntegration& operator=(const ImGuiIntegration&) = delete;

    // Window must outlive this integration. We need both its HWND for the
    // Win32 backend and its generic callback API for message forwarding.
    bool Initialize(
        Window& window,
        ID3D11Device* device,
        ID3D11DeviceContext* context);

    void BeginFrame();
    void Render();

    // Static adapter matching WindowMessageHandler. userData recovers this
    // object without making Window depend on Dear ImGui.
    static WindowMessageResult HandleWindowMessage(
        void* userData,
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);

private:
    // Non-owning pointer used to disconnect the callback during destruction.
    Window* window_ = nullptr;

    bool initialized_ = false;
};
