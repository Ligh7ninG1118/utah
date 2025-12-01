#pragma once
#include "Entity.h"
#include <vector>
#include <cassert>

class IComponentPool
{
public:
	virtual ~IComponentPool() = default;
	virtual void Remove(Entity e) = 0;
	virtual bool Has(Entity e) const = 0;
};

template <typename T>
class ComponentPool : public IComponentPool
{
public:
	void Insert(Entity e, T component)
	{
		if (e >= _componentMap.size())
			_componentMap.resize(e + 1, INVALID_ID);
		
		// if component exists, update it
		if (_componentMap[e] != INVALID_ID)
		{
			_componentData[_componentMap[e]] = component;
			return;
		}

		// add new component
		_componentMap[e] = _componentData.size();
		_entityIndices.push_back(e);
		_componentData.push_back(component);
	}

	void Remove(Entity e) override
	{
		if (e >= _componentMap.size() || _componentMap[e] == INVALID_ID)
			return;

		uint32_t indexToRemove = _componentMap[e];
		uint32_t lastIndex = _componentData.size() - 1;
		Entity lastEntity = _entityIndices[lastIndex];

		// Swap
		_componentData[indexToRemove] = _componentData[lastIndex];
		_entityIndices[indexToRemove] = _entityIndices[indexToRemove];
		_componentMap[lastEntity] = indexToRemove;

		// And pop
		_componentMap[e] = INVALID_ID;
		_componentMap.pop_back();
		_entityIndices.pop_back();
	}

	bool Has(Entity e) const override
	{
		return e < _componentMap.size() && _componentMap[e] != INVALID_ID;
	}

	T& Get(Entity e)
	{
		assert(Has(e));
		return _componentData[_componentMap[e]];
	}

	std::vector<T>& GetPool()
	{
		return _componentData;
	}

	const std::vector<Entity>& GetEntities() const
	{
		return _entityIndices;
	}

private:
	// Tightly packed component data, used for system iteration
	std::vector<T> _componentData;
	// Sparse Map (EntityID -> Component Index)
	std::vector<Entity> _componentMap;
	// Reverse Map (Component index to EntityID)
	std::vector<Entity> _entityIndices;

	static constexpr uint32_t INVALID_ID = 0xFFFFFFFF;
};