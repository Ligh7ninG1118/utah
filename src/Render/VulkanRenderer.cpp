#include "VulkanRenderer.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <unordered_map>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

constexpr uint32_t WINDOW_WIDTH = 1920;
constexpr uint32_t WINDOW_HEIGHT = 1080;

const std::string MODEL_PATH = "models/viking_room.obj";
const std::string TEXTURE_PATH = "models/viking_room.png";

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif


VulkanRenderer::VulkanRenderer()
	: _ctxRef(UtahCtx::Get())
{
}

VulkanRenderer::~VulkanRenderer()
{
}

void VulkanRenderer::Initialize()
{
	CreateInstance();
	SetupDebugMessenger();
	RegisterResizeCallback();

	CreateSurface();
	PickPhysicalDevice();
	_msaaSamples = GetMaxUsableSampleCount();
	CreateLogicalDevice();
	CreateSwapChain();
	CreateImageViews();

	CreateDescriptorSetLayout();
	CreateGraphicsPipeline();

	CreateCommandPool();
	CreateColorResources();
	CreateDepthResources();

	CreateTextureImage();
	CreateTextureImageView();
	CreateTextureSampler();

	LoadModel();
	CreateVertexBuffer();
	CreateIndexBuffer();
	CreateUniformBuffers();
	CreateDescriptorPool();
	CreateDescriptorSets();
	CreateCommandBuffers();

	CreateSyncObjects();
}

