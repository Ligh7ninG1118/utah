#pragma once

#include "Core/Registry.h"
#include "Core/ComponentPool.h"

class System
{
public:
	virtual ~System() = default;

	virtual void Init(Registry& registry) { }

	virtual void Update(double deltaTime) { }
private:
};
