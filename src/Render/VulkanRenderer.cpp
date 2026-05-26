#include "VulkanRenderer.h"
#include "DrawJob.h"
#include "RenderCommons.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <unordered_map>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"


VulkanRenderer::VulkanRenderer()
	: _programCtx(UtahCtx::Get())
{
}

VulkanRenderer::~VulkanRenderer()
{
	if (*_vkCtx.GetDevice() == VK_NULL_HANDLE)
		return;

	_vkCtx.GetDevice().waitIdle();

	_colorImageView = nullptr;
	_depthImageView = nullptr;

	DestroyImage(_colorImage);
	DestroyImage(_depthImage);

	for (auto& ubo : _cameraUBOs)
		DestroyBuffer(ubo);
	for (auto& ubo : _lightUBOs)
		DestroyBuffer(ubo);
	for (auto& ssbo : _objectSSBOs)
		DestroyBuffer(ssbo);
	for (auto& ssbo : _materialSSBOs)
		DestroyBuffer(ssbo);
}

void VulkanRenderer::Initialize()
{
	RegisterResizeCallback();

	_vkCtx.Initialize(_programCtx.GetContextWindow());

	_msaaSamples = GetMaxUsableSampleCount();
	CreateSwapChain();
	CreateImageViews();

	CreateDescriptorSetLayout();
	CreateGraphicsPipeline("shaderBin/blinn_phong_vert.spv", "shaderBin/blinn_phong_frag.spv");
	CreateGraphicsPipeline("shaderBin/unlit_vert.spv", "shaderBin/unlit_frag.spv");

	CreateCommandPool();
	CreateColorResources();
	CreateDepthResources();

	_textureManger.Initialize(this, &_vkCtx);
	_textureManger.ImportTexture("models/viking_room.png");
	_textureManger.ImportTexture("models/viking_room_2.png");

	_meshManager.Initialize(this);
	_meshManager.ImportMesh("models/viking_room.obj");
	_meshManager.ImportMesh("models/utah_teapot.obj");

	CreateUniformBuffers();
	CreateDescriptorPool();
	CreateDescriptorSets();
	CreateCommandBuffers();

	CreateSyncObjects();

	InitImGUI();

	std::vector<uint32_t> texIndices;
	texIndices.push_back(0);
	_materialManager.CreateBlinnPhongMaterial(0, texIndices, glm::vec4(0.0f));
	_materialManager.CreateUnlitMaterial(1, glm::vec4(0.2f, 0.9f, 0.2f, 1.0f));
	auto matGPUs = _materialManager.ConvertMaterialsToGPU();

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		MaterialSSBO* materials = static_cast<MaterialSSBO*>(_materialSSBOs[i].info.pMappedData);
		for (size_t j = 0; j < matGPUs.size(); ++j)
		{
			materials[j].color = matGPUs[j].baseColor;
			memcpy(materials[j].texIndices, matGPUs[j].texIndices, sizeof(uint32_t) * 4);
		}
	}
}

void VulkanRenderer::UpdateDrawList(std::vector<struct DrawJob>&& list)
{
	_drawList = std::move(list);
}

void VulkanRenderer::UpdateCamera(const CameraComponent& camera)
{
	_mainCam = camera;
}

void VulkanRenderer::UpdateLights(std::vector<PointLightGPU>&& lights)
{
	_pointLights = std::move(lights);
}

