#include "ImGuiIntegration.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

// imgui_impl_win32.h intentionally omits this declaration so that including
// the backend header does not pull Windows types into unrelated code. The
// backend asks the application integration point to declare it where needed.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam);

bool ImGuiIntegration::Initialize(
    Window& window,
    ID3D11Device* device,
    ID3D11DeviceContext* context)
{
    const HWND hwnd = window.GetNativeHandle();

    if (hwnd == nullptr
        || device == nullptr
        || context == nullptr
        || initialized_)
    {
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    // Stage one: platform backend.
    if (!ImGui_ImplWin32_Init(hwnd))
    {
        // Win32 did not initialize, so only destroy the context.
        ImGui::DestroyContext();
        return false;
    }

    // Stage two: renderer backend.
    if (!ImGui_ImplDX11_Init(device, context))
    {
        // Win32 succeeded, so roll it back.
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    // Both backends are now ready. Mark this object initialized before making
    // it reachable through Window's callback.
    window_ = &window;
    initialized_ = true;

    // Make messages able to reach ImGui.
    window_->SetMessageHandler(
        &ImGuiIntegration::HandleWindowMessage,
        this);

    return true;
}

ImGuiIntegration::~ImGuiIntegration()
{
    if (!initialized_)
        return;

    // Disconnect first so later destruction messages from Window cannot reach
    // an ImGui context that has already been destroyed.
    if (window_ != nullptr)
    {
        window_->ClearMessageHandler();
        window_ = nullptr;
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    initialized_ = false;
}

void ImGuiIntegration::BeginFrame()
{
    // Refresh renderer resources, then platform input/timing, then begin the
    // Dear ImGui frame in the order required by the stock backends.
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiIntegration::Render()
{
    // Finalize all widgets built this frame and draw them into the render
    // target currently bound by Renderer::BeginFrame.
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

WindowMessageResult ImGuiIntegration::HandleWindowMessage(
    void* userData,
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    // Recover the owning integration from Window's opaque callback payload.
    auto* self = static_cast<ImGuiIntegration*>(userData);

    if (self == nullptr || !self->initialized_)
    {
        return {};
    }

    // A zero result means Dear ImGui did not consume this Win32 message.
    const LRESULT result =
        ImGui_ImplWin32_WndProcHandler(
            hwnd,
            message,
            wParam,
            lParam);

    return {
        .handled = result != 0,
        .result = result
    };
}
