#include "ECSCommandBuffer.h"

#include "Engine/ECS/Framework/World.h"

ECSCommandBuffer::ECSCommandBuffer(World& world) : commandBuffer_(16 * 1024), world_(world)
{
}

ECSCommandBuffer::~ECSCommandBuffer() noexcept
{
	// Arena storage alone cannot invoke concrete payload destructors.
	for (const auto& command : commands_)
	{
		command.destroy(command.payload);
	}
}

void ECSCommandBuffer::Clear()
{
    for (auto& command : commands_)
    {
        command.destroy(command.payload);
    }

	commands_.clear();
	commandBuffer_.Reset();
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