void VulkanRenderer::DrawFrame()
{
	// Wait previous frame to finish (block execution)
	while (vk::Result::eTimeout == _device.waitForFences(*_inFlightFences[_currentFrame], vk::True, UINT64_MAX))
		;

	// Acquire image from the swap chain
	auto [result, imageIndex] =
		_swapChain.acquireNextImage(UINT64_MAX, *_presentCompleteSemaphores[_semaphoreIndex], nullptr);

	if (result == vk::Result::eErrorOutOfDateKHR)
	{
		RecreateSwapChain();
		return;
	}
	if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
	{
		throw std::runtime_error("Failed to acquire swapchain image");
	}
	UpdateUniformBuffer(_currentFrame);

	_device.resetFences(*_inFlightFences[_currentFrame]);

	_commandBuffers[_currentFrame].reset();
	RecordCommandBuffer(imageIndex);

	vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
	const vk::SubmitInfo   submitInfo{ .waitSemaphoreCount = 1,
									  .pWaitSemaphores = &*_presentCompleteSemaphores[_semaphoreIndex],
									  .pWaitDstStageMask = &waitDestinationStageMask,
									  .commandBufferCount = 1,
									  .pCommandBuffers = &*_commandBuffers[_currentFrame],
									  .signalSemaphoreCount = 1,
									  .pSignalSemaphores = &*_renderFinishedSemaphores[imageIndex] };
	_queue.submit(submitInfo, *_inFlightFences[_currentFrame]);

	try
	{
		const vk::PresentInfoKHR presentInfo{ .waitSemaphoreCount = 1,
											 .pWaitSemaphores = &*_renderFinishedSemaphores[imageIndex],
											 .swapchainCount = 1,
											 .pSwapchains = &*_swapChain,
											 .pImageIndices = &imageIndex };

		result = _queue.presentKHR(presentInfo);
		if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || _framebufferResized)
		{
			_framebufferResized = false;
			RecreateSwapChain();
		}
		else if (result != vk::Result::eSuccess)
		{
			throw std::runtime_error("Failed to present swap chain image");
		}
	}
	catch (const vk::SystemError& e)
	{
		if (e.code().value() == static_cast<int>(vk::Result::eErrorOutOfDateKHR))
		{
			RecreateSwapChain();
			return;
		}
		else
		{
			throw;
		}
	}
	_semaphoreIndex = (_semaphoreIndex + 1) % _presentCompleteSemaphores.size();
	_currentFrame = (_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::WaitForIdle()
{
	_device.waitIdle();
}

void VulkanRenderer::UpdateUniformBuffer(uint32_t currentImage)
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto  currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	UniformBufferObject ubo{};
	ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	//ubo.model = glm::mat4(1.0f);
	ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.proj =
		glm::perspective(glm::radians(45.0f), _swapChainExtent.width / (float)_swapChainExtent.height, 0.1f, 10.0f);
	ubo.proj[1][1] *= -1;

	memcpy(_uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void VulkanRenderer::RegisterResizeCallback()
{
	glfwSetFramebufferSizeCallback(_ctxRef.GetContextWindow(), FramebufferResizeCallback);
}

void VulkanRenderer::FramebufferResizeCallback(GLFWwindow* window, int width, int height)
{
	auto pAppCtx = static_cast<VulkanRenderer*>(glfwGetWindowUserPointer(window));
	pAppCtx->_framebufferResized = true;
}

void VulkanRenderer::CreateInstance()
{
	constexpr vk::ApplicationInfo appInfo{ .pApplicationName = "Hello Triangle",
										  .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
										  .pEngineName = "No Engine",
										  .engineVersion = VK_MAKE_VERSION(1, 0, 0),
										  .apiVersion = vk::ApiVersion14 };
	// Can use appInfo.pNext to point to extension information

	std::vector<char const*> requiredLayers;
	if (enableValidationLayers)
	{
		requiredLayers.assign(validationLayers.begin(), validationLayers.end());
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

	auto requiredExtensions = GetRequiredExtensions();
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

// Grab extensions for GLFW/Windows
[[nodiscard]] std::vector<const char*> VulkanRenderer::GetRequiredExtensions() const
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

void VulkanRenderer::SetupDebugMessenger()
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

VKAPI_ATTR vk::Bool32 VKAPI_CALL VulkanRenderer::DebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT      severity,
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

void VulkanRenderer::CreateSurface()
{
	VkSurfaceKHR tempSurface;
	if (glfwCreateWindowSurface(*_instance, _ctxRef.GetContextWindow(), nullptr, &tempSurface) != 0)
	{
		throw std::runtime_error("Failed to create window surface!");
	}
	_surface = vk::raii::SurfaceKHR(_instance, tempSurface);
}

void VulkanRenderer::PickPhysicalDevice()
{
	std::vector<vk::raii::PhysicalDevice> devices = _instance.enumeratePhysicalDevices();
	const auto                            devIter = std::ranges::find_if(devices, [&](auto const& device) {
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

		auto features = device.template getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features,
			vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
		bool supportsRequiredFeatures =
			features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
			features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
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

void VulkanRenderer::CreateLogicalDevice()
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
	vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features,
		vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
		featureChain = {
			{.features = {.samplerAnisotropy = true}},                   // vk::PhysicalDeviceFeatures2
			{.synchronization2 = true, .dynamicRendering = true},        // vk::PhysicalDeviceVulkan13Features
			{.extendedDynamicState = true}                               // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
	};

	// create a Device
	float                     queuePriority = 0.0f;
	vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
		.queueFamilyIndex = _queueIndex, .queueCount = 1, .pQueuePriorities = &queuePriority };
	vk::DeviceCreateInfo deviceCreateInfo{ .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
										  .queueCreateInfoCount = 1,
										  .pQueueCreateInfos = &deviceQueueCreateInfo,
										  .enabledExtensionCount =
											  static_cast<uint32_t>(_requiredDeviceExtension.size()),
										  .ppEnabledExtensionNames = _requiredDeviceExtension.data() };

	_device = vk::raii::Device(_physicalDevice, deviceCreateInfo);
	_queue = vk::raii::Queue(_device, _queueIndex, 0);
}

vk::SurfaceFormatKHR VulkanRenderer::ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
{
	assert(!availableFormats.empty());

	const auto formatIt = std::ranges::find_if(availableFormats, [](const auto& format) {
		return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
		});

	return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
}

vk::PresentModeKHR VulkanRenderer::ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
{
	assert(std::ranges::any_of(availablePresentModes,
		[](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }));

	return std::ranges::any_of(availablePresentModes,
		[](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; }) ?
		vk::PresentModeKHR::eMailbox :
		vk::PresentModeKHR::eFifo;
}

uint32_t VulkanRenderer::ChooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const& surfaceCapabilities)
{
	auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
	if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount))
	{
		minImageCount = surfaceCapabilities.maxImageCount;
	}
	return minImageCount;
}

[[nodiscard]] vk::Extent2D VulkanRenderer::ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) const
{
	if (capabilities.currentExtent.width != 0xFFFFFFFF)
		return capabilities.currentExtent;

	int width, height;
	glfwGetFramebufferSize(_ctxRef.GetContextWindow(), &width, &height);

	return { std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
			std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height) };
}

void VulkanRenderer::CreateSwapChain()
{
	auto surfaceCapabilities = _physicalDevice.getSurfaceCapabilitiesKHR(*_surface);
	_swapChainExtent = ChooseSwapExtent(surfaceCapabilities);
	_swapChainSurfaceFormat = ChooseSwapSurfaceFormat(_physicalDevice.getSurfaceFormatsKHR(*_surface));

	vk::SwapchainCreateInfoKHR swapChainCreateInfo{
		.surface = *_surface,
		.minImageCount = ChooseSwapMinImageCount(surfaceCapabilities),
		// Format: Color channels and types
		.imageFormat = _swapChainSurfaceFormat.format,
		// Color space: SRGB, etc.
		.imageColorSpace = _swapChainSurfaceFormat.colorSpace,
		// Swap Extent: Resolution
		.imageExtent = _swapChainExtent,
		.imageArrayLayers = 1,
		.imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
		.imageSharingMode = vk::SharingMode::eExclusive,
		.preTransform = surfaceCapabilities.currentTransform,
		.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
		// Present Mode: Conditions for showing images (Immediate, FIFO, FIFO Relaxed, Mailbox)
		.presentMode = ChooseSwapPresentMode(_physicalDevice.getSurfacePresentModesKHR(*_surface)),
		.clipped = true };

	_swapChain = vk::raii::SwapchainKHR(_device, swapChainCreateInfo);
	_swapChainImages = _swapChain.getImages();
}

