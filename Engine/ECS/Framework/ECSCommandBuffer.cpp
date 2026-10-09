#include "ECSCommandBuffer.h"

ECSCommandBuffer::ECSCommandBuffer() : commandBuffer_(16 * 1024)
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

}

void ECSCommandBuffer::Flush()
{

}