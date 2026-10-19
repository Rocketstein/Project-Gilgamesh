#include "ISystem.h"

#include "SystemGroup.h"
#include "Engine/Core/Logging/Logger.h"

// Attached systems must enter their lifecycle through the owning group.
void ISystem::Initialize()
{
	if (systemGroup_ != nullptr || transitioning_ || !IsStateChangeAllowed())
	{
		GILGAMESH_LOG(Core, Warning, "Cannot initialize an attached or currently locked system directly.");
		return;
	}
	InitializeOwned();
}

// Explicit shutdown keeps the world alive until every owned lifecycle hook finishes.
void ISystem::Shutdown(World& world) noexcept
{
	if (systemGroup_ != nullptr || transitioning_ || !IsStateChangeAllowed())
	{
		GILGAMESH_LOG(Core, Warning, "Cannot shut down an attached or currently locked system directly.");
		return;
	}
	ShutdownOwned(world);
}

// Scheduling changes are accepted only outside owner traversal and lifecycle hooks.
void ISystem::SetEnabled(bool enabled)
{
	if (transitioning_ || !IsStateChangeAllowed())
	{
		GILGAMESH_LOG(Core, Warning, "Cannot change system enablement during system execution or lifecycle transitions.");
		return;
	}

	enabled_ = enabled;
	RefreshActivation();
}

// A child cannot change scheduling state while its owner or an ancestor is traversed.
bool ISystem::IsStateChangeAllowed() const noexcept
{
	return systemGroup_ == nullptr || systemGroup_->CanMutate();
}

// Creation is idempotent; a failed creation may be retried after reporting failure.
void ISystem::InitializeOwned()
{
	if (initialized_ || transitioning_)
		return;

	transitioning_ = true;
	try
	{
		OnCreate();
	}
	catch (...)
	{
		transitioning_ = false;
		GILGAMESH_LOG(Core, Error, "System creation failed.");
		throw;
	}

	initialized_ = true;
	transitioning_ = false;
	RefreshActivation();
}

// Teardown continues even if user hooks fail, so remaining owned systems are released.
void ISystem::ShutdownOwned(World& world) noexcept
{
	if (!initialized_ || transitioning_)
		return;

	// Deactivate before destruction; parent shutdown may already have done this.
	enabled_ = false;
	RefreshActivation();
	transitioning_ = true;
	try
	{
		OnDestroy(world);
	}
	catch (...)
	{
		GILGAMESH_LOG(Core, Error, "System destruction hook failed; continuing teardown.");
	}

	initialized_ = false;
	transitioning_ = false;
}

// Parent activation changes effective state without changing a child's requested flag.
void ISystem::RefreshActivation() noexcept
{
	const bool shouldBeActive = initialized_ && enabled_ &&
		(systemGroup_ == nullptr || systemGroup_->IsActive());
	if (active_ == shouldBeActive || transitioning_)
		return;

	active_ = shouldBeActive;
	transitioning_ = true;
	try
	{
		if (active_)
			OnEnable();
		else
			OnDisable();
	}
	catch (...)
	{
		GILGAMESH_LOG(Core, Error, "System activation hook failed.");
	}
	transitioning_ = false;
}

