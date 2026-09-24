#pragma once

#include <string_view>

#include "GraphPassBuilder.h"

class RenderGraph
{
public:
    RenderGraph() = default;
    ~RenderGraph() = default;

    RenderGraph(const RenderGraph&) = delete;
    RenderGraph& operator=(const RenderGraph&) = delete;

    bool Initialize(ID3D11Device* device);

    void Reset() noexcept;

    template<class SetupFunction, class ExecuteFunction>
    void AddPass(
        std::string_view name,
        SetupFunction&& setup,
        ExecuteFunction&& execute);


    [[nodiscard]]
    bool Compile();

    // Excutes in compilation order
    void Execute(ID3D11DeviceContext* context);

    [[nodiscard]]
    bool IsCompiled() const noexcept;

private:

};