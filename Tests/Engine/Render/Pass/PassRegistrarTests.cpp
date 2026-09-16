#include "Engine/Render/Pass/PassRegistrar.h"
#include "Tests/TestFramework.h"

namespace
{
    PassDescriptor MakeDescriptor(const char* name)
    {
        PassDescriptor descriptor{};
        descriptor.name = name;
        descriptor.vertexShader = L"Primitive";
        descriptor.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        return descriptor;
    }

    class FirstPass final : public IRenderPass
    {
    public:
        static PassDescriptor Describe() { return MakeDescriptor("First"); }

        void Execute(RenderPassContext&, const RenderFrameData&) override {}
    };

    class SecondPass final : public IRenderPass
    {
    public:
        static PassDescriptor Describe()
        {
            PassDescriptor descriptor = MakeDescriptor("Second");
            descriptor.vertexShader = L"EditorGrid";
            descriptor.pixelShader = L"EditorGrid";
            descriptor.depthStencil.depthTestEnabled = true;
            descriptor.depthStencil.depthWriteEnabled = false;
            descriptor.blend.enabled = true;
            return descriptor;
        }

        void Execute(RenderPassContext&, const RenderFrameData&) override {}
    };

    class OwnedPass final : public IRenderPass
    {
    public:
        explicit OwnedPass(int& destructionCount)
            : destructionCount_(destructionCount) {}

        ~OwnedPass() override { ++destructionCount_; }

        static PassDescriptor Describe() { return MakeDescriptor("Owned"); }

        void Execute(RenderPassContext&, const RenderFrameData&) override {}

    private:
        int& destructionCount_;
    };
}

GILGAMESH_TEST("Engine.Render.PassRegistrar", "PreservesDescriptorRegistrationOrder")
{
    PassRegistrar registrar;
    FirstPass& first = registrar.Register<FirstPass>();
    SecondPass& second = registrar.Register<SecondPass>();

    GILGAMESH_CHECK_MESSAGE(registrar.Count() == 2,
        "registering two passes must retain both entries");
    GILGAMESH_CHECK_MESSAGE(registrar.GetDescriptor(0).name == "First" &&
        registrar.GetDescriptor(1).name == "Second",
        "pass descriptors must remain in registration order");
    const PassDescriptor& secondDescriptor = registrar.GetDescriptor(1);
    GILGAMESH_CHECK_MESSAGE(
        secondDescriptor.vertexShader == L"EditorGrid" &&
        secondDescriptor.pixelShader == L"EditorGrid" &&
        secondDescriptor.depthStencil.depthTestEnabled &&
        !secondDescriptor.depthStencil.depthWriteEnabled &&
        secondDescriptor.blend.enabled,
        "registration must preserve the pass's shader and pipeline requirements");
    GILGAMESH_CHECK_MESSAGE(&registrar.GetPass(0) == &first &&
        &registrar.GetPass(1) == &second,
        "the registrar must expose the registered concrete pass objects");
}

GILGAMESH_TEST("Engine.Render.PassRegistrar", "OwnsAndDestroysRegisteredPass")
{
    int destructionCount = 0;
    {
        PassRegistrar registrar;
        OwnedPass& pass = registrar.Register<OwnedPass>(destructionCount);
        GILGAMESH_CHECK_MESSAGE(registrar.Count() == 1 &&
            &registrar.GetPass(0) == &pass,
            "registration must retain the constructed pass");
        GILGAMESH_CHECK_MESSAGE(destructionCount == 0,
            "the registered pass must remain alive while the registrar lives");
    }

    GILGAMESH_CHECK_MESSAGE(destructionCount == 1,
        "destroying the registrar must destroy its pass exactly once");
}