void VulkanRenderer::CreateImageViews()
{
	assert(_swapChainImageViews.empty());

	vk::ImageViewCreateInfo imageViewCreateInfo{ .viewType = vk::ImageViewType::e2D,
												.format = _swapChainSurfaceFormat.format,
												.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1} };

	for (auto image : _swapChainImages)
	{
		imageViewCreateInfo.image = image;
		_swapChainImageViews.emplace_back(_device, imageViewCreateInfo);
	}
}

void VulkanRenderer::CleanupSwapChain()
{
	_swapChainImageViews.clear();
	_swapChain = nullptr;
}

void VulkanRenderer::RecreateSwapChain()
{
	int width = 0, height = 0;
	glfwGetFramebufferSize(_ctxRef.GetContextWindow(), &width, &height);
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(_ctxRef.GetContextWindow(), &width, &height);
		glfwWaitEvents();
	}

	_device.waitIdle();

	CleanupSwapChain();

	CreateSwapChain();
	CreateImageViews();
	CreateColorResources();
	CreateDepthResources();
}

void VulkanRenderer::CreateDescriptorSetLayout()
{
	std::array bindings = {
		vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex,
									   nullptr),
		vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1,
									   vk::ShaderStageFlagBits::eFragment, nullptr),
	};

	vk::DescriptorSetLayoutCreateInfo layoutInfo{ .bindingCount = static_cast<uint32_t>(bindings.size()),
												 .pBindings = bindings.data() };
	_descriptorSetLayout = vk::raii::DescriptorSetLayout(_device, layoutInfo);
}

void VulkanRenderer::CreateDescriptorPool()
{
	std::array poolSize{ vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT),
						vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, MAX_FRAMES_IN_FLIGHT) };

	vk::DescriptorPoolCreateInfo poolInfo{ .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
										  .maxSets = MAX_FRAMES_IN_FLIGHT,
										  .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
										  .pPoolSizes = poolSize.data() };

	_descriptorPool = vk::raii::DescriptorPool(_device, poolInfo);
}

void VulkanRenderer::CreateDescriptorSets()
{
	std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, _descriptorSetLayout);
	vk::DescriptorSetAllocateInfo        allocInfo{ .descriptorPool = _descriptorPool,
												   .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
												   .pSetLayouts = layouts.data() };

	_descriptorSets.clear();
	_descriptorSets = _device.allocateDescriptorSets(allocInfo);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vk::DescriptorBufferInfo bufferInfo{
			.buffer = _uniformBuffers[i], .offset = 0, .range = sizeof(UniformBufferObject) };
		vk::DescriptorImageInfo imageInfo{ .sampler = _textureSampler,
										  .imageView = _textureImageView,
										  .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal };
		std::array              descriptorWrites{ vk::WriteDescriptorSet{.dstSet = _descriptorSets[i],
																		.dstBinding = 0,
																		.dstArrayElement = 0,
																		.descriptorCount = 1,
																		.descriptorType = vk::DescriptorType::eUniformBuffer,
																		.pBufferInfo = &bufferInfo},
									vk::WriteDescriptorSet{.dstSet = _descriptorSets[i],
																		.dstBinding = 1,
																		.dstArrayElement = 0,
																		.descriptorCount = 1,
																		.descriptorType = vk::DescriptorType::eCombinedImageSampler,
																		.pImageInfo = &imageInfo} };
		_device.updateDescriptorSets(descriptorWrites, {});
	}
}