void VulkanRenderer::DrawFrame()
{
	// Wait previous frame to finish (block execution)
	while (vk::Result::eTimeout == _vkCtx.GetDevice().waitForFences(*_inFlightFences[_currentFrame], vk::True, UINT64_MAX))
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

	_vkCtx.GetDevice().resetFences(*_inFlightFences[_currentFrame]);

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
	_vkCtx.GetQueue().submit(submitInfo, *_inFlightFences[_currentFrame]);

	try
	{
		const vk::PresentInfoKHR presentInfo{ .waitSemaphoreCount = 1,
											 .pWaitSemaphores = &*_renderFinishedSemaphores[imageIndex],
											 .swapchainCount = 1,
											 .pSwapchains = &*_swapChain,
											 .pImageIndices = &imageIndex };

		result = _vkCtx.GetQueue().presentKHR(presentInfo);
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
	_vkCtx.GetDevice().waitIdle();
}

void VulkanRenderer::InitImGUI()
{
	// Separate desc pool for imgui
	vk::DescriptorPoolSize poolSize{ vk::DescriptorType::eCombinedImageSampler,
									IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE };
	vk::DescriptorPoolCreateInfo poolInfo{ .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
										.maxSets = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE,
										.poolSizeCount = 1,
										.pPoolSizes = &poolSize };

	_imguiDescriptorPool = vk::raii::DescriptorPool(_vkCtx.GetDevice(), poolInfo);

	ImGui_ImplGlfw_InitForVulkan(_programCtx.GetContextWindow(), true);
	VkFormat colorFormat = static_cast<VkFormat>(_swapChainSurfaceFormat.format);
	VkFormat depthFormat = static_cast<VkFormat>(FindDepthFormat());

	ImGui_ImplVulkan_InitInfo initInfo{};
	initInfo.ApiVersion = VK_API_VERSION_1_3;
	initInfo.Instance = *_vkCtx.GetInstance();
	initInfo.PhysicalDevice = *_vkCtx.GetPhysicalDevice();
	initInfo.Device = *_vkCtx.GetDevice();
	initInfo.QueueFamily = _vkCtx.GetQueueFamilyIndex();
	initInfo.Queue = *_vkCtx.GetQueue();
	initInfo.DescriptorPool = *_imguiDescriptorPool;
	initInfo.MinImageCount = 2;
	initInfo.ImageCount = static_cast<uint32_t>(_swapChainImages.size());
	initInfo.PipelineCache = VK_NULL_HANDLE;

	initInfo.PipelineInfoMain.MSAASamples = static_cast<VkSampleCountFlagBits>(_msaaSamples);
	initInfo.UseDynamicRendering = true;
	initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = {};
	initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType =
		VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;
	initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.depthAttachmentFormat = depthFormat;


	ImGui_ImplVulkan_Init(&initInfo);
}

void VulkanRenderer::UpdateUniformBuffer(uint32_t currentImage)
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto  currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	CameraUBO camUBO{};
	camUBO.view = glm::lookAt(_mainCam._pos, _mainCam._pos + _mainCam.GetFrontVector(), glm::vec3(0.0f, 1.0f, 0.0f));
	// Swap far and near plane input -> Reverse Z for greater precision
	camUBO.proj =
		glm::perspective(glm::radians(_mainCam._verticalFOV), _swapChainExtent.width / (float)_swapChainExtent.height, _mainCam._farPlane, _mainCam._nearPlane);
	// Flip y axis since GLM's was inverted
	camUBO.proj[1][1] *= -1;
	memcpy(_cameraUBOs[currentImage].info.pMappedData, &camUBO, sizeof(camUBO));

	LightUBO lightUBO{};
	lightUBO.eyePos = _mainCam._pos;
	lightUBO.pointLightNum = static_cast<uint32_t>(_pointLights.size());
	std::memcpy(lightUBO.pointLights, _pointLights.data(),
		std::min(_pointLights.size(), size_t(32)) * sizeof(PointLightGPU));
	memcpy(_lightUBOs[currentImage].info.pMappedData, &lightUBO, sizeof(lightUBO));

	ObjectSSBO* objects = static_cast<ObjectSSBO*>(_objectSSBOs[currentImage].info.pMappedData);
	for (size_t i = 0; i < _drawList.size(); ++i)
	{
		objects[i].model = _drawList[i]._model;
	}
}

void VulkanRenderer::RegisterResizeCallback()
{
	glfwSetFramebufferSizeCallback(_programCtx.GetContextWindow(), FramebufferResizeCallback);
}

void VulkanRenderer::FramebufferResizeCallback(GLFWwindow* window, int width, int height)
{
	auto* pAppCtx = static_cast<UtahCtx*>(glfwGetWindowUserPointer(window));
	if (!pAppCtx)
		throw std::runtime_error("Unable to get Utah Context from GLFW");

	// Reroute resize notification to UtahCtx->RenderSystem->VulkanRenderer
	pAppCtx->NotifyFramebufferResized();
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
	glfwGetFramebufferSize(_programCtx.GetContextWindow(), &width, &height);

	return { std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
			std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height) };
}

