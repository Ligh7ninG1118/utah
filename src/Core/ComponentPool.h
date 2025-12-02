#pragma once
#include "Entity.h"
#include <vector>
#include <cassert>
#include <utility>


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
	template <typename... Args>
	void Emplace(Entity e, Args&&...args)
	{
		if (e >= _componentMap.size())
			_componentMap.resize(e + 1, INVALID_ID);

		// if component exists, update it
		if (_componentMap[e] != INVALID_ID)
		{
			_componentData[_componentMap[e]] = T(std::forward<Args>(args)...);
			return;
		}

		// add new component
		_componentMap[e] = static_cast<uint32_t>(_componentData.size());
		_entityIndices.push_back(e);
		_componentData.emplace_back(std::forward<Args>(args)...);
	}

	void Insert(Entity e, T component)
	{
		Emplace(e, std::move(component));
	}

	void Remove(Entity e) override
	{
		if (e >= _componentMap.size() || _componentMap[e] == INVALID_ID)
			return;

		const uint32_t indexToRemove = _componentMap[e];
		const uint32_t lastIndex = static_cast<uint32_t>(_componentData.size() - 1);
		const Entity lastEntity = _entityIndices[lastIndex];

		if (indexToRemove != lastIndex)
		{
			// Swap
			_componentData[indexToRemove] = std::move(_componentData[lastIndex]);
			_entityIndices[indexToRemove] = lastEntity; // or _entityIndices[indexToRemove]? Double check this
			_componentMap[lastEntity] = indexToRemove;
		}

		

		// And pop
		_componentMap[e] = INVALID_ID;
		_componentData.pop_back();
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