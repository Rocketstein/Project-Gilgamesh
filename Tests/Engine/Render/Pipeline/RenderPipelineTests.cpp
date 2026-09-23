#include "Engine/Render/Pipeline/RenderPipeline.h"
#include "Engine/Render/Shader/ShaderManager.h"
#include "Tests/TestFramework.h"

#include <array>
#include <filesystem>
#include <string_view>

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

namespace
{
    struct TestDevice
    {
        Microsoft::WRL::ComPtr<ID3D11Device> device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    };

    const TestDevice& GetTestDevice()
    {
        static const TestDevice device = []
        {
            TestDevice result;
            const HRESULT createResult = D3D11CreateDevice(
                nullptr,
                D3D_DRIVER_TYPE_WARP,
                nullptr,
                0,
                nullptr,
                0,
                D3D11_SDK_VERSION,
                result.device.GetAddressOf(),
                nullptr,
                result.context.GetAddressOf());

            if (FAILED(createResult))
            {
                result.device.Reset();
                result.context.Reset();
            }

            return result;
        }();

        return device;
    }

    std::filesystem::path GetShaderDirectory()
    {
        std::array<wchar_t, MAX_PATH> executablePath{};
        const DWORD length = GetModuleFileNameW(
            nullptr,
            executablePath.data(),
            static_cast<DWORD>(executablePath.size()));
        if (length == 0 || length == executablePath.size()) return {};

        return std::filesystem::path{
            std::wstring_view{ executablePath.data(), length }
        }.parent_path() / L"Shaders";
    }

    PassDescriptor MakeDescriptor(const char* name)
    {
        return {
            .name = name,
            .vertexShader = L"EditorGrid",
            .pixelShader = L"EditorGrid"
        };
    }

    class ValidPass final : public IRenderPass
    {
    public:
        static PassDescriptor Describe()
        {
            return MakeDescriptor("Valid");
        }

        void Execute(
            RenderPassContext&,
            const RenderFrameData&) override
        {
        }
    };

    class MissingPixelShaderPass final : public IRenderPass
    {
    public:
        static PassDescriptor Describe()
        {
            PassDescriptor descriptor = MakeDescriptor("MissingPixelShader");
            descriptor.pixelShader = L"MissingRenderPipelineShader";
            return descriptor;
        }

        void Execute(
            RenderPassContext&,
            const RenderFrameData&) override
        {
        }
    };
}

GILGAMESH_TEST(
    "Engine.Render.RenderPipeline",
    "RejectsInvalidDependenciesAndAllowsRetry")
{
    RenderPipeline pipeline;
    pipeline.AddPass<ValidPass>();

    ShaderManager shaders;
    GILGAMESH_CHECK_MESSAGE(
        !pipeline.Initialize(nullptr, shaders),
        "pipeline initialization must reject a null device");
    GILGAMESH_CHECK_MESSAGE(
        !pipeline.IsInitialized(),
        "a rejected device must leave the pipeline uninitialized");

    const TestDevice& testDevice = GetTestDevice();
    GILGAMESH_CHECK_MESSAGE(
        testDevice.device != nullptr,
        "the WARP test device must be available");
    if (!testDevice.device) return;

    GILGAMESH_CHECK_MESSAGE(
        !pipeline.Initialize(testDevice.device.Get(), shaders),
        "pipeline initialization must reject an uninitialized shader manager");
    GILGAMESH_CHECK_MESSAGE(
        !pipeline.IsInitialized(),
        "a rejected shader manager must leave the pipeline uninitialized");

    GILGAMESH_CHECK_MESSAGE(
        shaders.Initialize(testDevice.device.Get(), GetShaderDirectory()),
        "the shader manager must initialize against the test shader directory");
    if (!shaders.IsInitialized()) return;

    GILGAMESH_CHECK_MESSAGE(
        pipeline.Initialize(testDevice.device.Get(), shaders),
        "the same pipeline must remain retryable after invalid dependencies");
    GILGAMESH_CHECK_MESSAGE(
        pipeline.IsInitialized(),
        "successful initialization must publish the prepared pipeline");

    bool rejectedLatePass = false;
    try
    {
        pipeline.AddPass<ValidPass>();
    }
    catch (const std::logic_error&)
    {
        rejectedLatePass = true;
    }
    GILGAMESH_CHECK_MESSAGE(
        rejectedLatePass,
        "an initialized pipeline must reject additional passes");
}

GILGAMESH_TEST(
    "Engine.Render.RenderPipeline",
    "DoesNotPublishPartiallyPreparedEntries")
{
    const TestDevice& testDevice = GetTestDevice();
    GILGAMESH_CHECK_MESSAGE(
        testDevice.device != nullptr,
        "the WARP test device must be available");
    if (!testDevice.device) return;

    ShaderManager shaders;
    GILGAMESH_CHECK_MESSAGE(
        shaders.Initialize(testDevice.device.Get(), GetShaderDirectory()),
        "the shader manager must initialize against the test shader directory");
    if (!shaders.IsInitialized()) return;

    RenderPipeline pipeline;
    pipeline.AddPass<ValidPass>();
    pipeline.AddPass<MissingPixelShaderPass>();

    GILGAMESH_CHECK_MESSAGE(
        !pipeline.Initialize(testDevice.device.Get(), shaders),
        "a later shader failure must fail the complete pipeline initialization");
    GILGAMESH_CHECK_MESSAGE(
        !pipeline.IsInitialized(),
        "failed preparation must not publish an earlier prepared entry");
}