void VulkanRenderer::CreateSwapChain()
{
	auto surfaceCapabilities = _vkCtx.GetPhysicalDevice().getSurfaceCapabilitiesKHR(*_vkCtx.GetSurface());
	_swapChainExtent = ChooseSwapExtent(surfaceCapabilities);
	_swapChainSurfaceFormat = ChooseSwapSurfaceFormat(_vkCtx.GetPhysicalDevice().getSurfaceFormatsKHR(*_vkCtx.GetSurface()));

	vk::SwapchainCreateInfoKHR swapChainCreateInfo{
		.surface = *_vkCtx.GetSurface(),
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
		.presentMode = ChooseSwapPresentMode(_vkCtx.GetPhysicalDevice().getSurfacePresentModesKHR(*_vkCtx.GetSurface())),
		.clipped = true };

	_swapChain = vk::raii::SwapchainKHR(_vkCtx.GetDevice(), swapChainCreateInfo);
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
		_swapChainImageViews.emplace_back(_vkCtx.GetDevice(), imageViewCreateInfo);
	}
}

void VulkanRenderer::CleanupSwapChain()
{
	// Image views must be destroyed before the underlying VMA-allocated images.
	_colorImageView = nullptr;
	_depthImageView = nullptr;
	DestroyImage(_colorImage);
	DestroyImage(_depthImage);

	_swapChainImageViews.clear();
	_swapChain = nullptr;
}

void VulkanRenderer::RecreateSwapChain()
{
	int width = 0, height = 0;
	glfwGetFramebufferSize(_programCtx.GetContextWindow(), &width, &height);
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(_programCtx.GetContextWindow(), &width, &height);
		glfwWaitEvents();
	}

	_vkCtx.GetDevice().waitIdle();

	CleanupSwapChain();

	CreateSwapChain();
	CreateImageViews();
	CreateColorResources();
	CreateDepthResources();
}

void VulkanRenderer::CreateDescriptorSetLayout()
{
	std::array bindings = {
			// 0: CameraUBO (vertex)
			vk::DescriptorSetLayoutBinding{
				.binding = 0, .descriptorType = vk::DescriptorType::eUniformBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eVertex },
			// 1: LightUBO (fragment)
			vk::DescriptorSetLayoutBinding{
				.binding = 1, .descriptorType = vk::DescriptorType::eUniformBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eFragment },
			// 2: ObjectBuffer SSBO (vertex)
			vk::DescriptorSetLayoutBinding{
				.binding = 2, .descriptorType = vk::DescriptorType::eStorageBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eVertex },
			// 3: MaterialBuffer SSBO (fragment)
			vk::DescriptorSetLayoutBinding{
				.binding = 3, .descriptorType = vk::DescriptorType::eStorageBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eFragment },
			// 4: combined-image-sampler array (fragment) - bindless texture slot
			vk::DescriptorSetLayoutBinding{
				.binding = 4, .descriptorType = vk::DescriptorType::eCombinedImageSampler,
				.descriptorCount = MAX_TEXTURES,
				.stageFlags = vk::ShaderStageFlagBits::eFragment },
	};

	// Per-binding flags so binding 4 can be partially-bound + update-after-bind
	std::array<vk::DescriptorBindingFlags, 5> bindingFlags = {
	{
		{}, {}, {}, {},
		vk::DescriptorBindingFlagBits::ePartiallyBound |
		vk::DescriptorBindingFlagBits::eUpdateAfterBind
	}
	};
	vk::DescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{
		.bindingCount = static_cast<uint32_t>(bindingFlags.size()),
		.pBindingFlags = bindingFlags.data()
	};

	vk::DescriptorSetLayoutCreateInfo info{
		.pNext = &flagsInfo,
		.flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
		.bindingCount = static_cast<uint32_t>(bindings.size()),
		.pBindings = bindings.data()
	};

	_globalDescriptorSetLayout = vk::raii::DescriptorSetLayout(_vkCtx.GetDevice(), info);

}

