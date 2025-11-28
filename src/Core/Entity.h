#pragma once
#include <cstdint>
#include <vector>

struct Component;

class Entity
{
public:
	Entity(uint32_t entityID);
	~Entity() = default;

	inline uint32_t GetID() const 
	{ 
		return _id; 
	}

private:
	uint32_t _id;
	//std::vector<Component*> _components;
};
