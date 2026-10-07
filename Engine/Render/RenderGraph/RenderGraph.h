#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <d3d11.h>
#include <wrl/client.h>

#include "GraphPassBuilder.h"

class RenderGraph final
{
public:
    RenderGraph() noexcept;
    ~RenderGraph() = default;

    RenderGraph(const RenderGraph&) = delete;
    RenderGraph& operator=(const RenderGraph&) = delete;
    RenderGraph(RenderGraph&&) = delete;
    RenderGraph& operator=(RenderGraph&&) = delete;

    // Registers a graph-owned texture that will be allocated by a later stage.
    [[nodiscard]]
    RenderGraphTextureHandle CreateTexture(
        std::string_view name,
        const RenderGraphTextureDesc& description);

    // Registers caller-owned native objects that must outlive graph execution.
    [[nodiscard]]
    RenderGraphTextureHandle ImportTexture(
        std::string_view name,
        const RenderGraphTextureDesc& description,
        const ImportedRenderGraphTexture& importedTexture);

    // Discards this frame's declarations and invalidates their handles.
    void Reset() noexcept;

    template<class SetupFunction, class ExecuteFunction>
    void AddPass(
        std::string_view name,
        SetupFunction&& setup,
        ExecuteFunction&& execute);

    // Compiles declarations and creates the graph-owned GPU resources.
    [[nodiscard]]
    bool Compile(ID3D11Device* device);

    // Binds declared resources and invokes each pass in compiled order.
    // Callbacks must not bind graph textures through undeclared stages or slots.
    [[nodiscard]]
    bool Execute(ID3D11DeviceContext* context);

    [[nodiscard]]
    bool IsCompiled() const noexcept
    {
        return isCompiled_;
    }

    // Indices refer to passes in their original AddPass registration order.
    [[nodiscard]]
    std::span<const std::size_t> GetCompiledPassOrder() const noexcept
    {
        return compiledPassOrder_;
    }

    // Checks ownership, generation, and resource index for a texture handle.
    [[nodiscard]]
    bool IsValid(RenderGraphTextureHandle handle) const noexcept;

    [[nodiscard]]
    std::size_t GetTextureCount() const noexcept
    {
        return textures_.size();
    }

    [[nodiscard]]
    std::size_t GetPassCount() const noexcept
    {
        return passes_.size();
    }

private:
    // Describes the native texture views required by compiled pass bindings.
    enum TextureViewRequirement : std::uint8_t
    {
        None                 = 0,
        RenderTarget         = 1 << 0,
        WritableDepthStencil = 1 << 1,
        ReadOnlyDepthStencil = 1 << 2,
        ShaderResource       = 1 << 3
    };

    struct TextureRecord
    {
        std::string name;
        RenderGraphTextureDesc description;
        std::optional<ImportedRenderGraphTexture> importedTexture;
        std::uint8_t compiledViewRequirements =
            TextureViewRequirement::None;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> ownedTexture;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> ownedRTV;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> ownedWritableDSV;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> ownedReadOnlyDSV;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ownedSRV;
    };

    // Owns everything declared by one AddPass call after setup returns.
    struct PassDeclaration
    {
        std::string name;
        std::vector<GraphPassBuilder::TextureAccess> textureAccesses;
        std::function<void(ID3D11DeviceContext&)> execute;
    };

    // Summarizes one pass's dependency-relevant use of one texture.
    struct TextureUse
    {
        std::size_t passIndex = 0;
        bool reads = false;
        bool writes = false;
    };

    using TextureUsesByTexture =
        std::vector<std::vector<TextureUse>>;

private:
    // Validates logical and imported texture declarations.
    [[nodiscard]]
    bool ValidateResourceDeclarations() const;

    // Validates pass bindings and records their derived texture state.
    [[nodiscard]]
    bool ValidateBindingDeclarations(
        TextureUsesByTexture& usesByTexture);

    // Discards every result derived from the current declarations.
    void InvalidateCompiledState() noexcept;

    // Creates GPU resources from the textures after compilation.
    bool CreateResources(ID3D11Device* device);

    // Releases every GPU resource owned by this graph.
    void ReleaseResources() noexcept;

    // Resolves the native views selected during resource realization.
    [[nodiscard]]
    ID3D11RenderTargetView* ResolveRenderTargetView(
        const TextureRecord& texture) const noexcept;

    [[nodiscard]]
    ID3D11DepthStencilView* ResolveDepthStencilView(
        const TextureRecord& texture,
        RenderGraphDepthAccess access) const noexcept;

    [[nodiscard]]
    ID3D11ShaderResourceView* ResolveShaderResourceView(
        const TextureRecord& texture) const noexcept;

    // Applies the attachment state declared by one compiled pass.
    void BindPassAttachments(
        ID3D11DeviceContext* context,
        const PassDeclaration& pass) const noexcept;

    // Executes attachment clear operations before the pass callback.
    void ClearPassAttachments(
        ID3D11DeviceContext* context,
        const PassDeclaration& pass) const noexcept;

    // Binds stage-specific shader reads after attachment clears.
    void BindPassShaderResources(
        ID3D11DeviceContext* context,
        const PassDeclaration& pass) const noexcept;

    // Ends the graph-managed resource usages of one pass.
    void UnbindPassResources(
        ID3D11DeviceContext* context,
        const PassDeclaration& pass) const noexcept;

private:
    Microsoft::WRL::ComPtr<ID3D11Device> compiledDevice_;
    std::vector<TextureRecord> textures_;
    std::vector<PassDeclaration> passes_;
    std::vector<std::size_t> compiledPassOrder_;

    std::uint64_t ownerId_ = 0;
    std::uint64_t generation_ = 1;
    bool isAddingPass_ = false;
    bool isExecuting_ = false;
    bool isCompiled_ = false;
};

template<class SetupFunction, class ExecuteFunction>
void RenderGraph::AddPass(
    std::string_view name,
    SetupFunction&& setup,
    ExecuteFunction&& execute)
{
    using StoredExecuteFunction = std::decay_t<ExecuteFunction>;

    static_assert(
        std::is_invocable_v<SetupFunction&&, GraphPassBuilder&>,
        "RenderGraph pass setup must accept GraphPassBuilder&");
    static_assert(
        std::is_invocable_r_v<
            void,
            StoredExecuteFunction&,
            ID3D11DeviceContext&>,
        "RenderGraph pass execute must accept ID3D11DeviceContext&");
    static_assert(
        std::is_copy_constructible_v<StoredExecuteFunction>,
        "RenderGraph pass execute must be copy constructible in C++20");

    if (isAddingPass_)
    {
        throw std::logic_error(
            "RenderGraph::AddPass cannot be nested inside pass setup");
    }

    if (isExecuting_)
    {
        throw std::logic_error(
            "RenderGraph::AddPass cannot run during graph execution");
    }

    PassDeclaration pass{
        std::string(name),
        {},
        std::function<void(ID3D11DeviceContext&)>(
            std::forward<ExecuteFunction>(execute))
    };

    GraphPassBuilder builder;
    isAddingPass_ = true;

    // Setup writes only to temporary storage, so failure leaves the graph intact.
    try
    {
        std::invoke(
            std::forward<SetupFunction>(setup),
            builder);
    }
    catch (...)
    {
        isAddingPass_ = false;
        throw;
    }

    isAddingPass_ = false;
    pass.textureAccesses = std::move(builder.textureAccesses_);
    passes_.push_back(std::move(pass));

    // A newly declared pass may change every dependency in the old plan.
    InvalidateCompiledState();
}
