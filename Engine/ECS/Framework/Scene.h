#pragma once

#include <string>
#include <vector>

#include "Engine/ECS/Entity/Entity.h"

struct SceneHandle
{
    std::uint32_t id = InvalidIndex;
    std::uint32_t generation = 0;

    [[nodiscard]]
	bool IsNull() const noexcept { return id == InvalidIndex; }

private:
    static constexpr std::uint32_t InvalidIndex =
        std::numeric_limits<std::uint32_t>::max();
};

class Scene final
{
public:
    Scene(std::string name);

    std::string_view Name() const noexcept { return name_; }
    bool Contains(Entity entity) const noexcept;

    template<class Fn>
    void ForEachEntity(Fn&& fn) const
    {
        for (Entity entity : entities_)
        {
            fn(entity);
        }
    }

private:
    friend class World;

	std::vector<Entity> entities_;
	std::string name_;
};