void VulkanRenderer::CreateDescriptorPool()
{
	std::array poolSize{
						vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT * 2),
						vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, MAX_FRAMES_IN_FLIGHT * 2),
						vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, MAX_FRAMES_IN_FLIGHT * MAX_TEXTURES) };

	vk::DescriptorPoolCreateInfo poolInfo{ .flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind | vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
										  .maxSets = MAX_FRAMES_IN_FLIGHT,
										  .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
										  .pPoolSizes = poolSize.data() };

	_descriptorPool = vk::raii::DescriptorPool(_vkCtx.GetDevice(), poolInfo);

	//TODO: Check out using dynamic uniform buffers to cut down the number
}

void VulkanRenderer::CreateDescriptorSets()
{
	std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, _globalDescriptorSetLayout);

	vk::DescriptorSetAllocateInfo allocInfo{ .descriptorPool = _descriptorPool,
											.descriptorSetCount = static_cast<uint32_t>(layouts.size()),
											.pSetLayouts = layouts.data() };

	_globalDescriptorSets.clear();
	_globalDescriptorSets = _vkCtx.GetDevice().allocateDescriptorSets(allocInfo);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vk::DescriptorBufferInfo cameraBufferInfo{ .buffer = _cameraUBOs[i].buffer,
											.offset = 0,
											.range = sizeof(CameraUBO) };

		vk::DescriptorBufferInfo lightsBufferInfo{ .buffer = _lightUBOs[i].buffer,
											.offset = 0,
											.range = sizeof(LightUBO) };

		vk::DescriptorBufferInfo objectBufferInfo{ .buffer = _objectSSBOs[i].buffer,
											.offset = 0,
											.range = sizeof(ObjectSSBO) * MAX_OBJECTS };

		vk::DescriptorBufferInfo materialBufferInfo{ .buffer = _materialSSBOs[i].buffer,
											.offset = 0,
											.range = sizeof(MaterialSSBO) * MAX_OBJECTS }; //TODO: Need of a MAX_MATERIALS?

			// Slot 0 of the bindless texture array -- partially-bound, so we only
			// write the slots actually accessed by the shader.

		size_t texCount = _textureManger.GetTexturesCount();
		std::vector<vk::DescriptorImageInfo> textureInfos;
		textureInfos.reserve(texCount);

		for (int i = 0; i < texCount; i++)
		{
			textureInfos.push_back(vk::DescriptorImageInfo{ .sampler = _textureManger.GetTextureSampler(i),
											.imageView = _textureManger.GetTextureImageView(i),
											.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal });
		}


		std::array descriptorWrites{
			vk::WriteDescriptorSet{.dstSet = _globalDescriptorSets[i],
									.dstBinding = 0,
									.dstArrayElement = 0,
									.descriptorCount = 1,
									.descriptorType = vk::DescriptorType::eUniformBuffer,
									.pBufferInfo = &cameraBufferInfo},
			vk::WriteDescriptorSet{.dstSet = _globalDescriptorSets[i],
									.dstBinding = 1,
									.dstArrayElement = 0,
									.descriptorCount = 1,
									.descriptorType = vk::DescriptorType::eUniformBuffer,
									.pBufferInfo = &lightsBufferInfo},
			vk::WriteDescriptorSet{.dstSet = _globalDescriptorSets[i],
									.dstBinding = 2,
									.dstArrayElement = 0,
									.descriptorCount = 1,
									.descriptorType = vk::DescriptorType::eStorageBuffer,
									.pBufferInfo = &objectBufferInfo},
			vk::WriteDescriptorSet{.dstSet = _globalDescriptorSets[i],
									.dstBinding = 3,
									.dstArrayElement = 0,
									.descriptorCount = 1,
									.descriptorType = vk::DescriptorType::eStorageBuffer,
									.pBufferInfo = &materialBufferInfo},
			vk::WriteDescriptorSet{.dstSet = _globalDescriptorSets[i],
									.dstBinding = 4,
									.dstArrayElement = 0,
									.descriptorCount = static_cast<uint32_t>(texCount),
									.descriptorType = vk::DescriptorType::eCombinedImageSampler,
									.pImageInfo = textureInfos.data()}};

		_vkCtx.GetDevice().updateDescriptorSets(descriptorWrites, {});


	}
}

