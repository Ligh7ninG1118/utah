#pragma once
#include "Core/Component.h"
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan_raii.hpp>


struct RenderComponent : public Component
{
	glm::vec3 _position = { 0.0f, 0.0f, 0.0f };
	glm::vec3 _rotation = { 0.0f, 0.0f, 0.0f };
	glm::vec3 _scale = { 1.0f, 1.0f, 1.0f };

	std::vector<vk::raii::Buffer> _ubo;
	std::vector<vk::raii::DeviceMemory> _uboMemory;
	std::vector<void*> _uboMapped;

	std::vector<vk::raii::DescriptorSet> _descSets;

	glm::mat4 GetModelMatrix() const
	{
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, _position);
		model = glm::rotate(model, _rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::rotate(model, _rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::rotate(model, _rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
		model = glm::scale(model, _scale);
		return model;
	}
};

