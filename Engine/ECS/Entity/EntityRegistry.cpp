#include "EntityRegistry.h"

Entity EntityRegistry::Create()
{
	if (!freeIndices_.empty())
	{
		const std::uint32_t index = freeIndices_.back(); freeIndices_.pop_back();

		EntitySlot& slot = entities_[index];
	}
}