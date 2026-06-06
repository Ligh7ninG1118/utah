#include "VulkanContext.h"

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS  0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <iostream>
#include <ranges>
#include <set>
#include <stdexcept>
#include <string>


#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif


VulkanContext::VulkanContext()
{
}

VulkanContext::~VulkanContext()
{
	if (_allocator != VK_NULL_HANDLE)
	{
		vmaDestroyAllocator(_allocator);
		_allocator = VK_NULL_HANDLE;
	}
}

void VulkanContext::Initialize(GLFWwindow* pWindow)
{
#if USE_NSIGHT_AFTERMATH
	_gpuCrashTrakcer.Initialize(false);
#endif
	CreateInstance();
	SetupDebugMessenger();
	CreateSurface(pWindow);
	PickPhysicalDevice();
	CreateLogicalDevice();
	
	CreateAllocator();
}

void VulkanContext::CreateAllocator()
{
	VmaVulkanFunctions vkFunctions{};
	vkFunctions.vkGetInstanceProcAddr = _vkContext.getDispatcher()->vkGetInstanceProcAddr;
	vkFunctions.vkGetDeviceProcAddr = _device.getDispatcher()->vkGetDeviceProcAddr;

	VmaAllocatorCreateInfo info = {};
	info.physicalDevice = *_physicalDevice;
	info.device = *_device;
	info.instance = *_instance;
	info.pVulkanFunctions = &vkFunctions;
	info.vulkanApiVersion = VK_API_VERSION_1_3;
	if (vmaCreateAllocator(&info, &_allocator) != VK_SUCCESS)
		throw std::runtime_error("Failed to create VMA allocator");
}

void VulkanContext::CreateInstance()
{
	constexpr vk::ApplicationInfo appInfo{ .pApplicationName = "Hello Triangle",
										  .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
										  .pEngineName = "No Engine",
										  .engineVersion = VK_MAKE_VERSION(1, 0, 0),
										  .apiVersion = vk::ApiVersion13 };
	// Can use appInfo.pNext to point to extension information

	std::vector<char const*> requiredLayers;
	if (enableValidationLayers)
	{
		requiredLayers.assign(_validationLayers.begin(), _validationLayers.end());
	}

	auto layerProperties = _vkContext.enumerateInstanceLayerProperties();
	for (auto const& requiredLayer : requiredLayers)
	{
		if (std::ranges::none_of(layerProperties, [requiredLayer](auto const& layerProperty) {
			return strcmp(layerProperty.layerName, requiredLayer) == 0;
			}))
		{
			throw std::runtime_error("Required layer not supported: " + std::string(requiredLayer));
		}
	}

	auto requiredExtensions = GetRequiredInstanceExtensions();
	auto extensionProperties = _vkContext.enumerateInstanceExtensionProperties();
	for (auto const& requiredExtension : requiredExtensions)
	{
		if (std::ranges::none_of(extensionProperties, [requiredExtension](auto const& extensionProperty) {
			return strcmp(extensionProperty.extensionName, requiredExtension) == 0;
			}))
		{
			throw std::runtime_error("Required extension not supported: " + std::string(requiredExtension));
		}
	}

	vk::InstanceCreateInfo createInfo{ .pApplicationInfo = &appInfo,
									  .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
									  .ppEnabledLayerNames = requiredLayers.data(),
									  .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
									  .ppEnabledExtensionNames = requiredExtensions.data() };

	_instance = vk::raii::Instance(_vkContext, createInfo);
}

void VulkanContext::SetupDebugMessenger()
{
	if (!enableValidationLayers)
		return;

	vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
	vk::DebugUtilsMessageTypeFlagsEXT     messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
		vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
		vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
	vk::DebugUtilsMessengerCreateInfoEXT  debugUtilsMessengerCreateInfoEXT{
		 .messageSeverity = severityFlags, .messageType = messageTypeFlags, .pfnUserCallback = DebugCallback };

	_debugMessenger = _instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
}

void VulkanContext::CreateSurface(GLFWwindow* pWindow)
{
	VkSurfaceKHR tempSurface;
	if (glfwCreateWindowSurface(*_instance, pWindow, nullptr, &tempSurface) != 0)
	{
		throw std::runtime_error("Failed to create window surface!");
	}
	_surface = vk::raii::SurfaceKHR(_instance, tempSurface);
}

