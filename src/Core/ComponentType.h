#pragma once
#include <cstdint>


struct ComponentCounter
{
	static inline uint32_t _counter = 0;
};

template <typename T>
struct ComponentType
{
	static uint32_t id()
	{
		static uint32_t val = ComponentCounter::_counter++;
		return val;
	}
};