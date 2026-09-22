#pragma once

#include <concepts>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "IRenderPass.h"
#include "PassDescriptor.h"

class ShaderManager;

// Owns pass instances and their descriptions in registration order.
class PassRegistrar final
{
public:
    template<class Pass, class... Args>
        requires std::derived_from<Pass, IRenderPass> &&
            std::constructible_from<Pass, Args...> &&
            requires { { Pass::Describe() } -> std::same_as<PassDescriptor>; }
    Pass& Register(Args&&... args)
    {
        PassDescriptor descriptor = Pass::Describe();
        auto pass = std::make_unique<Pass>(std::forward<Args>(args)...);
        Pass& registered = *pass;
        entries_.push_back({ std::move(descriptor), std::move(pass) });
        return registered;
    }

    bool Prepare(ID3D11Device* device, ShaderManager& shaders);

    [[nodiscard]] std::size_t Count() const noexcept { return entries_.size(); }

    [[nodiscard]] const PassDescriptor& GetDescriptor(std::size_t index) const
    {
        return entries_.at(index).descriptor;
    }

    [[nodiscard]] IRenderPass& GetPass(std::size_t index)
    {
        return *entries_.at(index).pass;
    }

private:
    struct Entry
    {
        PassDescriptor descriptor;
        std::unique_ptr<IRenderPass> pass;
    };

    std::vector<Entry> entries_;
};