void VulkanContext::PickPhysicalDevice()
{
	std::vector<vk::raii::PhysicalDevice> devices = _instance.enumeratePhysicalDevices();
	const auto devIter = std::ranges::find_if(devices, [&](auto const& device) {
		// Check if the device supports the Vulkan 1.3 API version
		bool supportsVulkan1_3 = device.getProperties().apiVersion >= VK_API_VERSION_1_3;

		// Check if any of the queue families support graphics operations
		auto queueFamilies = device.getQueueFamilyProperties();
		bool supportsGraphics = std::ranges::any_of(
			queueFamilies, [](auto const& qfp) { return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics); });

		// Check if all required device extensions are available
		auto availableDeviceExtensions = device.enumerateDeviceExtensionProperties();
		bool supportsAllRequiredExtensions = std::ranges::all_of(
			_requiredDeviceExtension, [&availableDeviceExtensions](auto const& requiredDeviceExtension) {
				return std::ranges::any_of(
					availableDeviceExtensions, [requiredDeviceExtension](auto const& availableDeviceExtension) {
						return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0;
					});
			});

		auto features = device.template getFeatures2<
			vk::PhysicalDeviceFeatures2,
			vk::PhysicalDeviceVulkan13Features,
			vk::PhysicalDeviceVulkan12Features,
			vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
		bool supportsRequiredFeatures =
			features.template get<vk::PhysicalDeviceFeatures2>().features.fillModeNonSolid &&
			features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
			features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
			features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
			features.template get<vk::PhysicalDeviceVulkan12Features>().runtimeDescriptorArray &&
			features.template get<vk::PhysicalDeviceVulkan12Features>().descriptorBindingPartiallyBound &&
			features.template get<vk::PhysicalDeviceVulkan12Features>().shaderSampledImageArrayNonUniformIndexing &&
			features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

		return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
		});
	if (devIter != devices.end())
	{
		_physicalDevice = *devIter;
	}
	else
	{
		throw std::runtime_error("failed to find a suitable GPU!");
	}
}

void VulkanContext::CreateLogicalDevice()
{
	std::vector<vk::QueueFamilyProperties> queueFamilyProperties = _physicalDevice.getQueueFamilyProperties();

	// get the first index into queueFamilyProperties which supports both graphics and present
	for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
	{
		if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
			_physicalDevice.getSurfaceSupportKHR(qfpIndex, *_surface))
		{
			// found a queue family that supports both graphics and present
			_queueIndex = qfpIndex;
			break;
		}
	}
	if (_queueIndex == ~0)
	{
		throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
	}

	// query for Vulkan 1.3 features
	vk::StructureChain<
		vk::PhysicalDeviceFeatures2,
		vk::PhysicalDeviceVulkan13Features,
		vk::PhysicalDeviceVulkan12Features,
		vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
		featureChain = {
			{.features = {.fillModeNonSolid = true, .samplerAnisotropy = true}},	// vk::PhysicalDeviceFeatures2
			{.synchronization2 = true, .dynamicRendering = true},					// vk::PhysicalDeviceVulkan13Features
			{.descriptorIndexing = true,											// vk::PhysicalDeviceVulkan12Features
			  .shaderSampledImageArrayNonUniformIndexing = true,
			  .descriptorBindingSampledImageUpdateAfterBind = true,
			  .descriptorBindingPartiallyBound = true,
			  .runtimeDescriptorArray = true,
			  .bufferDeviceAddress = false},
			{.extendedDynamicState = true}                               // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
	};

	// create a Device
	// Priority between 0.0f to 1.0f
	float queuePriority = 0.0f;
	vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
		.queueFamilyIndex = _queueIndex,
		.queueCount = 1,
		.pQueuePriorities = &queuePriority };
	vk::DeviceCreateInfo deviceCreateInfo{
		.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &deviceQueueCreateInfo,
		.enabledExtensionCount = static_cast<uint32_t>(_requiredDeviceExtension.size()),
		.ppEnabledExtensionNames = _requiredDeviceExtension.data() };

#if USE_NSIGHT_AFTERMATH
	vk::DeviceDiagnosticsConfigCreateInfoNV diagnosticsConfig{
	.flags = vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableResourceTracking
		   | vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableAutomaticCheckpoints
		   | vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableShaderDebugInfo
		   | vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableShaderErrorReporting
	};
	diagnosticsConfig.pNext = const_cast<void*>(deviceCreateInfo.pNext);
	deviceCreateInfo.pNext = &diagnosticsConfig;
#endif

	_device = vk::raii::Device(_physicalDevice, deviceCreateInfo);
	_queue = vk::raii::Queue(_device, _queueIndex, 0);
}

// Grab extensions for GLFW/Windows
[[nodiscard]] std::vector<const char*> VulkanContext::GetRequiredInstanceExtensions()
{
	uint32_t glfwExtensionCount = 0;
	auto     glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (enableValidationLayers)
	{
		extensions.push_back(vk::EXTDebugUtilsExtensionName);
	}

	return extensions;
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL VulkanContext::DebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT      severity,
	vk::DebugUtilsMessageTypeFlagsEXT             type,
	const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError ||
		severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
	{
		std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
	}
	return vk::False;
}

#if USE_NSIGHT_AFTERMATH
void VulkanContext::WaitForCrashDump()
{
	using namespace std::chrono;
	auto tStart = steady_clock::now();
	GFSDK_Aftermath_CrashDump_Status status = GFSDK_Aftermath_CrashDump_Status_Unknown;
	AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_GetCrashDumpStatus(&status));
	while (status != GFSDK_Aftermath_CrashDump_Status_CollectingDataFailed &&
		status != GFSDK_Aftermath_CrashDump_Status_Finished &&
		duration_cast<seconds>(steady_clock::now() - tStart).count() < 5)
	{
		std::this_thread::sleep_for(milliseconds(50));
		AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_GetCrashDumpStatus(&status));
	}
}
#endif