uint32_t VulkanRenderer::CreateGraphicsPipeline(const std::string& vertPath, const std::string& fragPath)
{
	vk::raii::ShaderModule vertModule = CreateShaderModule(ReadFile(vertPath));
	vk::raii::ShaderModule fragModule = CreateShaderModule(ReadFile(fragPath));


	vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
		.stage = vk::ShaderStageFlagBits::eVertex, .module = vertModule, .pName = "main" };
	vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
		.stage = vk::ShaderStageFlagBits::eFragment, .module = fragModule, .pName = "main" };
	vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	auto bindingDescription = Vertex::GetBindingDescription();
	auto attributeDescriptions = Vertex::GetAttributeDescriptions();
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

	std::vector dynamicStates = { 
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor,
		vk::DynamicState::eCullMode,
		vk::DynamicState::eFrontFace,
		vk::DynamicState::eDepthTestEnable,
		vk::DynamicState::eDepthWriteEnable,
		vk::DynamicState::eDepthCompareOp,
		vk::DynamicState::ePrimitiveTopology };

	vk::PipelineDynamicStateCreateInfo dynamicState{ .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
													.pDynamicStates = dynamicStates.data() };

	vk::PushConstantRange pushRange{
		.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
		.offset = 0,
		.size = sizeof(uint32_t) * 2   // objectIndex, textureIndex
	};

	vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
		.setLayoutCount = 1,
		.pSetLayouts = &*_globalDescriptorSetLayout,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &pushRange};

	_pipelineLayout = vk::raii::PipelineLayout(_vkCtx.GetDevice(), pipelineLayoutInfo);

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


	_pipelines.emplace_back(vk::raii::Pipeline(_vkCtx.GetDevice(), nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>()));
	return _pipelines.size() - 1;
}

void VulkanRenderer::CreateCommandPool()
{
	vk::CommandPoolCreateInfo poolInfo{ .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
									   .queueFamilyIndex = _vkCtx.GetQueueFamilyIndex() };

	_commandPool = vk::raii::CommandPool(_vkCtx.GetDevice(), poolInfo);
}

void VulkanRenderer::CreateCommandBuffers()
{
	_commandBuffers.clear();
	vk::CommandBufferAllocateInfo allocInfo{ .commandPool = _commandPool,
											.level = vk::CommandBufferLevel::ePrimary,
											.commandBufferCount = MAX_FRAMES_IN_FLIGHT };
	_commandBuffers = vk::raii::CommandBuffers(_vkCtx.GetDevice(), allocInfo);
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
	TransitionImageLayout(_colorImage.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
		vk::AccessFlagBits2::eColorAttachmentWrite, vk::AccessFlagBits2::eColorAttachmentWrite,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::ImageAspectFlagBits::eColor);
	// Transition the depth image to DEPTH_ATTACHMENT_OPTIMAL
	TransitionImageLayout(
		_depthImage.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal,
		vk::AccessFlagBits2::eDepthStencilAttachmentWrite, vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
		vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
		vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
		vk::ImageAspectFlagBits::eDepth);

	vk::ClearValue clearColor = vk::ClearColorValue(0.015f, 0.015f, 0.015f, 1.0f);
	// Reverse Z, cleared 0.0f instead
	vk::ClearValue clearDepth = vk::ClearDepthStencilValue(0.0f, 0);

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
	

	for (uint32_t i = 0; i < _drawList.size(); i++)
	{
		uint32_t j = i % 2;

		Material mat = _materialManager.GetMaterial(j);

		_commandBuffers[_currentFrame].bindPipeline(vk::PipelineBindPoint::eGraphics, *_pipelines[mat.pipeline]);

		_commandBuffers[_currentFrame].setViewport(0,
			vk::Viewport(
				0.0f, 0.0f,
				static_cast<float>(_swapChainExtent.width),
				static_cast<float>(_swapChainExtent.height),
				0.0f, 1.0f));

		_commandBuffers[_currentFrame].setScissor(0,
			vk::Rect2D(
				vk::Offset2D(0, 0),
				_swapChainExtent));

		_commandBuffers[_currentFrame].setCullMode(vk::CullModeFlagBits::eBack);
		_commandBuffers[_currentFrame].setFrontFace(vk::FrontFace::eCounterClockwise);
		_commandBuffers[_currentFrame].setDepthTestEnable(vk::True);
		_commandBuffers[_currentFrame].setDepthWriteEnable(vk::True);
		// Reverse Z, use Greater instead
		_commandBuffers[_currentFrame].setDepthCompareOp(vk::CompareOp::eGreater);
		_commandBuffers[_currentFrame].setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);


		PerDrawPC pc{ i, j };

		Mesh mesh = _meshManager.GetMesh(j);

		_commandBuffers[_currentFrame].bindVertexBuffers(0, vk::Buffer(mesh.vertexBuffer.buffer), { 0 });

		_commandBuffers[_currentFrame].bindIndexBuffer(vk::Buffer(mesh.indexBuffer.buffer), 0, vk::IndexType::eUint32);

		_commandBuffers[_currentFrame].bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			_pipelineLayout,
			0,
			*_globalDescriptorSets[_currentFrame],
			nullptr);

		_commandBuffers[_currentFrame].pushConstants<PerDrawPC>(
			_pipelineLayout,
			vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
			0,
			pc
		);

		_commandBuffers[_currentFrame].drawIndexed(static_cast<uint32_t>(mesh.indexCount), 1, 0, 0, 0);

	}

	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *_commandBuffers[_currentFrame]);

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
		std::make_unique<vk::raii::CommandBuffer>(std::move(vk::raii::CommandBuffers(_vkCtx.GetDevice(), allocInfo).front()));

	vk::CommandBufferBeginInfo beginInfo{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
	commandBuffer->begin(beginInfo);

	return commandBuffer;
}

