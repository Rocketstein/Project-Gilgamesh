#pragma once

#include <string>
#include <vector>

#include "Engine/ECS/Entity/Entity.h"

// Identifies a scene slot and its generation within a World.
struct SceneHandle
{
    std::uint32_t id = InvalidIndex;  // Index into the owning World's scene slots.
    std::uint32_t generation = 0;     // Distinguishes a scene from later occupants of its slot.
    std::uint32_t owningWorldID_ = 0; // World that issued this handle.

    [[nodiscard]]
	bool IsNull() const noexcept { return id == InvalidIndex; }

private:
    static constexpr std::uint32_t InvalidIndex =
        std::numeric_limits<std::uint32_t>::max();
};

// A named group of entities whose membership is managed by World.
class Scene final
{
public:
    Scene(std::string name);

    std::string_view Name() const noexcept { return name_; }
    bool Contains(Entity entity) const noexcept;

    // Visits member handles; the callback must not change scene membership during traversal.
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

	std::vector<Entity> entities_; // Membership handles; component storage belongs to World.
	std::string name_;
};
