#pragma once

#include "Core/Registry.h"
#include "Core/ComponentPool.h"

class System
{
public:
	virtual ~System() = default;

	virtual void Init(Registry& registry) = 0;

	virtual void Update(double deltaTime) = 0;
private:
};