void VulkanRenderer::EndSingleTimeCommands(const vk::raii::CommandBuffer& commandBuffer)
{
	commandBuffer.end();

	vk::SubmitInfo submitInfo{ .commandBufferCount = 1, .pCommandBuffers = &*commandBuffer };
	_vkCtx.GetQueue().submit(submitInfo, nullptr);
	_vkCtx.GetQueue().waitIdle();
}


AllocatedImage VulkanRenderer::CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, 
	vk::SampleCountFlagBits numSamples,
	vk::Format format, 
	vk::ImageTiling tiling, 
	vk::ImageUsageFlags usage,
	VmaMemoryUsage memUsage)
{
	VkImageCreateInfo imageInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = static_cast<VkFormat>(format),
		.extent = { width, height, 1 },
		.mipLevels = mipLevels,
		.arrayLayers = 1,
		.samples = static_cast<VkSampleCountFlagBits>(numSamples),
		.tiling = static_cast<VkImageTiling>(tiling),
		.usage = static_cast<VkImageUsageFlags>(usage),
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED };

	VmaAllocationCreateInfo allocCreateInfo{};
	allocCreateInfo.usage = memUsage;

	AllocatedImage result{};
	if (vmaCreateImage(_vkCtx.GetAllocator(), &imageInfo, &allocCreateInfo,
		&result.image, &result.allocation, nullptr) != VK_SUCCESS)
	{
		throw std::runtime_error("vmaCreateImage failed");
	}
	return result;
}

[[nodiscard]] vk::raii::ImageView VulkanRenderer::CreateImageView(vk::Image image, vk::Format format,
	vk::ImageAspectFlags aspectFlags, uint32_t mipLevels) const
{
	vk::ImageViewCreateInfo viewInfo{ .image = image,
									 .viewType = vk::ImageViewType::e2D,
									 .format = format,
									 .subresourceRange = {aspectFlags, 0, mipLevels, 0, 1} };
	return vk::raii::ImageView(_vkCtx.GetDevice(), viewInfo);
}

