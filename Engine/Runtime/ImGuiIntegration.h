#pragma once

#include <Windows.h>

struct ID3D11Device;
struct ID3D11DeviceContext;

class ImGuiLayer
{
public:
    ImGuiLayer() = default;
    ~ImGuiLayer();

    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;

    bool Initialize(
        HWND window,
        ID3D11Device* device,
        ID3D11DeviceContext* context);

    void BeginFrame();
    void Render();

private:
    bool initialized_ = false;
};