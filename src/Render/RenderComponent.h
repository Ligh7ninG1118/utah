#pragma once
//#include <glm/gtc/matrix_transform.hpp>
//#include <vulkan/vulkan_raii.hpp>
//
//#include <vector>
//
//struct RenderComponent
//{
//	std::vector<vk::raii::Buffer> _ubo;
//	std::vector<vk::raii::DeviceMemory> _uboMemory;
//	std::vector<void*> _uboMapped;
//
//	std::vector<vk::raii::DescriptorSet> _descSets;
//
//	// Prevents copying and enables moving since raii is move only
//	RenderComponent() = default;
//	RenderComponent(RenderComponent&&) noexcept = default;
//	RenderComponent& operator=(RenderComponent&&) noexcept = default;
//
//	RenderComponent(const RenderComponent&) = delete;
//	RenderComponent& operator=(const RenderComponent&) = delete;
//};

#include <cstdint>

using MeshHandle = uint32_t;
using MaterialHandle = uint32_t;
constexpr uint32_t INVALID_HANDLE = 0xFFFFFFFF;


struct RenderComponent
{
	MeshHandle _mesh = 0;
	MaterialHandle _material = 0;
};