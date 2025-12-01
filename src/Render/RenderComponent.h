#pragma once
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <vector>



struct RenderComponent
{
	std::vector<vk::raii::Buffer> _ubo;
	std::vector<vk::raii::DeviceMemory> _uboMemory;
	std::vector<void*> _uboMapped;

	std::vector<vk::raii::DescriptorSet> _descSets;
};

