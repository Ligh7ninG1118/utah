#pragma once
#include <cstdint>

class Component;

class Entity
{
  public:
	Entity(uint32_t entityID);
	~Entity();

  private:
	uint32_t _id;
};
