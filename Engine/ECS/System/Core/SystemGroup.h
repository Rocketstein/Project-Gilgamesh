#pragma once

#include "ISystem.h"

#include <memory>
#include <vector>

// Exclusively owns systems and executes them in explicit registration order.
class SystemGroup final : public ISystem
{
public:
	// New systems must be uninitialized. Initialized groups create additions immediately.
	// Null input or mutation during execution throws after logging; no reference can be returned.
	ISystem& AddSystem(std::unique_ptr<ISystem> system);
	bool RemoveSystem(ISystem& system, World& world);

	void Update(World& world, float deltaTime) override;

private:
	// Only the world can change phase locks; callbacks cannot unlock another root.
	void SetMutationAllowed(bool allowed) noexcept;

	void OnCreate() override;
	void OnDestroy(World& world) override;
	void OnEnable() override;
	void OnDisable() override;
	[[nodiscard]] bool IsStateChangeAllowed() const noexcept override;

	// Ancestor locks also cover nested groups and their child state transitions.
	[[nodiscard]] bool CanMutate() const noexcept;

private:
	std::vector<std::unique_ptr<ISystem>> systems_; // Stable objects in registration order.
	bool executing_ = false; // Protects iteration and user lifecycle callbacks.
	bool mutationAllowed_ = true; // External world-phase mutation gate.

private:
	friend class ISystem;
	friend class World;
};
