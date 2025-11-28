#pragma once
#include "Core/Component.h"
#include "Core/TransformComponent.h"

#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <vector>



struct RenderComponent : Component
{
	std::vector<vk::raii::Buffer> _ubo;
	std::vector<vk::raii::DeviceMemory> _uboMemory;
	std::vector<void*> _uboMapped;

	std::vector<vk::raii::DescriptorSet> _descSets;
};