void VulkanRenderer::CreateGraphicsPipeline()
{
	vk::raii::ShaderModule shaderModule = CreateShaderModule(ReadFile("shaderBin/HelloTri.spv"));

	vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
		.stage = vk::ShaderStageFlagBits::eVertex, .module = shaderModule, .pName = "vertMain" };
	vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
		.stage = vk::ShaderStageFlagBits::eFragment, .module = shaderModule, .pName = "fragMain" };
	vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	auto                                   bindingDescription = Vertex::GetBindingDescription();
	auto                                   attributeDescriptions = Vertex::GetAttributeDescriptions();
	vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &bindingDescription,
		.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
		.pVertexAttributeDescriptions = attributeDescriptions.data() };
	vk::PipelineInputAssemblyStateCreateInfo inputAssembly{ .topology = vk::PrimitiveTopology::eTriangleList,
														   .primitiveRestartEnable = vk::False };
	vk::PipelineViewportStateCreateInfo      viewportState{ .viewportCount = 1, .scissorCount = 1 };
	vk::PipelineRasterizationStateCreateInfo rasterizer{ .depthClampEnable = vk::False,
														.rasterizerDiscardEnable = vk::False,
														.polygonMode = vk::PolygonMode::eFill,
														.cullMode = vk::CullModeFlagBits::eBack,
														.frontFace = vk::FrontFace::eCounterClockwise,
														.depthBiasEnable = vk::False };
	rasterizer.lineWidth = 1.0f;
	vk::PipelineMultisampleStateCreateInfo  multisampling{ .rasterizationSamples = _msaaSamples,
														  .sampleShadingEnable = vk::False };
	vk::PipelineDepthStencilStateCreateInfo depthStencil{ .depthTestEnable = vk::True,
														 .depthWriteEnable = vk::True,
														 .depthCompareOp = vk::CompareOp::eLess,
														 .depthBoundsTestEnable = vk::False,
														 .stencilTestEnable = vk::False };
	vk::PipelineColorBlendAttachmentState   colorBlendAttachment;
	colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
		vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	colorBlendAttachment.blendEnable = vk::False;

	vk::PipelineColorBlendStateCreateInfo colorBlending{ .logicOpEnable = vk::False,
														.logicOp = vk::LogicOp::eCopy,
														.attachmentCount = 1,
														.pAttachments = &colorBlendAttachment };

	std::vector                        dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	vk::PipelineDynamicStateCreateInfo dynamicState{ .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
													.pDynamicStates = dynamicStates.data() };

	vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
		.setLayoutCount = 1, .pSetLayouts = &*_descriptorSetLayout, .pushConstantRangeCount = 0 };

	_pipelineLayout = vk::raii::PipelineLayout(_device, pipelineLayoutInfo);

	vk::Format depthFormat = FindDepthFormat();

	vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
		{.stageCount = 2,
		 .pStages = shaderStages,
		 .pVertexInputState = &vertexInputInfo,
		 .pInputAssemblyState = &inputAssembly,
		 .pViewportState = &viewportState,
		 .pRasterizationState = &rasterizer,
		 .pMultisampleState = &multisampling,
		 .pDepthStencilState = &depthStencil,
		 .pColorBlendState = &colorBlending,
		 .pDynamicState = &dynamicState,
		 .layout = _pipelineLayout,
		 .renderPass = nullptr},
		{.colorAttachmentCount = 1,
		 .pColorAttachmentFormats = &_swapChainSurfaceFormat.format,
		 .depthAttachmentFormat = depthFormat} };

	_graphicsPipeline =
		vk::raii::Pipeline(_device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
}

void VulkanRenderer::CreateCommandPool()
{
	vk::CommandPoolCreateInfo poolInfo{ .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
									   .queueFamilyIndex = _queueIndex };

	_commandPool = vk::raii::CommandPool(_device, poolInfo);
}

void VulkanRenderer::CreateCommandBuffers()
{
	_commandBuffers.clear();
	vk::CommandBufferAllocateInfo allocInfo{ .commandPool = _commandPool,
											.level = vk::CommandBufferLevel::ePrimary,
											.commandBufferCount = MAX_FRAMES_IN_FLIGHT };
	_commandBuffers = vk::raii::CommandBuffers(_device, allocInfo);
}

void VulkanRenderer::RecordCommandBuffer(uint32_t imageIndex)
{
	_commandBuffers[_currentFrame].begin({});
	// Before starting rendering, transition the swapchain image to COLOR_ATTACHMENT_OPTIMAL
	TransitionImageLayout(_swapChainImages[imageIndex], vk::ImageLayout::eUndefined,
		vk::ImageLayout::eColorAttachmentOptimal,
		{},                                                        // srcAccessMask (no need to wait for previous operations)
		vk::AccessFlagBits2::eColorAttachmentWrite,                // dstAccessMask
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // dstStage
		vk::ImageAspectFlagBits::eColor);
	// Transition the multisampled color image to COLOR_ATTACHMENT_OPTIMAL
	TransitionImageLayout(*_colorImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
		vk::AccessFlagBits2::eColorAttachmentWrite, vk::AccessFlagBits2::eColorAttachmentWrite,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::ImageAspectFlagBits::eColor);
	// Transition the depth image to DEPTH_ATTACHMENT_OPTIMAL
	TransitionImageLayout(
		*_depthImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal,
		vk::AccessFlagBits2::eDepthStencilAttachmentWrite, vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
		vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
		vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
		vk::ImageAspectFlagBits::eDepth);

	vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
	vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);

	// Color attachment (multisampled) with resolve attachment
	vk::RenderingAttachmentInfo colorAttachment = { .imageView = _colorImageView,
												   .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
												   .resolveMode = vk::ResolveModeFlagBits::eAverage,
												   .resolveImageView = _swapChainImageViews[imageIndex],
												   .resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal,
												   .loadOp = vk::AttachmentLoadOp::eClear,
												   .storeOp = vk::AttachmentStoreOp::eStore,
												   .clearValue = clearColor };

	// Depth attachment
	vk::RenderingAttachmentInfo depthAttachment = { .imageView = _depthImageView,
												   .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
												   .loadOp = vk::AttachmentLoadOp::eClear,
												   .storeOp = vk::AttachmentStoreOp::eDontCare,
												   .clearValue = clearDepth };

	vk::RenderingInfo renderingInfo = { .renderArea = {.offset = {0, 0}, .extent = _swapChainExtent},
									   .layerCount = 1,
									   .colorAttachmentCount = 1,
									   .pColorAttachments = &colorAttachment,
									   .pDepthAttachment = &depthAttachment };
	_commandBuffers[_currentFrame].beginRendering(renderingInfo);
	_commandBuffers[_currentFrame].bindPipeline(vk::PipelineBindPoint::eGraphics, *_graphicsPipeline);
	_commandBuffers[_currentFrame].setViewport(0,
		vk::Viewport(0.0f, 0.0f, static_cast<float>(_swapChainExtent.width),
			static_cast<float>(_swapChainExtent.height), 0.0f, 1.0f));
	_commandBuffers[_currentFrame].setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), _swapChainExtent));
	_commandBuffers[_currentFrame].bindVertexBuffers(0, *_vertexBuffer, { 0 });
	_commandBuffers[_currentFrame].bindIndexBuffer(*_indexBuffer, 0, vk::IndexType::eUint32);
	_commandBuffers[_currentFrame].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _pipelineLayout, 0,
		*_descriptorSets[_currentFrame], nullptr);
	_commandBuffers[_currentFrame].drawIndexed(_indices.size(), 1, 0, 0, 0);
	_commandBuffers[_currentFrame].endRendering();
	// After rendering, transition the swapchain image to PRESENT_SRC
	TransitionImageLayout(_swapChainImages[imageIndex], vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageLayout::ePresentSrcKHR,
		vk::AccessFlagBits2::eColorAttachmentWrite,                // srcAccessMask
		{},                                                        // dstAccessMask
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
		vk::PipelineStageFlagBits2::eBottomOfPipe,                 // dstStage
		vk::ImageAspectFlagBits::eColor);
	_commandBuffers[_currentFrame].end();
}

