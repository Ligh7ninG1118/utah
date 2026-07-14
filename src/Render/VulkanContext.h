#pragma once
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1

#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc.h>

#include <cstdint>
#include <vector>

#define USE_NSIGHT_AFTERMATH 0

#if USE_NSIGHT_AFTERMATH 
#include "Aftermath/NsightAftermathGpuCrashTracker.h"
#endif

struct GLFWwindow;

struct AllocatedBuffer
{
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
	VmaAllocationInfo info{};
};

struct AllocatedImage
{
	VkImage image = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
};



class VulkanContext
{
public:
	VulkanContext();
	~VulkanContext();

	void Initialize(GLFWwindow* pWindow);

	const vk::raii::Instance& GetInstance() const { return _instance; }
	const vk::raii::PhysicalDevice& GetPhysicalDevice() const { return _physicalDevice; }
	const vk::raii::Device& GetDevice() const { return _device; }
	const vk::raii::SurfaceKHR& GetSurface() const { return _surface; }
	const vk::raii::Queue& GetQueue() const { return _queue; }
	uint32_t GetQueueFamilyIndex() const { return _queueIndex; }

	VmaAllocator GetAllocator() const { return _allocator; }

#if USE_NSIGHT_AFTERMATH
	void WaitForCrashDump();
#endif

private:
	void CreateAllocator();

	void CreateInstance();
	void SetupDebugMessenger();
	void CreateSurface(GLFWwindow* pWindow);
	void PickPhysicalDevice();
	void CreateLogicalDevice();

	static std::vector<const char*> GetRequiredInstanceExtensions();

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
		vk::DebugUtilsMessageTypeFlagsEXT             messageType,
		const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);


	vk::raii::Context                _vkContext;
	vk::raii::Instance               _instance = nullptr;
	vk::raii::DebugUtilsMessengerEXT _debugMessenger = nullptr;
	vk::raii::SurfaceKHR             _surface = nullptr;
	vk::raii::PhysicalDevice         _physicalDevice = nullptr;
	vk::raii::Device                 _device = nullptr;

	uint32_t        _queueIndex = ~0u;
	vk::raii::Queue _queue = nullptr;

	std::vector<const char*> _validationLayers = { "VK_LAYER_KHRONOS_validation" };

	std::vector<const char*> _requiredDeviceExtension = { 
		vk::KHRSwapchainExtensionName, 
		vk::KHRSpirv14ExtensionName,
		vk::KHRSynchronization2ExtensionName,
		vk::KHRCreateRenderpass2ExtensionName,
		vk::KHRMapMemory2ExtensionName,
		vk::KHRMaintenance5ExtensionName,
#if USE_NSIGHT_AFTERMATH
		vk::NVDeviceDiagnosticsConfigExtensionName,
		vk::NVDeviceDiagnosticCheckpointsExtensionName,
#endif
	};

	VmaAllocator _allocator = VK_NULL_HANDLE;

#if USE_NSIGHT_AFTERMATH
	GpuCrashTracker::MarkerMap _markerMap;
	GpuCrashTracker _gpuCrashTrakcer{ _markerMap };
#endif
};

