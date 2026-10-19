#include "ECSCommandBuffer.h"

#include "Engine/ECS/Framework/World.h"
#include "Engine/ECS/Framework/Scene.h"

ECSCommandBuffer::ECSCommandBuffer(World& world) : commandBuffer_(16 * 1024), world_(world)
{
}

ECSCommandBuffer::~ECSCommandBuffer() noexcept
{
	// Arena storage alone cannot invoke concrete payload destructors.
	for (const auto& command : commands_)
	{
		command.destroy(command.payload, world_);
	}
}

Entity ECSCommandBuffer::CreateEntity()
{
    const Entity entity = world_.ReserveEntity();

    using Payload = ECSAddWorldEntityCommandPayload;
    Payload* payload = nullptr;
    try
    {
        // Keep the concrete payload at a stable, correctly aligned address.
        void* writePtr = commandBuffer_.Allocate(
            sizeof(Payload), alignof(Payload));

        if (!writePtr)
        {
            throw std::bad_alloc{};
        }

        // Construct the payload before publishing its metadata.
        payload = new (writePtr) Payload{ entity };
        commands_.push_back({ &Payload::Execute, &Payload::Destroy, payload });
    }
    catch (...)
    {
        // Destroy a constructed payload, or cancel an unpublished reservation.
        if (payload)
        {
            Payload::Destroy(payload, world_);
        }
        else
        {
            world_.CancelEntityReservation(entity);
        }
        throw;
    }

    return entity;
}

// Record a reserved entity and cancel it if allocation or registration fails.
Entity ECSCommandBuffer::CreateEntity(SceneHandle handle)
{
    const Entity entity = world_.ReserveEntity();

	using Payload = ECSAddEntityCommandPayload;
    Payload* payload = nullptr;
    try
    {
        // Keep the concrete payload at a stable, correctly aligned address.
        void* writePtr = commandBuffer_.Allocate(
            sizeof(Payload), alignof(Payload));

        if (!writePtr)
        {
            throw std::bad_alloc{};
        }

        // Construct the payload before publishing its metadata.
        payload = new (writePtr) Payload{ entity, handle };
        commands_.push_back({ &Payload::Execute, &Payload::Destroy, payload });
    }
    catch (...)
    {
        // Destroy a constructed payload, or cancel an unpublished reservation.
        if (payload)
        {
            Payload::Destroy(payload, world_);
        }
        else
        {
            world_.CancelEntityReservation(entity);
        }
        throw;
    }

    return entity;
}

void ECSCommandBuffer::RemoveEntity(Entity entity)
{
    using Payload = ECSRemoveEntityCommandPayload;
    Payload* payload = nullptr;

    // Keep the concrete payload at a stable, correctly aligned address.
    void* writePtr = commandBuffer_.Allocate(
        sizeof(Payload), alignof(Payload));

    if (!writePtr)
    {
        throw std::bad_alloc{};
    }

    // Construct the payload before publishing its metadata.
    payload = new (writePtr) Payload{ entity };

    // Registration failure must not leave an untracked live payload.
    try
    {
        commands_.push_back({ &Payload::Execute, &Payload::Destroy, payload });
    }
    catch (...)
    {
        Payload::Destroy(payload, world_);
        throw;
    }
}

void ECSCommandBuffer::Clear()
{
    for (auto& command : commands_)
    {
        command.destroy(command.payload, world_);
    }

	commands_.clear();
	commandBuffer_.Reset();
}

// Publish an arena-owned scene operation only after its payload is complete.
void ECSCommandBuffer::RecordSceneCommand(ECSSceneCommandType type, SceneHandle scene)
{
    using Payload = ECSSceneCommandPayload;
    void* storage = commandBuffer_.Allocate(sizeof(Payload), alignof(Payload));
    if (!storage) throw std::bad_alloc{};

    auto* payload = new (storage) Payload{ type, scene };
    try
    {
        commands_.push_back({ &Payload::Execute, &Payload::Destroy, payload });
    }
    catch (...)
    {
        Payload::Destroy(payload, world_);
        throw;
    }
}

void ECSCommandBuffer::Flush()
{
    // Execute commands in recording order.
    try
    {
        for (const auto& header : commands_)
        {
            header.dispatch(header.payload, world_.entityRegistry_, world_);
        }
    }
    catch (...)
    {
        // Discard remaining payloads before propagating the failure.
        Clear();
        throw;
    }

	Clear();
}
