#pragma once

#include <any>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>

// Owns typed, frame-local payloads for one synchronous render-pass sequence.
class RenderFrameData
{
public:
    template<class T, class... Args>
    T& Emplace(Args&&... args)
    {
        static_assert(std::is_copy_constructible_v<T>,
            "Render frame payloads must be copy-constructible");

        auto result = values_.insert_or_assign(
            std::type_index(typeid(T)),
            std::any(std::in_place_type<T>, std::forward<Args>(args)...));
        return *std::any_cast<T>(&result.first->second);
    }

    template<class T>
    const T* TryGet() const noexcept
    {
        const auto it = values_.find(std::type_index(typeid(T)));
        return it == values_.end()
            ? nullptr
            : std::any_cast<T>(&it->second);
    }

private:
    // One value per type; a later Emplace of that type replaces the earlier one.
    std::unordered_map<std::type_index, std::any> values_;
};