void VulkanRenderer::TransitionImageLayout(vk::Image image, vk::ImageLayout oldLayout,
	vk::ImageLayout newLayout, uint32_t mipLevels)
{
	const auto commandBuffer = BeginSingleTimeCommands();

	vk::AccessFlags2 srcAccess{}, dstAccess{};
	vk::PipelineStageFlags2 srcStage{}, dstStage{};

	if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
	{
		srcAccess = {};
		dstAccess = vk::AccessFlagBits2::eTransferWrite;
		srcStage = vk::PipelineStageFlagBits2::eTopOfPipe;
		dstStage = vk::PipelineStageFlagBits2::eAllTransfer;
	}
	else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
	{
		srcAccess = vk::AccessFlagBits2::eTransferWrite;
		dstAccess = vk::AccessFlagBits2::eShaderRead;
		srcStage = vk::PipelineStageFlagBits2::eAllTransfer;
		dstStage = vk::PipelineStageFlagBits2::eFragmentShader;
	}
	else
	{
		throw std::invalid_argument("unsupported layout transition!");
	}

	vk::ImageMemoryBarrier2 barrier{
		.srcStageMask = srcStage,
		.srcAccessMask = srcAccess,
		.dstStageMask = dstStage,
		.dstAccessMask = dstAccess,
		.oldLayout = oldLayout,
		.newLayout = newLayout,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.image = image,
		.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, 1} };

	vk::DependencyInfo dependencyInfo{
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &barrier };

	commandBuffer->pipelineBarrier2(dependencyInfo);
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

void VulkanRenderer::CopyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width,
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

void VulkanRenderer::GenerateMipmaps(vk::Image image, vk::Format imageFormat, int32_t texWidth, int32_t texHeight,
	uint32_t mipLevels)
{
	// Check if image format supports linear blit-ing
	vk::FormatProperties formatProperties = _vkCtx.GetPhysicalDevice().getFormatProperties(imageFormat);

	if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear))
	{
		throw std::runtime_error("texture image format does not support linear blitting!");
	}

	std::unique_ptr<vk::raii::CommandBuffer> commandBuffer = BeginSingleTimeCommands();

	vk::ImageMemoryBarrier2 barrier{
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
		barrier.srcStageMask = vk::PipelineStageFlagBits2::eAllTransfer;
		barrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
		barrier.dstStageMask = vk::PipelineStageFlagBits2::eAllTransfer;
		barrier.dstAccessMask = vk::AccessFlagBits2::eTransferRead;

		commandBuffer->pipelineBarrier2(vk::DependencyInfo{
			.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier });

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
		barrier.srcStageMask = vk::PipelineStageFlagBits2::eAllTransfer;
		barrier.srcAccessMask = vk::AccessFlagBits2::eTransferRead;
		barrier.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;
		barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;

		commandBuffer->pipelineBarrier2(vk::DependencyInfo{
			.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier });

		if (mipWidth > 1)
			mipWidth /= 2;
		if (mipHeight > 1)
			mipHeight /= 2;
	}

	barrier.subresourceRange.baseMipLevel = mipLevels - 1;
	barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
	barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	barrier.srcStageMask = vk::PipelineStageFlagBits2::eAllTransfer;
	barrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
	barrier.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;
	barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;

	commandBuffer->pipelineBarrier2(vk::DependencyInfo{
		.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier });

	EndSingleTimeCommands(*commandBuffer);
}

void VulkanRenderer::CreateDepthResources()
{
	vk::Format depthFormat = FindDepthFormat();

	_depthImage = CreateImage(_swapChainExtent.width, _swapChainExtent.height, 1, _msaaSamples, depthFormat,
		vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment);

	_depthImageView = CreateImageView(_depthImage.image, depthFormat, vk::ImageAspectFlagBits::eDepth, 1);
}

vk::Format VulkanRenderer::FindSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling,
	vk::FormatFeatureFlags features) const
{
	for (const auto format : candidates)
	{
		vk::FormatProperties props = _vkCtx.GetPhysicalDevice().getFormatProperties(format);

		if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features)
			return format;
		if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features)
			return format;
	}

	throw std::runtime_error("Failed to find supported format");
}

