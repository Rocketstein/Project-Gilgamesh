#pragma once

class World;
class SystemGroup;

class ISystem
{
public:
	// =============================================================================
	// Lifecycle API
	// =============================================================================
	virtual void OnCreate() {};
	virtual void OnDestroy(World& world) {};

	virtual void OnEnable() {};
	virtual void OnDisable() {};

	// =============================================================================
	// Execution API
	// =============================================================================
	virtual void Update(World& world, float deltaTime) = 0;

	// =============================================================================
	// Scheduling API
	// =============================================================================
	bool IsEnabled() const { return enabled_; }
	void SetEnabled(bool flag) { enabled_ = flag; }

	SystemGroup* GetSystemGroup() const;

private:
	bool enabled_ = false;

};