std::unique_ptr<vk::raii::CommandBuffer> VulkanRenderer::BeginSingleTimeCommands()
{
	vk::CommandBufferAllocateInfo allocInfo{
		.commandPool = _commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1 };
	std::unique_ptr<vk::raii::CommandBuffer> commandBuffer =
		std::make_unique<vk::raii::CommandBuffer>(std::move(vk::raii::CommandBuffers(_device, allocInfo).front()));

	vk::CommandBufferBeginInfo beginInfo{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
	commandBuffer->begin(beginInfo);

	return commandBuffer;
}

void VulkanRenderer::EndSingleTimeCommands(const vk::raii::CommandBuffer& commandBuffer)
{
	commandBuffer.end();

	vk::SubmitInfo submitInfo{ .commandBufferCount = 1, .pCommandBuffers = &*commandBuffer };
	_queue.submit(submitInfo, nullptr);
	_queue.waitIdle();
}

void VulkanRenderer::CreateTextureImage()
{
	int            texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	vk::DeviceSize imageSize = texWidth * texHeight * 4;
	_mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

	if (!pixels)
	{
		throw std::runtime_error("failed to load texture image!");
	}

	vk::raii::Buffer       stagingBuffer({});
	vk::raii::DeviceMemory stagingBufferMemory({});
	CreateBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer,
		stagingBufferMemory);

	void* data = stagingBufferMemory.mapMemory(0, imageSize);
	memcpy(data, pixels, imageSize);
	stagingBufferMemory.unmapMemory();

	stbi_image_free(pixels);

	CreateImage(texWidth, texHeight, _mipLevels, vk::SampleCountFlagBits::e1, vk::Format::eR8G8B8A8Srgb,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst |
		vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal, _textureImage, _textureImageMemory);

	TransitionImageLayout(_textureImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, _mipLevels);
	CopyBufferToImage(stagingBuffer, _textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

	GenerateMipmaps(_textureImage, vk::Format::eR8G8B8A8Srgb, texWidth, texHeight, _mipLevels);
}

void VulkanRenderer::CreateTextureImageView()
{
	_textureImageView =
		CreateImageView(_textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, _mipLevels);
}

void VulkanRenderer::CreateTextureSampler()
{
	vk::PhysicalDeviceProperties properties = _physicalDevice.getProperties();
	vk::SamplerCreateInfo        samplerInfo{ .magFilter = vk::Filter::eLinear,
											 .minFilter = vk::Filter::eLinear,
											 .mipmapMode = vk::SamplerMipmapMode::eLinear,
											 .addressModeU = vk::SamplerAddressMode::eRepeat,
											 .addressModeV = vk::SamplerAddressMode::eRepeat,
											 .addressModeW = vk::SamplerAddressMode::eRepeat,
											 .mipLodBias = 0.0f,
											 .anisotropyEnable = vk::True,
											 .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
											 .compareEnable = vk::False,
											 .compareOp = vk::CompareOp::eAlways };
	_textureSampler = vk::raii::Sampler(_device, samplerInfo);
}

void VulkanRenderer::CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, vk::SampleCountFlagBits numSamples,
	vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
	vk::MemoryPropertyFlags properties, vk::raii::Image& image,
	vk::raii::DeviceMemory& imageMemory)
{
	vk::ImageCreateInfo imageInfo{ .imageType = vk::ImageType::e2D,
								  .format = format,
								  .extent = {width, height, 1},
								  .mipLevels = mipLevels,
								  .arrayLayers = 1,
								  .samples = numSamples,
								  .tiling = tiling,
								  .usage = usage,
								  .sharingMode = vk::SharingMode::eExclusive,
								  .initialLayout = vk::ImageLayout::eUndefined };

	image = vk::raii::Image(_device, imageInfo);

	vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
	vk::MemoryAllocateInfo allocInfo{ .allocationSize = memRequirements.size,
									 .memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties) };
	imageMemory = vk::raii::DeviceMemory(_device, allocInfo);
	image.bindMemory(imageMemory, 0);
}

