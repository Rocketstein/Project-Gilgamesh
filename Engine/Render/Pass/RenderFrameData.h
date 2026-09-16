#pragma once
#include <unordered_map>

class RenderFrameData
{
public:
    template<class T, class... Args>
    T& Emplace(Args&&... args);

    template<class T>
    const T* TryGet() const noexcept;

private:
    std::unordered_map<std::type_index, std::any> values_;
};