[[nodiscard]] vk::Format VulkanRenderer::FindDepthFormat() const
{
	//TODO: Currently using reverse z, which only works with D32Sfloat. Switch based on the supported depth format found?
	return FindSupportedFormat({ vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
		vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

bool VulkanRenderer::HasStencilComponent(vk::Format format) const
{
	return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
}

AllocatedBuffer VulkanRenderer::CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, 
	VmaMemoryUsage memUsage, VmaAllocationCreateFlags allocFlags)
{
	VkBufferCreateInfo bufferInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
								   .size = size,
								   .usage = static_cast<VkBufferUsageFlags>(usage),
								   .sharingMode = VK_SHARING_MODE_EXCLUSIVE };

	VmaAllocationCreateInfo allocCreateInfo{};
	allocCreateInfo.usage = memUsage;
	allocCreateInfo.flags = allocFlags;

	AllocatedBuffer result{};
	if (vmaCreateBuffer(_vkCtx.GetAllocator(), &bufferInfo, &allocCreateInfo,
		&result.buffer, &result.allocation, &result.info) != VK_SUCCESS)
	{
		throw std::runtime_error("vmaCreateBuffer failed");
	}
	return result;
}

void VulkanRenderer::DestroyBuffer(AllocatedBuffer& buffer)
{
	if (buffer.buffer != VK_NULL_HANDLE)
	{
		vmaDestroyBuffer(_vkCtx.GetAllocator(), buffer.buffer, buffer.allocation);
		buffer = {};
	}
}

void VulkanRenderer::DestroyImage(AllocatedImage& image)
{
	if (image.image != VK_NULL_HANDLE)
	{
		vmaDestroyImage(_vkCtx.GetAllocator(), image.image, image.allocation);
		image = {};
	}
}

void VulkanRenderer::CreateUniformBuffers()
{
	auto makePersistentMapped = [&](vk::DeviceSize size,
		vk::BufferUsageFlags usage,
		std::vector<AllocatedBuffer>& buffers)
		{
			buffers.clear();
			buffers.reserve(MAX_FRAMES_IN_FLIGHT);

			for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
			{
				buffers.emplace_back(CreateBuffer(size, usage, VMA_MEMORY_USAGE_AUTO,
					VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT));
			}
		};

	makePersistentMapped(sizeof(CameraUBO), vk::BufferUsageFlagBits::eUniformBuffer, _cameraUBOs);
	makePersistentMapped(sizeof(LightUBO), vk::BufferUsageFlagBits::eUniformBuffer, _lightUBOs);
	makePersistentMapped(sizeof(ObjectSSBO) * MAX_OBJECTS, vk::BufferUsageFlagBits::eStorageBuffer, _objectSSBOs);
	makePersistentMapped(sizeof(MaterialSSBO) * MAX_OBJECTS, vk::BufferUsageFlagBits::eStorageBuffer, _materialSSBOs);

}

void VulkanRenderer::CopyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size)
{
	vk::CommandBufferAllocateInfo allocInfo{
		.commandPool = _commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1 };

	vk::raii::CommandBuffer commandCopyBuffer = std::move(_vkCtx.GetDevice().allocateCommandBuffers(allocInfo).front());

	commandCopyBuffer.begin(vk::CommandBufferBeginInfo{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
	commandCopyBuffer.copyBuffer(srcBuffer, dstBuffer, vk::BufferCopy{ .size = size });
	commandCopyBuffer.end();

	_vkCtx.GetQueue().submit(vk::SubmitInfo{ .commandBufferCount = 1, .pCommandBuffers = &*commandCopyBuffer }, nullptr);
	_vkCtx.GetQueue().waitIdle();
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
		_presentCompleteSemaphores.emplace_back(_vkCtx.GetDevice(), vk::SemaphoreCreateInfo());
		_renderFinishedSemaphores.emplace_back(_vkCtx.GetDevice(), vk::SemaphoreCreateInfo());
	}

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		_inFlightFences.emplace_back(_vkCtx.GetDevice(), vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled });
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

	vk::raii::ShaderModule shaderModule{ _vkCtx.GetDevice(), createInfo };

	return shaderModule;
}

vk::SampleCountFlagBits VulkanRenderer::GetMaxUsableSampleCount() const
{
	vk::PhysicalDeviceProperties physicalDeviceProperties = _vkCtx.GetPhysicalDevice().getProperties();

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

	_colorImage = CreateImage(_swapChainExtent.width, _swapChainExtent.height, 1, _msaaSamples, colorFormat,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment);

	_colorImageView = CreateImageView(_colorImage.image, colorFormat, vk::ImageAspectFlagBits::eColor, 1);
}