[[nodiscard]] vk::raii::ImageView VulkanRenderer::CreateImageView(const vk::raii::Image& image, vk::Format format,
	vk::ImageAspectFlags aspectFlags, uint32_t mipLevels) const
{
	vk::ImageViewCreateInfo viewInfo{ .image = image,
									 .viewType = vk::ImageViewType::e2D,
									 .format = format,
									 .subresourceRange = {aspectFlags, 0, mipLevels, 0, 1} };
	return vk::raii::ImageView(_device, viewInfo);
}

void VulkanRenderer::TransitionImageLayout(const vk::raii::Image& image, const vk::ImageLayout oldLayout,
	const vk::ImageLayout newLayout, uint32_t mipLevels)
{
	const auto commandBuffer = BeginSingleTimeCommands();

	vk::ImageMemoryBarrier barrier{ .oldLayout = oldLayout,
								   .newLayout = newLayout,
								   .image = image,
								   .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, 1} };

	vk::PipelineStageFlags sourceStage;
	vk::PipelineStageFlags destinationStage;

	if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
	{
		barrier.srcAccessMask = {};
		barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

		sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
		destinationStage = vk::PipelineStageFlagBits::eTransfer;
	}
	else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
	{
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		sourceStage = vk::PipelineStageFlagBits::eTransfer;
		destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
	}
	else
	{
		throw std::invalid_argument("unsupported layout transition!");
	}
	commandBuffer->pipelineBarrier(sourceStage, destinationStage, {}, {}, nullptr, barrier);
	EndSingleTimeCommands(*commandBuffer);
}
void VulkanRenderer::TransitionImageLayout(vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
	vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask,
	vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask,
	vk::ImageAspectFlags aspectMask)
{
	vk::ImageMemoryBarrier2 barrier = {
		.srcStageMask = srcStageMask,
		.srcAccessMask = srcAccessMask,
		.dstStageMask = dstStageMask,
		.dstAccessMask = dstAccessMask,
		.oldLayout = oldLayout,
		.newLayout = newLayout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = image,
		.subresourceRange = {
			   .aspectMask = aspectMask, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1} };
	vk::DependencyInfo dependency_info = {
		.dependencyFlags = {}, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier };
	_commandBuffers[_currentFrame].pipelineBarrier2(dependency_info);
}

void VulkanRenderer::CopyBufferToImage(const vk::raii::Buffer& buffer, const vk::raii::Image& image, uint32_t width,
	uint32_t height)
{
	std::unique_ptr<vk::raii::CommandBuffer> commandBuffer = BeginSingleTimeCommands();

	vk::BufferImageCopy region{ .bufferOffset = 0,
							   .bufferRowLength = 0,
							   .bufferImageHeight = 0,
							   .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
							   .imageOffset = {0, 0, 0},
							   .imageExtent = {width, height, 1} };

	commandBuffer->copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, { region });

	EndSingleTimeCommands(*commandBuffer);
}

