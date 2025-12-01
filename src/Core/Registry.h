#pragma once
#include "Entity.h"
#include "ComponentType.h"
#include "ComponentPool.h"
#include <vector>
#include <unordered_map>
#include <memory>

class Registry
{
public:
	Entity CreateEntity()
	{
		if (!_freeEntities.empty())
		{
			Entity newEntity = _freeEntities.back();
			_freeEntities.pop_back();
			return newEntity;
		}

		return _entityCounter++;
	}

	void DestroyEntity(Entity e)
	{
		for (auto& pool : _componentPools)
		{
			pool.second->Remove(e);
		}
		_freeEntities.push_back(e);
	}

	template <typename T>
	void AddComponent(Entity e, T component)
	{
		GetPool<T>()->Insert(e, component);
	}

	template <typename T>
	void RemoveComponent(Entity e)
	{
		GetPool<T>()->Remove(e);
	}

	template <typename T>
	T& GetComponent(Entity e)
	{
		return GetPool<T>()->Get(e);
	}

	template <typename T>
	bool HasComponent(Entity e)
	{
		uint32_t cTypeID = ComponentType<T>::id();
		if (!_componentPools.contains(cTypeID))
			return false;

		return GetPool<T>()->Has(e);
	}

	template <typename T>
	ComponentPool<T>* GetPool()
	{
		uint32_t cTypeID = ComponentType<T>::id();
		if (!_componentPools.contains(cTypeID))
			_componentPools[cTypeID] = std::make_unique<ComponentPool<T>>();

		return static_cast<ComponentPool<T>*>(_componentPools[cTypeID].get());
	}


private:
	// Component
	std::unordered_map<uint32_t, std::unique_ptr<IComponentPool>> _componentPools;

	// Entity
	std::vector<Entity> _freeEntities;
	Entity _entityCounter = 0;
};


