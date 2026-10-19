#pragma once

class World;
class SystemGroup;

// Provides explicit lifecycle transitions and parent-aware scheduling state.
class ISystem
{
public:
	virtual ~ISystem() = default;

	ISystem() = default;

	ISystem(const ISystem&) = delete;
	ISystem& operator=(const ISystem&) = delete;
	ISystem(ISystem&&) = delete;
	ISystem& operator=(ISystem&&) = delete;

	// Root systems initialize directly; attached systems are managed by their group.
	void Initialize();
	void Shutdown(World& world) noexcept;

	// Requested enablement survives temporary deactivation by a parent group.
	void SetEnabled(bool enabled);
	[[nodiscard]] bool IsEnabled() const noexcept { return enabled_; }
	[[nodiscard]] bool IsActive() const noexcept { return active_; }
	[[nodiscard]] bool IsInitialized() const noexcept { return initialized_; }
	[[nodiscard]] SystemGroup* GetSystemGroup() const noexcept { return systemGroup_; }

	// Lifecycle hooks are invoked by Initialize, Shutdown, and enable transitions.
	virtual void OnCreate() {}
	virtual void OnDestroy(World& world) {}
	virtual void OnEnable() {}
	virtual void OnDisable() {}

	virtual void Update(World& world, float deltaTime) = 0;

protected:
	// Groups extend this check with their own traversal and world-phase locks.
	[[nodiscard]] virtual bool IsStateChangeAllowed() const noexcept;

private:
	// Owner-only entry points allow propagation while external mutation is locked.
	void InitializeOwned();
	void ShutdownOwned(World& world) noexcept;
	void RefreshActivation() noexcept;

private:
	SystemGroup* systemGroup_ = nullptr; // Borrowed owner; null for a root system.
	bool enabled_ = false; // Caller-requested state, independent of parent enablement.
	bool initialized_ = false; // OnCreate has completed for the current lifetime.
	bool active_ = false; // Effective state after initialization and parent gating.
	bool transitioning_ = false; // Rejects reentrant lifecycle or enable transitions.

private:
	friend class SystemGroup;
	friend class World;
};