void VulkanRenderer::GenerateMipmaps(const vk::raii::Image& image, vk::Format imageFormat, int32_t texWidth, int32_t texHeight,
	uint32_t mipLevels)
{
	// Check if image format supports linear blit-ing
	vk::FormatProperties formatProperties = _physicalDevice.getFormatProperties(imageFormat);

	if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear))
	{
		throw std::runtime_error("texture image format does not support linear blitting!");
	}

	std::unique_ptr<vk::raii::CommandBuffer> commandBuffer = BeginSingleTimeCommands();

	vk::ImageMemoryBarrier barrier = { .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
											   .dstAccessMask = vk::AccessFlagBits::eTransferRead,
											   .oldLayout = vk::ImageLayout::eTransferDstOptimal,
											   .newLayout = vk::ImageLayout::eTransferSrcOptimal,
											   .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
											   .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
											   .image = image };
	barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	int32_t mipWidth = texWidth;
	int32_t mipHeight = texHeight;

	for (uint32_t i = 1; i < mipLevels; i++)
	{
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
		barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

		commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {},
			{}, {}, barrier);

		vk::ArrayWrapper1D<vk::Offset3D, 2> offsets, dstOffsets;
		offsets[0] = vk::Offset3D(0, 0, 0);
		offsets[1] = vk::Offset3D(mipWidth, mipHeight, 1);
		dstOffsets[0] = vk::Offset3D(0, 0, 0);
		dstOffsets[1] = vk::Offset3D(mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1);
		vk::ImageBlit blit = {
			.srcSubresource = {}, .srcOffsets = offsets, .dstSubresource = {}, .dstOffsets = dstOffsets };
		blit.srcSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i - 1, 0, 1);
		blit.dstSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i, 0, 1);

		commandBuffer->blitImage(image, vk::ImageLayout::eTransferSrcOptimal, image,
			vk::ImageLayout::eTransferDstOptimal, { blit }, vk::Filter::eLinear);

		barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
		barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
			{}, {}, {}, barrier);

		if (mipWidth > 1)
			mipWidth /= 2;
		if (mipHeight > 1)
			mipHeight /= 2;
	}

	barrier.subresourceRange.baseMipLevel = mipLevels - 1;
	barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
	barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
	barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

	commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {},
		{}, {}, barrier);

	EndSingleTimeCommands(*commandBuffer);
}

void VulkanRenderer::LoadModel()
{
	tinyobj::attrib_t                attrib;
	std::vector<tinyobj::shape_t>    shapes;
	std::vector<tinyobj::material_t> materials;
	std::string                      err;
	std::string                      warn;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str()))
		throw std::runtime_error(err);

	std::unordered_map<Vertex, uint32_t> uniqueVertices{};

	for (const auto& shape : shapes)
	{
		for (const auto& index : shape.mesh.indices)
		{
			Vertex vertex{};

			vertex.pos = { attrib.vertices[3 * index.vertex_index + 0], attrib.vertices[3 * index.vertex_index + 1],
						  attrib.vertices[3 * index.vertex_index + 2] };

			vertex.texCoord = { attrib.texcoords[2 * index.texcoord_index + 0],
							   1.0f - attrib.texcoords[2 * index.texcoord_index + 1] };

			vertex.color = { 1.0f, 1.0f, 1.0f };

			if (!uniqueVertices.contains(vertex))
			{
				uniqueVertices[vertex] = static_cast<uint32_t>(_vertices.size());
				_vertices.push_back(vertex);
			}

			_indices.push_back(uniqueVertices[vertex]);
		}
	}
}

void VulkanRenderer::CreateDepthResources()
{
	vk::Format depthFormat = FindDepthFormat();

	CreateImage(_swapChainExtent.width, _swapChainExtent.height, 1, _msaaSamples, depthFormat,
		vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment,
		vk::MemoryPropertyFlagBits::eDeviceLocal, _depthImage, _depthImageMemory);

	_depthImageView = CreateImageView(_depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth, 1);
}

vk::Format VulkanRenderer::FindSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling,
	vk::FormatFeatureFlags features) const
{
	for (const auto format : candidates)
	{
		vk::FormatProperties props = _physicalDevice.getFormatProperties(format);

		if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features)
			return format;
		if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features)
			return format;
	}

	throw std::runtime_error("Failed to find supported format");
}

[[nodiscard]] vk::Format VulkanRenderer::FindDepthFormat() const
{
	return FindSupportedFormat({ vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
		vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

bool VulkanRenderer::HasStencilComponent(vk::Format format) const
{
	return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
}

void VulkanRenderer::CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties,
	vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufferMemory)
{
	vk::BufferCreateInfo bufferInfo{ .size = size, .usage = usage, .sharingMode = vk::SharingMode::eExclusive };

	buffer = vk::raii::Buffer(_device, bufferInfo);

	vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();

	vk::MemoryAllocateInfo allocInfo{ .allocationSize = memRequirements.size,
									 .memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties) };

	bufferMemory = vk::raii::DeviceMemory(_device, allocInfo);

	buffer.bindMemory(bufferMemory, 0);
}

void VulkanRenderer::CreateVertexBuffer()
{
	vk::DeviceSize         bufferSize = sizeof(_vertices[0]) * _vertices.size();
	vk::raii::Buffer       stagingBuffer({});
	vk::raii::DeviceMemory stagingBufferMemory({});
	CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer,
		stagingBufferMemory);

	void* dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
	memcpy(dataStaging, _vertices.data(), bufferSize);
	stagingBufferMemory.unmapMemory();

	CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal, _vertexBuffer, _vertexBufferMemory);

	CopyBuffer(stagingBuffer, _vertexBuffer, bufferSize);
}

void VulkanRenderer::CreateIndexBuffer()
{
	vk::DeviceSize bufferSize = sizeof(_indices[0]) * _indices.size();

	vk::raii::Buffer       stagingBuffer({});
	vk::raii::DeviceMemory stagingBufferMemory({});
	CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer,
		stagingBufferMemory);

	void* data = stagingBufferMemory.mapMemory(0, bufferSize);
	memcpy(data, _indices.data(), bufferSize);
	stagingBufferMemory.unmapMemory();

	CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal, _indexBuffer, _indexBufferMemory);

	CopyBuffer(stagingBuffer, _indexBuffer, bufferSize);
}

void VulkanRenderer::CreateUniformBuffers()
{
	_uniformBuffers.clear();
	_uniformBuffersMemory.clear();
	_uniformBuffersMapped.clear();

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vk::DeviceSize         bufferSize = sizeof(UniformBufferObject);
		vk::raii::Buffer       buffer({});
		vk::raii::DeviceMemory bufferMem({});
		CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, buffer,
			bufferMem);
		_uniformBuffers.emplace_back(std::move(buffer));
		_uniformBuffersMemory.emplace_back(std::move(bufferMem));
		_uniformBuffersMapped.emplace_back(_uniformBuffersMemory[i].mapMemory(0, bufferSize));
	}
}

uint32_t VulkanRenderer::FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
	vk::PhysicalDeviceMemoryProperties memProperties = _physicalDevice.getMemoryProperties();

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		// Check if corresponding bit set to 1 AND
		// support for property
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			return i;
	}

	throw std::runtime_error("Failed to find suitable memory type!");
}

void VulkanRenderer::CopyBuffer(vk::raii::Buffer& srcBuffer, vk::raii::Buffer& dstBuffer, vk::DeviceSize size)
{
	vk::CommandBufferAllocateInfo allocInfo{
		.commandPool = _commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1 };

	vk::raii::CommandBuffer commandCopyBuffer = std::move(_device.allocateCommandBuffers(allocInfo).front());

	commandCopyBuffer.begin(vk::CommandBufferBeginInfo{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
	commandCopyBuffer.copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy{ .size = size });
	commandCopyBuffer.end();

	_queue.submit(vk::SubmitInfo{ .commandBufferCount = 1, .pCommandBuffers = &*commandCopyBuffer }, nullptr);
	_queue.waitIdle();
}

void VulkanRenderer::CreateSyncObjects()
{
	// Semaphores for swapchain operations on GPU (acquire image, submit command buffer etc.)
	// Fence for waiting the previous frame to finish on CPU (block execution)
	_presentCompleteSemaphores.clear();
	_renderFinishedSemaphores.clear();
	_inFlightFences.clear();

	for (size_t i = 0; i < _swapChainImages.size(); i++)
	{
		_presentCompleteSemaphores.emplace_back(_device, vk::SemaphoreCreateInfo());
		_renderFinishedSemaphores.emplace_back(_device, vk::SemaphoreCreateInfo());
	}

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		_inFlightFences.emplace_back(_device, vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled });
	}
}

std::vector<char> VulkanRenderer::ReadFile(const std::string& filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open())
		throw std::runtime_error("Failed to open file");

	std::vector<char> buffer(file.tellg());
	file.seekg(0, std::ios::beg);
	file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
	file.close();

	return buffer;
}

[[nodiscard]] vk::raii::ShaderModule VulkanRenderer::CreateShaderModule(const std::vector<char>& code) const
{
	vk::ShaderModuleCreateInfo createInfo{ .codeSize = code.size(),
										  .pCode = reinterpret_cast<const uint32_t*>(code.data()) };

	vk::raii::ShaderModule shaderModule{ _device, createInfo };

	return shaderModule;
}

vk::SampleCountFlagBits VulkanRenderer::GetMaxUsableSampleCount() const
{
	vk::PhysicalDeviceProperties physicalDeviceProperties = _physicalDevice.getProperties();

	vk::SampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts &
		physicalDeviceProperties.limits.framebufferDepthSampleCounts;

	if (counts & vk::SampleCountFlagBits::e64)
		return vk::SampleCountFlagBits::e64;
	if (counts & vk::SampleCountFlagBits::e32)
		return vk::SampleCountFlagBits::e32;
	if (counts & vk::SampleCountFlagBits::e16)
		return vk::SampleCountFlagBits::e16;
	if (counts & vk::SampleCountFlagBits::e8)
		return vk::SampleCountFlagBits::e8;
	if (counts & vk::SampleCountFlagBits::e4)
		return vk::SampleCountFlagBits::e4;
	if (counts & vk::SampleCountFlagBits::e2)
		return vk::SampleCountFlagBits::e2;

	return vk::SampleCountFlagBits::e1;
}

void VulkanRenderer::CreateColorResources()
{
	vk::Format colorFormat = _swapChainSurfaceFormat.format;

	CreateImage(_swapChainExtent.width, _swapChainExtent.height, 1, _msaaSamples, colorFormat,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment,
		vk::MemoryPropertyFlagBits::eDeviceLocal, _colorImage, _colorImageMemory);

	_colorImageView = CreateImageView(_colorImage, colorFormat, vk::ImageAspectFlagBits::eColor, 1);
}
