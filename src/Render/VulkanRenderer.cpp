#include "VulkanRenderer.h"
#include "DrawJob.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <filesystem>
#include <chrono>
#include <string>
#include <format>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>


VulkanRenderer::VulkanRenderer()
	: _programCtx(UtahCtx::Get())
{
}

VulkanRenderer::~VulkanRenderer()
{
	if (*_vkCtx.GetDevice() == VK_NULL_HANDLE)
		return;

	_vkCtx.GetDevice().waitIdle();

	//TODO: add a way to unify all image/imageview? also ID by enum type
	for (int i = 0; i < _shadowMapImages.size(); i++)
	{
		_shadowMapImageViews[i] = nullptr;
		DestroyImage(_shadowMapImages[i]);
	}

	for (int i = 0; i < _shadowCubeMapImages.size(); i++)
	{
		_shadowCubeMapImageViews[i] = nullptr;
		DestroyImage(_shadowCubeMapImages[i]);
	}

	_hdrColorImageView = nullptr;
	_depthImageView = nullptr;
	DestroyImage(_hdrColorImage);
	DestroyImage(_depthImage);

	for (size_t i = 0; i < _gBufferColorTargetImageViews.size(); i++)
	{
		_gBufferColorTargetImageViews[i] = nullptr;
		DestroyImage(_gBufferColorTargetImages[i]);
	}

	for (auto& frame : _frames)
		for (auto& buffer : frame.globalBuffers)
			DestroyBuffer(buffer);
}

void VulkanRenderer::Initialize()
{
	RegisterResizeCallback();

	_vkCtx.Initialize(_programCtx.GetContextWindow());

	_pSwapChainCtx = std::make_unique<SwapChainContext>(_vkCtx);

	InitBindingDescs();
	_globalDescriptorSetLayout = std::move(CreateDescriptorSetLayout(bindingDescs));
	CreatePipelineLayouts();
	CreatePipelines();

	CreateCommandPool();
	CreateColorResources();
	CreateDepthResources();
	CreateGBufferImages();

	_textureManger.Initialize(this, &_vkCtx);
	TextureHandle vikingRoomTex = _textureManger.ImportTexture("textures/viking_room.png", "viking_room");

	TextureHandle hemletAlbedoTex = _textureManger.ImportTexture("models/DamagedHelmet/Default_albedo.jpg", "helmet_albedo");
	TextureHandle helmetORMTex = _textureManger.ImportTexture("models/DamagedHelmet/Default_orm.png", "helmet_omr", TextureColorSpace::Linear);
	TextureHandle helmetNormalTex = _textureManger.ImportTexture("models/DamagedHelmet/Default_normal.jpg", "helmet_normal", TextureColorSpace::Linear);
	TextureHandle helmetEmissiveTex = _textureManger.ImportTexture("models/DamagedHelmet/Default_emissive.jpg", "helmet_emissive");

	equirectHandle = _textureManger.ImportTexture("textures/cobblestone_parish_road.hdr", "equirect", TextureColorSpace::HDR, SamplerType::RepeatUClampV);
	cubemapRTHandle = _textureManger.CreateCubemapRenderTarget("cubemap_render_target", 2048);
	convolutionHandle = _textureManger.CreateCubemapRenderTarget("convolution_render_target", 32);
	prefilterHandle = _textureManger.CreateCubemapRenderTargetWithMips("prefilter_render_target", PREFILTER_RESOLUTION, PREFILTER_MIP_LEVELS);
	brdfLUTHandle = _textureManger.Create2DRenderTarget("brdf_lut_render_target", BRDF_LUT_RESOLUTION, BRDF_LUT_RESOLUTION);


	_meshManager.Initialize(this);
	_meshManager.ImportMeshOBJ("models/viking_room.obj", "viking_room");
	_meshManager.ImportMeshOBJ("models/utah_teapot.obj", "teapot");
	_meshManager.ImportMeshGLTF("models/DamagedHelmet/DamagedHelmet.gltf", "helmet");

#if USE_DEAR_IMGUI_INTERFACE
	InitImGUI();
#endif
	CreateShadowMapResources();

	CreateUniformBuffers();
	CreateDescriptorPool();
	CreateDescriptorSets();
	CreateCommandBuffers();

	CreateSyncObjects();
	_materialManager.CreatePBRMaterial("helmet", _pbrPipeline, { hemletAlbedoTex, helmetORMTex, helmetNormalTex, helmetEmissiveTex }); // PBR, tex (damaged helmet)

	_materialManager.CreatePBRMaterial("viking_room", _pbrPipeline, { vikingRoomTex });
	_materialManager.CreatePBRMaterial("pure_green", _pbrPipeline, {}, glm::vec4(0.2f, 0.9f, 0.2f, 1.0f)); // no tex green color
	_materialManager.CreatePBRMaterial("pure_white", _pbrPipeline, {}, glm::vec4(0.9f, 0.9f, 0.9f, 1.0f)); // no tex white color
	_debugAABBMaterial = _materialManager.CreateUnlitMaterial("debug_wireframe_yellow", _debugWireframePipeline, glm::vec4(0.9f, 0.9f, 0.2f, 1.0f)); // Debug Wireframe, Yellow (For AABB)
	_debugLightMaterial = _materialManager.CreateUnlitMaterial("debug_wireframe_blue", _debugWireframePipeline, glm::vec4(0.2f, 0.2f, 0.9f, 1.0f)); // Debug Wireframe, Blue (For point lights)

	auto matGPUs = _materialManager.ConvertMaterialsToGPU();

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		MaterialSSBO* materials = static_cast<MaterialSSBO*>(_frames[i].Mapped(GlobalBinding::MaterialSSBO));
		for (size_t j = 0; j < matGPUs.size(); ++j)
		{
			materials[j].baseColorFactor = matGPUs[j].baseColorFactor;
			materials[j].ormFactor = matGPUs[j].ormFactor;
			materials[j].emissiveFactor = matGPUs[j].emissiveFactor;
			materials[j].normalScale = matGPUs[j].normalScale;

			memcpy(materials[j].texIndices, matGPUs[j].texIndices, sizeof(uint32_t) * 4);
			memcpy(materials[j].samplerIndices, matGPUs[j].samplerIndices, sizeof(uint32_t) * 4);
		}
	}

	ConvertEquirectToCubeMap();
	ConvolveIrradianceMap();
	PrefilterEnvironmentMap();
	BakeBRDFLUT();
}

void VulkanRenderer::SetDrawList(std::vector<struct DrawJob>&& list)
{
	_drawList = std::move(list);
}

void VulkanRenderer::SetDebugAABBDrawList(std::vector<glm::mat4>&& list)
{
	_debugAABBDrawList = std::move(list);
}

void VulkanRenderer::SetCameraComponent(const CameraComponent& camera)
{
	_mainCam = camera;
}

void VulkanRenderer::SetLights(std::vector<PointLightGPU>&& pointLights, std::vector<DirectionalLightGPU>&& dirLights
	, std::vector<SpotLightGPU>&& spotLights)
{
	_pointLights = std::move(pointLights);
	_dirLights = std::move(dirLights);
	_spotLights = std::move(spotLights);
}

void VulkanRenderer::DrawFrame()
{
	if (_programCtx.GetRequestShaderReload())
	{
		HotReloadShaders();

		_programCtx.SetRequestShaderReload(false);
	}


	try
	{
		FrameData& frame = _frames[_currentFrame];

		// Wait previous frame to finish (block execution)
		while (vk::Result::eTimeout == _vkCtx.GetDevice().waitForFences(*frame.inFlightFence, vk::True, UINT64_MAX))
			;

		// Acquire image from the swap chain
		auto [result, imageIndex] = _pSwapChainCtx->AcquireNextImage(UINT64_MAX, *frame.presentCompleteSemaphore, nullptr);

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

		_vkCtx.GetDevice().resetFences(*frame.inFlightFence);

		frame.commandBuffer.reset();
		//RecordCommandBufferShadowMapView(imageIndex);
		//RecordCommandBuffer(imageIndex);
		RecordCommandBufferDeferredRendering(imageIndex);

		vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
		const vk::SubmitInfo   submitInfo{ .waitSemaphoreCount = 1,
										  .pWaitSemaphores = &*frame.presentCompleteSemaphore,
										  .pWaitDstStageMask = &waitDestinationStageMask,
										  .commandBufferCount = 1,
										  .pCommandBuffers = &*frame.commandBuffer,
										  .signalSemaphoreCount = 1,
										  .pSignalSemaphores = &*_renderFinishedSemaphores[imageIndex] };
		_vkCtx.GetQueue().submit(submitInfo, *frame.inFlightFence);

		try
		{
			result = _pSwapChainCtx->Present(_vkCtx.GetQueue(), *_renderFinishedSemaphores[imageIndex], imageIndex);

			if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || _framebufferResized)
			{
				_framebufferResized = false;
				RecreateSwapChain();
			}
			else if (result != vk::Result::eSuccess)
			{
				throw std::runtime_error("Failed to present swap chain image");
			}

			if (_programCtx.GetRequestScreenshot())
			{
				SaveScreenshot();

				_programCtx.SetRequestScreenshot(false);
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
		_currentFrame = (_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}
	catch (const vk::DeviceLostError&)
	{
#if USE_NSIGHT_AFTERMATH
		_vkCtx.WaitForCrashDump();
#endif
		throw;
	}
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
	VkFormat colorFormat = static_cast<VkFormat>(_pSwapChainCtx->GetSurfaceFormat().format);
	VkFormat depthFormat = VK_FORMAT_UNDEFINED;

	ImGui_ImplVulkan_InitInfo initInfo{};
	initInfo.ApiVersion = VK_API_VERSION_1_3;
	initInfo.Instance = *_vkCtx.GetInstance();
	initInfo.PhysicalDevice = *_vkCtx.GetPhysicalDevice();
	initInfo.Device = *_vkCtx.GetDevice();
	initInfo.QueueFamily = _vkCtx.GetQueueFamilyIndex();
	initInfo.Queue = *_vkCtx.GetQueue();
	initInfo.DescriptorPool = *_imguiDescriptorPool;
	initInfo.MinImageCount = 2;
	initInfo.ImageCount = static_cast<uint32_t>(_pSwapChainCtx->GetSwapChainImagesSize());
	initInfo.PipelineCache = VK_NULL_HANDLE;

	initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
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
	FrameData& frame = _frames[currentImage];

	static auto startTime = std::chrono::high_resolution_clock::now();

	auto  currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	const auto swapChainExtent = _pSwapChainCtx->GetExtent();

	CameraUBO camUBO{};
	camUBO.view = glm::lookAt(_mainCam._pos, _mainCam._pos + _mainCam.GetFrontVector(), glm::vec3(0.0f, 1.0f, 0.0f));
	// Swap far and near plane input -> Reverse Z for greater precision
	camUBO.proj =
		glm::perspective(glm::radians(_mainCam._verticalFOV), swapChainExtent.width / (float)swapChainExtent.height, _mainCam._farPlane, _mainCam._nearPlane);
	// Flip y axis since GLM's was inverted
	camUBO.proj[1][1] *= -1;
	camUBO.eyePos = _mainCam._pos;
	camUBO.nearPlane = _mainCam._nearPlane;
	camUBO.farPlane = _mainCam._farPlane;
	memcpy(frame.Mapped(GlobalBinding::CameraUBO), &camUBO, sizeof(camUBO));

	LightUBO lightUBO{};
	lightUBO.dirLightNum = std::min(static_cast<uint32_t>(_dirLights.size()), MAX_DIR_LIGHTS);
	std::memcpy(lightUBO.dirLights, _dirLights.data(),
		std::min(_dirLights.size(), size_t(MAX_DIR_LIGHTS)) * sizeof(DirectionalLightGPU));

	lightUBO.spotLightNum = std::min(static_cast<uint32_t>(_spotLights.size()), MAX_SPOT_LIGHTS);
	std::memcpy(lightUBO.spotLights, _spotLights.data(),
		std::min(_spotLights.size(), size_t(MAX_SPOT_LIGHTS)) * sizeof(SpotLightGPU));

	lightUBO.pointLightNum = std::min(static_cast<uint32_t>(_pointLights.size()), MAX_POINT_LIGHTS);
	std::memcpy(lightUBO.pointLights, _pointLights.data(),
		std::min(_pointLights.size(), size_t(MAX_POINT_LIGHTS)) * sizeof(PointLightGPU));

	memcpy(frame.Mapped(GlobalBinding::LightUBO), &lightUBO, sizeof(lightUBO));


	size_t currentOffset = 0;

	ObjectSSBO* objects = static_cast<ObjectSSBO*>(frame.Mapped(GlobalBinding::ObjectSSBO));
	for (size_t i = 0; i < _drawList.size(); ++i)
	{
		objects[i].model = _drawList[i]._model;
	}

	currentOffset += _drawList.size();
	//TODO: Group this with a DebugDraw struct

	// AABB Bounding boxes
	for (size_t i = 0; i < _debugAABBDrawList.size(); ++i)
	{
		objects[i + currentOffset].model = _debugAABBDrawList[i];
	}

	currentOffset += _debugAABBDrawList.size();

	for (size_t i = 0; i < _dirLights.size(); i++)
	{
		glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 3.0f, 0.0f));
		glm::vec3 dir = glm::normalize(_dirLights[i].direction);

		model *= glm::mat4_cast(glm::rotation(glm::vec3(0.0f, -1.0f, 0.0f), dir));

		objects[i + currentOffset].model = model;
	}

	currentOffset += _dirLights.size();

	for (size_t i = 0; i < _spotLights.size(); i++)
	{
		glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 3.0f, 0.0f));
		glm::vec3 dir = glm::normalize(_spotLights[i].direction);

		model *= glm::mat4_cast(glm::rotation(glm::vec3(0.0f, -1.0f, 0.0f), dir));

		objects[i + currentOffset].model = model;
	}

	currentOffset += _spotLights.size();

	for (size_t i = 0; i < _pointLights.size(); i++)
	{
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, _pointLights[i].position);
		objects[i + currentOffset].model = model;
	}

	// Shadow Map
	// Reverse Z, flip near and far plane

	std::vector<glm::mat4> viewProjMatrices;
	viewProjMatrices.reserve(_shadowMapImages.size() + _pointLights.size() * 6);

	for (size_t i = 0; i < _dirLights.size(); i++)
	{
		glm::mat4 orthoProj = glm::ortho(-20.0f, 20.0f, -20.0f, 20.0f, _mainCam._farPlane, _mainCam._nearPlane);
		glm::vec3 lightEye = glm::vec3(0.0f) - glm::normalize(_dirLights[i].direction) * 2.0f;
		glm::mat4 view = glm::lookAt(lightEye, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

		viewProjMatrices.emplace_back(orthoProj * view);
	}

	for (size_t i = 0; i < _spotLights.size(); i++)
	{
		float cosOuter = _spotLights[i].outerCutoff;          // GPU struct stores cos(radians(outerCutoff))
		float fov = 2.0f * acosf(glm::clamp(cosOuter, -1.f, 1.f)) * 1.1f;  // full cone + slight pad
		glm::mat4 proj = glm::perspective(fov, 1.0f, _mainCam._farPlane, _mainCam._nearPlane); // reverse-Z (near/far swapped), aspect 1

		glm::vec3 eye = _spotLights[i].position;
		glm::vec3 dir = glm::normalize(_spotLights[i].direction);
		glm::vec3 up = (fabs(dir.y) > 0.99f) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
		glm::mat4 view = glm::lookAt(eye, eye + dir, up);

		viewProjMatrices.emplace_back(proj * view);
	}

	// pad unused 2d slots
	viewProjMatrices.resize(std::max(viewProjMatrices.size(), _shadowMapImages.size()));

	for (size_t i = 0; i < _pointLights.size(); i++)
	{
		glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, _mainCam._farPlane, _mainCam._nearPlane);

		// +X
		glm::vec3 eye = _pointLights[i].position;
		glm::vec3 dir = glm::vec3(1.0f, 0.0f, 0.0f);
		glm::vec3 up = glm::vec3(0.0f, -1.0f, 0.0f);
		glm::mat4 view = glm::lookAt(eye, eye + dir, up);
		viewProjMatrices.emplace_back(proj * view);

		// -X
		dir = glm::vec3(-1.0f, 0.0f, 0.0f);
		view = glm::lookAt(eye, eye + dir, up);
		viewProjMatrices.emplace_back(proj * view);

		// +Y
		dir = glm::vec3(0.0f, 1.0f, 0.0f);
		up = glm::vec3(0.0f, 0.0f, 1.0f);
		view = glm::lookAt(eye, eye + dir, up);
		viewProjMatrices.emplace_back(proj * view);

		// -Y
		dir = glm::vec3(0.0f, -1.0f, 0.0f);
		up = glm::vec3(0.0f, 0.0f, -1.0f);
		view = glm::lookAt(eye, eye + dir, up);
		viewProjMatrices.emplace_back(proj * view);

		// +Z
		dir = glm::vec3(0.0f, 0.0f, 1.0f);
		up = glm::vec3(0.0f, -1.0f, 0.0f);
		view = glm::lookAt(eye, eye + dir, up);
		viewProjMatrices.emplace_back(proj * view);

		// -Z
		dir = glm::vec3(0.0f, 0.0f, -1.0f);
		up = glm::vec3(0.0f, -1.0f, 0.0f);
		view = glm::lookAt(eye, eye + dir, up);
		viewProjMatrices.emplace_back(proj * view);
	}

	memcpy(frame.Mapped(GlobalBinding::ShadowMapUBO), viewProjMatrices.data(), viewProjMatrices.size() * sizeof(glm::mat4));

	//TODO: No need to update this per frame probably
	SceneIBLUBO sceneIBL
	{   
		.ambientColor = glm::vec3(0.0f),		// intensity 1 + ambient 0 for baked IBL; vice versa for no IBL
		.intensity = 1.0f,	
		.irradianceIndex = convolutionHandle.index,
		.prefilteredIndex = prefilterHandle.index,
		.brdfLUTIndex = brdfLUTHandle.index,
		.prefilteredMaxMip = PREFILTER_MIP_LEVELS - 1,
	};

	memcpy(frame.Mapped(GlobalBinding::SceneIBLUBO), &sceneIBL, sizeof(sceneIBL));
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

void VulkanRenderer::CleanupSwapChain()
{
	// Image views must be destroyed before the underlying VMA-allocated images.
	_hdrColorImageView = nullptr;
	_depthImageView = nullptr;
	DestroyImage(_hdrColorImage);
	DestroyImage(_depthImage);
}

//TODO: Doesnt work, will cause device lost on resize
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

	vk::SwapchainKHR oldHandle = _pSwapChainCtx ? _pSwapChainCtx->GetHandle() : VK_NULL_HANDLE;
	auto newCtx = std::make_unique<SwapChainContext>(_vkCtx, oldHandle);
	_pSwapChainCtx = std::move(newCtx);

	_renderFinishedSemaphores.clear();
	for (size_t i = 0; i < _pSwapChainCtx->GetSwapChainImagesSize(); i++)
		_renderFinishedSemaphores.emplace_back(_vkCtx.GetDevice(), vk::SemaphoreCreateInfo());

	_currentFrame = 0;

	CreateColorResources();
	CreateDepthResources();
	// No need to recreate shadow resources since it's window independent
	//CreateShadowMapResources();

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		FrameData& frame = _frames[i];
		std::vector<vk::WriteDescriptorSet> writes;
		std::vector<vk::DescriptorImageInfo> infos;
		infos.reserve(16);

		auto pushImage = [&](GlobalBinding b, vk::ImageView view) {
			infos.push_back({ .imageView = view,
							  .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal });
			writes.push_back({ .dstSet = frame.globalDescriptorSet,
							   .dstBinding = ToIdx(b),
							   .dstArrayElement = 0,
							   .descriptorCount = 1,
							   .descriptorType = vk::DescriptorType::eSampledImage,
							   .pImageInfo = &infos.back() });
			};

		pushImage(GlobalBinding::HDROutput, _hdrColorImageView);
		// shadow maps & shadow cubes: loop pushImage for each into ShadowMaps / ShadowCubeMaps

		_vkCtx.GetDevice().updateDescriptorSets(writes, {});
	}
}

void VulkanRenderer::InitBindingDescs()
{
	bindingDescs.reserve(static_cast<size_t>(GlobalBinding::Count));

	// 0: Camera UBO
	bindingDescs.push_back(BindingDesc{ .bindingIndex = ToIdx(GlobalBinding::CameraUBO),
										.type = vk::DescriptorType::eUniformBuffer,
										.count = 1,
										.bufferSize = sizeof(CameraUBO),
										.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
										.bindingFlags = {} });
	// 1: Light UBO
	bindingDescs.push_back(BindingDesc{ .bindingIndex = ToIdx(GlobalBinding::LightUBO),
										.type = vk::DescriptorType::eUniformBuffer,
										.count = 1,
										.bufferSize = sizeof(LightUBO),
										.stageFlags = vk::ShaderStageFlagBits::eFragment,
										.bindingFlags = {} });
	// 2: Object SSBO
	bindingDescs.push_back(BindingDesc{ .bindingIndex = ToIdx(GlobalBinding::ObjectSSBO),
										.type = vk::DescriptorType::eStorageBuffer,
										.count = 1,
										.bufferSize = sizeof(ObjectSSBO) * MAX_OBJECTS,
										.stageFlags = vk::ShaderStageFlagBits::eVertex,
										.bindingFlags = {} });
	// 3: Material SSBO
	bindingDescs.push_back(BindingDesc{ .bindingIndex = ToIdx(GlobalBinding::MaterialSSBO),
										.type = vk::DescriptorType::eStorageBuffer,
										.count = 1,
										.bufferSize = sizeof(MaterialSSBO) * MAX_OBJECTS,
										.stageFlags = vk::ShaderStageFlagBits::eFragment,
										.bindingFlags = {} });
	// 4: Bindless texture array
	bindingDescs.push_back(BindingDesc{ .bindingIndex = ToIdx(GlobalBinding::Textures),
										.type = vk::DescriptorType::eSampledImage,
										.count = MAX_TEXTURES,
										.stageFlags = vk::ShaderStageFlagBits::eFragment,
										.bindingFlags = vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind });
	// 5: Texture sampler array
	bindingDescs.push_back(BindingDesc{ .bindingIndex = ToIdx(GlobalBinding::TextureSamplers),
										.type = vk::DescriptorType::eSampler, //TODO: Check out pImmutableSamplers
										.count = MAX_TEXTURE_SAMPLERS,
										.stageFlags = vk::ShaderStageFlagBits::eFragment,
										.bindingFlags = vk::DescriptorBindingFlagBits::ePartiallyBound });
	// 6: Shadow map UBO
	bindingDescs.push_back(BindingDesc{ .bindingIndex = ToIdx(GlobalBinding::ShadowMapUBO),
										.type = vk::DescriptorType::eUniformBuffer,
										.count = 1,
										.bufferSize = sizeof(glm::mat4) * MAX_SHADOW_CASTER_LIGHTS,
										.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
										.bindingFlags = {} });
	// 7: Shadow map texture array
	bindingDescs.push_back(BindingDesc{ .bindingIndex = ToIdx(GlobalBinding::ShadowMaps),
										.type = vk::DescriptorType::eSampledImage,
										.count = MAX_SHADOW_CASTER_LIGHTS,
										.stageFlags = vk::ShaderStageFlagBits::eFragment,
										.bindingFlags = vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind });
	// 8: Shadow map texture sampler
	bindingDescs.push_back(BindingDesc{ .bindingIndex = ToIdx(GlobalBinding::ShadowMapSampler),
										.type = vk::DescriptorType::eSampler,
										.count = 1,
										.stageFlags = vk::ShaderStageFlagBits::eFragment,
										.bindingFlags = {} });

	// 9: Shadow cube map texture
	bindingDescs.push_back(BindingDesc{ .bindingIndex = ToIdx(GlobalBinding::ShadowCubeMaps),
										.type = vk::DescriptorType::eSampledImage,
										.count = MAX_SHADOW_CASTER_POINT_LIGHTS,
										.stageFlags = vk::ShaderStageFlagBits::eFragment,
										.bindingFlags = vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind });
	// 10: hdr intermediate texture
	bindingDescs.push_back(BindingDesc{ .bindingIndex = ToIdx(GlobalBinding::HDROutput),
										.type = vk::DescriptorType::eSampledImage,
										.count = 1,
										.stageFlags = vk::ShaderStageFlagBits::eFragment,
										.bindingFlags = {} });
	// 11: skybox cubemap
	bindingDescs.push_back(BindingDesc{ .bindingIndex = ToIdx(GlobalBinding::SkyboxCubemap),
										.type = vk::DescriptorType::eSampledImage,
										.count = 1,
										.stageFlags = vk::ShaderStageFlagBits::eFragment,
										.bindingFlags = vk::DescriptorBindingFlagBits::eUpdateAfterBind });
	// 12: Scene IBL UBO
	bindingDescs.push_back(BindingDesc{ .bindingIndex = ToIdx(GlobalBinding::SceneIBLUBO),
										.type = vk::DescriptorType::eUniformBuffer,
										.count = 1,
										.bufferSize = sizeof(SceneIBLUBO),
										.stageFlags = vk::ShaderStageFlagBits::eFragment,
										.bindingFlags = {} }); //TODO: Should I use update after bind here? If it does than camera needs it too
	// 13: G Buffer Color Targets
	bindingDescs.push_back(BindingDesc{ .bindingIndex = ToIdx(GlobalBinding::GBufferColorTargets),
										.type = vk::DescriptorType::eSampledImage,
										.count = static_cast<uint32_t>(GBufferColorTargetType::Count),
										.stageFlags = vk::ShaderStageFlagBits::eFragment,
										.bindingFlags = vk::DescriptorBindingFlagBits::eUpdateAfterBind });
	// 14: G Buffer Depth Target
	bindingDescs.push_back(BindingDesc{ .bindingIndex = ToIdx(GlobalBinding::DepthTarget),
										.type = vk::DescriptorType::eSampledImage,
										.count = 1,
										.stageFlags = vk::ShaderStageFlagBits::eFragment,
										.bindingFlags = vk::DescriptorBindingFlagBits::eUpdateAfterBind });
}

[[nodiscard]] vk::raii::DescriptorSetLayout VulkanRenderer::CreateDescriptorSetLayout(const std::vector<BindingDesc>& descs)
{
	std::vector<vk::DescriptorSetLayoutBinding> bindings;
	std::vector<vk::DescriptorBindingFlags> bindingFlags;

	bindings.reserve(descs.size());
	bindingFlags.reserve(descs.size());

	bool anyUpdateAfterBind = false;

	for (size_t i = 0; i < descs.size(); i++)
	{
		bindings.push_back(vk::DescriptorSetLayoutBinding
			{
				.binding = descs[i].bindingIndex,
				.descriptorType = descs[i].type,
				.descriptorCount = descs[i].count,
				.stageFlags = descs[i].stageFlags
			});

		bindingFlags.push_back(vk::DescriptorBindingFlags
			{
				descs[i].bindingFlags
			});

		if (descs[i].bindingFlags & vk::DescriptorBindingFlagBits::eUpdateAfterBind)
			anyUpdateAfterBind = true;
	}

	vk::DescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{
		.bindingCount = static_cast<uint32_t>(bindingFlags.size()),
		.pBindingFlags = bindingFlags.data()
	};

	vk::DescriptorSetLayoutCreateInfo info{
		.pNext = &flagsInfo,
		.flags = anyUpdateAfterBind ? vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool
									: vk::DescriptorSetLayoutCreateFlags{},
		.bindingCount = static_cast<uint32_t>(bindings.size()),
		.pBindings = bindings.data()
	};

	return vk::raii::DescriptorSetLayout(_vkCtx.GetDevice(), info);

}

void VulkanRenderer::CreateDescriptorPool()
{
	std::unordered_map<vk::DescriptorType, uint32_t> descTypeCounts;

	for (const auto& binding : bindingDescs)
	{
		descTypeCounts[binding.type] += binding.count;
	}


	std::vector<vk::DescriptorPoolSize> poolSize;
	poolSize.reserve(descTypeCounts.size());

	for (const auto& typeCount : descTypeCounts)
	{
		poolSize.emplace_back(typeCount.first, typeCount.second * MAX_FRAMES_IN_FLIGHT);
	}

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

	auto sets = _vkCtx.GetDevice().allocateDescriptorSets(allocInfo);
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		_frames[i].globalDescriptorSet = std::move(sets[i]);


	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		FrameData& frame = _frames[i];

		std::vector<vk::WriteDescriptorSet> writes;
		writes.reserve(bindingDescs.size());

		// buffer info using binding descs
		std::vector<vk::DescriptorBufferInfo> bufferInfos;
		bufferInfos.reserve(bindingDescs.size());

		for (const auto& desc : bindingDescs)
		{
			// image/sampler, skip
			if (desc.bufferSize == 0)
				continue;

			bufferInfos.push_back(vk::DescriptorBufferInfo{
				.buffer = frame.globalBuffers[desc.bindingIndex].buffer,
				.offset = 0,
				.range = desc.bufferSize
				});

			writes.push_back(vk::WriteDescriptorSet{
				.dstSet = frame.globalDescriptorSet,
				.dstBinding = desc.bindingIndex,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = desc.type,
				.pBufferInfo = &bufferInfos.back()
				});
		}

		// image info explicitly defined & partially bound, only write populated slots
		size_t texCount = _textureManger.GetTexturesCount();
		std::vector<vk::DescriptorImageInfo> textureInfos;
		textureInfos.reserve(texCount);
		for (size_t i = 0; i < texCount; i++)
		{
			// skip sampler field, defined in separate image info
			textureInfos.push_back(vk::DescriptorImageInfo{
											.imageView = _textureManger.GetTextureImageView(i),
											.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal });
		}

		size_t samplerCount = _textureManger.GetTextureSamplersCount();
		std::vector<vk::DescriptorImageInfo> samplerInfos;
		samplerInfos.reserve(samplerCount);
		for (size_t i = 0; i < samplerCount; i++)
		{
			samplerInfos.push_back(vk::DescriptorImageInfo{
										.sampler = _textureManger.GetTextureSampler(i),
				});
		}

		size_t shadowMapCount = _shadowMapImages.size();
		std::vector<vk::DescriptorImageInfo> shadowMapInfos;
		shadowMapInfos.reserve(shadowMapCount);
		for (size_t i = 0; i < shadowMapCount; i++)
		{
			// skip sampler field, defined in separate image info
			shadowMapInfos.push_back(vk::DescriptorImageInfo{
											.imageView = _shadowMapImageViews[i],
											.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal });
		}

		vk::DescriptorImageInfo shadowMapSamplerInfo = vk::DescriptorImageInfo{
										.sampler = _shadowMapSampler, };

		size_t shadowCubeMapCount = _shadowCubeMapImages.size();
		std::vector<vk::DescriptorImageInfo> shadowCubeMapInfos;
		shadowCubeMapInfos.reserve(shadowCubeMapCount);
		for (size_t i = 0; i < shadowCubeMapCount; i++)
		{
			// skip sampler field, defined in separate image info
			shadowCubeMapInfos.push_back(vk::DescriptorImageInfo{
											.imageView = _shadowCubeMapImageViews[i],
											.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal });
		}

		vk::DescriptorImageInfo hdrImageInfo = vk::DescriptorImageInfo{
											.imageView = _hdrColorImageView,
											.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal };

		vk::DescriptorImageInfo skyboxCubemapInfo = vk::DescriptorImageInfo{
											.imageView = _textureManger.GetTextureImageView(cubemapRTHandle.index),
											.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal };


		size_t gBufferColorTargetCount = _gBufferColorTargetImageViews.size();
		std::vector<vk::DescriptorImageInfo> gBufferColorTargetInfos;
		gBufferColorTargetInfos.reserve(gBufferColorTargetCount);
		for (size_t i = 0; i < gBufferColorTargetCount; i++)
		{
			// skip sampler field, defined in separate image info
			gBufferColorTargetInfos.push_back(vk::DescriptorImageInfo{
											.imageView = _gBufferColorTargetImageViews[i],
											.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal });
		}

		vk::DescriptorImageInfo depthTargetInfo = vk::DescriptorImageInfo{
											.imageView = _depthImageView,
											.imageLayout = vk::ImageLayout::eDepthReadOnlyOptimal };


		writes.push_back(vk::WriteDescriptorSet{
				.dstSet = frame.globalDescriptorSet,
				.dstBinding = ToIdx(GlobalBinding::Textures),
				.dstArrayElement = 0,
				.descriptorCount = static_cast<uint32_t>(texCount),
				.descriptorType = vk::DescriptorType::eSampledImage,
				.pImageInfo = textureInfos.data()
			});
		writes.push_back(vk::WriteDescriptorSet{
				.dstSet = frame.globalDescriptorSet,
				.dstBinding = ToIdx(GlobalBinding::TextureSamplers),
				.dstArrayElement = 0,
				.descriptorCount = static_cast<uint32_t>(samplerCount),
				.descriptorType = vk::DescriptorType::eSampler,
				.pImageInfo = samplerInfos.data()
			});
		writes.push_back(vk::WriteDescriptorSet{
				.dstSet = frame.globalDescriptorSet,
				.dstBinding = ToIdx(GlobalBinding::ShadowMaps),
				.dstArrayElement = 0,
				.descriptorCount = static_cast<uint32_t>(shadowMapCount),
				.descriptorType = vk::DescriptorType::eSampledImage,
				.pImageInfo = shadowMapInfos.data()
			});
		writes.push_back(vk::WriteDescriptorSet{
				.dstSet = frame.globalDescriptorSet,
				.dstBinding = ToIdx(GlobalBinding::ShadowMapSampler),
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eSampler,
				.pImageInfo = &shadowMapSamplerInfo
			});
		writes.push_back(vk::WriteDescriptorSet{
				.dstSet = frame.globalDescriptorSet,
				.dstBinding = ToIdx(GlobalBinding::ShadowCubeMaps),
				.dstArrayElement = 0,
				.descriptorCount = static_cast<uint32_t>(shadowCubeMapCount),
				.descriptorType = vk::DescriptorType::eSampledImage,
				.pImageInfo = shadowCubeMapInfos.data()
			});

		writes.push_back(vk::WriteDescriptorSet{
				.dstSet = frame.globalDescriptorSet,
				.dstBinding = ToIdx(GlobalBinding::HDROutput),
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eSampledImage,
				.pImageInfo = &hdrImageInfo
			});

		writes.push_back(vk::WriteDescriptorSet{
				.dstSet = frame.globalDescriptorSet,
				.dstBinding = ToIdx(GlobalBinding::SkyboxCubemap),
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eSampledImage,
				.pImageInfo = &skyboxCubemapInfo
			});

		writes.push_back(vk::WriteDescriptorSet{
				.dstSet = frame.globalDescriptorSet,
				.dstBinding = ToIdx(GlobalBinding::GBufferColorTargets),
				.dstArrayElement = 0,
				.descriptorCount = static_cast<uint32_t>(gBufferColorTargetCount),
				.descriptorType = vk::DescriptorType::eSampledImage,
				.pImageInfo = gBufferColorTargetInfos.data()
			});

		writes.push_back(vk::WriteDescriptorSet{
				.dstSet = frame.globalDescriptorSet,
				.dstBinding = ToIdx(GlobalBinding::DepthTarget),
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eSampledImage,
				.pImageInfo = &depthTargetInfo
			});

		_vkCtx.GetDevice().updateDescriptorSets(writes, {});
	}
}

void VulkanRenderer::CreatePipelineLayouts()
{
	vk::PushConstantRange pushRange{
		.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
		.offset = 0,
		.size = sizeof(uint32_t) * 3   // PerDrawPC: objectIndex, materialIndex, roughness for prefilter
	};

	vk::PipelineLayoutCreateInfo info{
		.setLayoutCount = 1,
		.pSetLayouts = &*_globalDescriptorSetLayout,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &pushRange };

	_globalPipelineLayout = vk::raii::PipelineLayout(_vkCtx.GetDevice(), info);
}

void VulkanRenderer::CreatePipelines()
{
	_pipelines.clear();

	_pbrPipeline = CreateGraphicsPipeline("shaderBin/static_mesh_vert.spv", "shaderBin/forward_pbr_frag.spv", *_globalPipelineLayout);
	_debugPipeline = CreateGraphicsPipeline("shaderBin/unlit_vert.spv", "shaderBin/unlit_frag.spv", *_globalPipelineLayout, PipelineType::Debug);
	_debugWireframePipeline = CreateGraphicsPipeline("shaderBin/unlit_vert.spv", "shaderBin/unlit_frag.spv", *_globalPipelineLayout, PipelineType::DebugWireframe);
	_shadowPipelineIndex = CreateShadowMapGraphicsPipeline("shaderBin/shadow_vert.spv", *_globalPipelineLayout);
	_shadowCubeMapPipelineIndex = CreateShadowCubeMapGraphicsPipeline("shaderBin/shadow_vert.spv", *_globalPipelineLayout);
	//TODO: actually has no use for push constant (at least currently), but reuse same layout just for simlicity
	_hdrOutputPipelineIndex = CreateHDRGraphicsPipeline("shaderBin/fullscreen_vert.spv", "shaderBin/tonemap_pbr_neutral_frag.spv", *_globalPipelineLayout);
	_skyboxPipelineIndex = CreateGraphicsPipeline("shaderBin/skybox_vert.spv", "shaderBin/skybox_frag.spv", *_globalPipelineLayout, PipelineType::Debug);
	_equirectToCubePipelineIndex = CreateEquirectToCubePipeline("shaderBin/ibl_equirect_to_cube_vert.spv", "shaderBin/ibl_equirect_to_cube_frag.spv", *_globalPipelineLayout);

	_convolutionPipelineIndex = CreateEquirectToCubePipeline("shaderBin/ibl_equirect_to_cube_vert.spv", "shaderBin/ibl_irradiance_convolve_frag.spv", *_globalPipelineLayout);
	_prefilterPipelineIndex = CreateEquirectToCubePipeline("shaderBin/ibl_equirect_to_cube_vert.spv", "shaderBin/ibl_prefilter_specular_frag.spv", *_globalPipelineLayout);
	_brdfLutPipelineIndex = CreateBRDFLUTPipeline("shaderBin/fullscreen_vert.spv", "shaderBin/ibl_brdf_lut_frag.spv", *_globalPipelineLayout);

	_deferredGBufferPipelineIndex = CreateDeferredGBufferPipeline("shaderBin/static_mesh_vert.spv", "shaderBin/deferred_gbuffer_frag.spv", *_globalPipelineLayout);
	_deferredLightingPipelineIndex = CreateDeferredLightingPipeline("shaderBin/fullscreen_vert.spv", "shaderBin/deferred_lighting_pbr_frag.spv", *_globalPipelineLayout);
}

uint32_t VulkanRenderer::CreateGraphicsPipeline(const std::string& vertPath, const std::string& fragPath, vk::PipelineLayout layout, PipelineType type)
{
	vk::raii::ShaderModule vertModule = CreateShaderModule(ReadFile(vertPath));
	vk::raii::ShaderModule fragModule = CreateShaderModule(ReadFile(fragPath));


	vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
		.stage = vk::ShaderStageFlagBits::eVertex, .module = vertModule, .pName = "main" };
	vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
		.stage = vk::ShaderStageFlagBits::eFragment, .module = fragModule, .pName = "main" };
	vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	auto bindingDescription = Vertex::GetBindingDescription();
	std::vector< vk::VertexInputAttributeDescription> attributeDescriptions;

	// Position only
	if (type == PipelineType::Debug || type == PipelineType::DebugWireframe)
	{
		bindingDescription = vk::VertexInputBindingDescription{ 0, sizeof(glm::vec3), vk::VertexInputRate::eVertex };
		attributeDescriptions = {
			vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, 0)
		};
	}
	else
	{
		auto arr = Vertex::GetAttributeDescriptions();
		attributeDescriptions.assign(arr.begin(), arr.end());
	}

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
														.depthBiasEnable = vk::False,
														.lineWidth = 1.0f };

	if (type == PipelineType::DebugWireframe)
	{
		inputAssembly.topology = vk::PrimitiveTopology::eLineList;
		rasterizer.polygonMode = vk::PolygonMode::eLine;
	}

	vk::PipelineMultisampleStateCreateInfo  multisampling{ .rasterizationSamples = vk::SampleCountFlagBits::e1,
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
		vk::DynamicState::eDepthCompareOp };

	vk::PipelineDynamicStateCreateInfo dynamicState{ .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
													.pDynamicStates = dynamicStates.data() };

	vk::Format depthFormat = FindDepthFormat();
	vk::Format hdrFormat = vk::Format::eR16G16B16A16Sfloat;

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
		 .layout = layout,
		 .renderPass = nullptr},
		{.colorAttachmentCount = 1,
		 .pColorAttachmentFormats = &hdrFormat,
		 .depthAttachmentFormat = depthFormat} };


	_pipelines.push_back(PipelineEntry{
		.pipeline = vk::raii::Pipeline(_vkCtx.GetDevice(), nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>()),
		.layout = layout
		});

	return static_cast<uint32_t>(_pipelines.size() - 1);
}


//TODO: combine this into the default creation function, if fragPath is null -> no frag module
uint32_t VulkanRenderer::CreateShadowMapGraphicsPipeline(const std::string& vertPath, vk::PipelineLayout layout)
{
	vk::raii::ShaderModule vertModule = CreateShaderModule(ReadFile(vertPath));

	vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
		.stage = vk::ShaderStageFlagBits::eVertex, .module = vertModule, .pName = "main" };
	vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo };

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
														.depthBiasEnable = vk::False,
														.lineWidth = 1.0f };

	vk::PipelineMultisampleStateCreateInfo  multisampling{ .rasterizationSamples = vk::SampleCountFlagBits::e1,
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

	std::vector dynamicStates = {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor,
		vk::DynamicState::eCullMode,
		vk::DynamicState::eFrontFace,
		vk::DynamicState::eDepthTestEnable,
		vk::DynamicState::eDepthWriteEnable,
		vk::DynamicState::eDepthCompareOp };

	vk::PipelineDynamicStateCreateInfo dynamicState{ .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
													.pDynamicStates = dynamicStates.data() };

	vk::Format depthFormat = FindDepthFormat();

	vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
		{.stageCount = 1,
		 .pStages = shaderStages,
		 .pVertexInputState = &vertexInputInfo,
		 .pInputAssemblyState = &inputAssembly,
		 .pViewportState = &viewportState,
		 .pRasterizationState = &rasterizer,
		 .pMultisampleState = &multisampling,
		 .pDepthStencilState = &depthStencil,
		 .pDynamicState = &dynamicState,
		 .layout = layout,
		 .renderPass = nullptr},
		{.colorAttachmentCount = 0,
		 .pColorAttachmentFormats = nullptr,
		 .depthAttachmentFormat = depthFormat} };


	_pipelines.push_back(PipelineEntry{
		.pipeline = vk::raii::Pipeline(_vkCtx.GetDevice(), nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>()),
		.layout = layout
		});
	return _pipelines.size() - 1;
}

uint32_t VulkanRenderer::CreateShadowCubeMapGraphicsPipeline(const std::string& vertPath, vk::PipelineLayout layout)
{
	vk::raii::ShaderModule vertModule = CreateShaderModule(ReadFile(vertPath));

	vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
		.stage = vk::ShaderStageFlagBits::eVertex, .module = vertModule, .pName = "main" };
	vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo };

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
														.depthBiasEnable = vk::False,
														.lineWidth = 1.0f };

	vk::PipelineMultisampleStateCreateInfo  multisampling{ .rasterizationSamples = vk::SampleCountFlagBits::e1,
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

	std::vector dynamicStates = {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor,
		vk::DynamicState::eCullMode,
		vk::DynamicState::eFrontFace,
		vk::DynamicState::eDepthTestEnable,
		vk::DynamicState::eDepthWriteEnable,
		vk::DynamicState::eDepthCompareOp };

	vk::PipelineDynamicStateCreateInfo dynamicState{ .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
													.pDynamicStates = dynamicStates.data() };

	vk::Format depthFormat = FindDepthFormat();

	vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
		{.stageCount = 1,
		 .pStages = shaderStages,
		 .pVertexInputState = &vertexInputInfo,
		 .pInputAssemblyState = &inputAssembly,
		 .pViewportState = &viewportState,
		 .pRasterizationState = &rasterizer,
		 .pMultisampleState = &multisampling,
		 .pDepthStencilState = &depthStencil,
		 .pDynamicState = &dynamicState,
		 .layout = layout,
		 .renderPass = nullptr},
		{.viewMask = 0b00111111,
		 .colorAttachmentCount = 0,
		 .pColorAttachmentFormats = nullptr,
		 .depthAttachmentFormat = depthFormat} };


	_pipelines.push_back(PipelineEntry{
		.pipeline = vk::raii::Pipeline(_vkCtx.GetDevice(), nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>()),
		.layout = layout
		});
	return _pipelines.size() - 1;
}

uint32_t VulkanRenderer::CreateHDRGraphicsPipeline(const std::string& vertPath, const std::string& fragPath, vk::PipelineLayout layout)
{
	vk::raii::ShaderModule vertModule = CreateShaderModule(ReadFile(vertPath));
	vk::raii::ShaderModule fragModule = CreateShaderModule(ReadFile(fragPath));


	vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
		.stage = vk::ShaderStageFlagBits::eVertex, .module = vertModule, .pName = "main" };
	vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
		.stage = vk::ShaderStageFlagBits::eFragment, .module = fragModule, .pName = "main" };
	vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };


	vk::PipelineVertexInputStateCreateInfo vertexInputInfo{ };

	vk::PipelineInputAssemblyStateCreateInfo inputAssembly{ .topology = vk::PrimitiveTopology::eTriangleList,
														   .primitiveRestartEnable = vk::False };
	vk::PipelineViewportStateCreateInfo      viewportState{ .viewportCount = 1, .scissorCount = 1 };
	vk::PipelineRasterizationStateCreateInfo rasterizer{ .depthClampEnable = vk::False,
														.rasterizerDiscardEnable = vk::False,
														.polygonMode = vk::PolygonMode::eFill,
														.cullMode = vk::CullModeFlagBits::eNone,
														.frontFace = vk::FrontFace::eCounterClockwise,
														.depthBiasEnable = vk::False,
														.lineWidth = 1.0f };

	vk::PipelineMultisampleStateCreateInfo  multisampling{ .rasterizationSamples = vk::SampleCountFlagBits::e1,
														  .sampleShadingEnable = vk::False };
	vk::PipelineDepthStencilStateCreateInfo depthStencil{ .depthTestEnable = vk::False,
														 .depthWriteEnable = vk::False, };
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
		vk::DynamicState::eScissor };

	vk::PipelineDynamicStateCreateInfo dynamicState{ .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
													.pDynamicStates = dynamicStates.data() };

	const vk::Format& colorFormat = _pSwapChainCtx->GetSurfaceFormat().format;

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
		 .layout = layout,
		 .renderPass = nullptr},
		{.colorAttachmentCount = 1,
		 .pColorAttachmentFormats = &colorFormat,
		 .depthAttachmentFormat = vk::Format::eUndefined} };


	_pipelines.push_back(PipelineEntry{
		.pipeline = vk::raii::Pipeline(_vkCtx.GetDevice(), nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>()),
		.layout = layout
		});

	return static_cast<uint32_t>(_pipelines.size() - 1);
}

uint32_t VulkanRenderer::CreateEquirectToCubePipeline(const std::string& vertPath, const std::string& fragPath, vk::PipelineLayout layout)
{
	vk::raii::ShaderModule vertModule = CreateShaderModule(ReadFile(vertPath));
	vk::raii::ShaderModule fragModule = CreateShaderModule(ReadFile(fragPath));


	vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
		.stage = vk::ShaderStageFlagBits::eVertex, .module = vertModule, .pName = "main" };
	vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
		.stage = vk::ShaderStageFlagBits::eFragment, .module = fragModule, .pName = "main" };
	vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	auto bindingDescription = vk::VertexInputBindingDescription{ 0, sizeof(glm::vec3), vk::VertexInputRate::eVertex };
	std::vector< vk::VertexInputAttributeDescription> attributeDescriptions;

	 attributeDescriptions = { vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, 0) };

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
														.cullMode = vk::CullModeFlagBits::eNone,
														.frontFace = vk::FrontFace::eCounterClockwise,
														.depthBiasEnable = vk::False,
														.lineWidth = 1.0f };


	vk::PipelineMultisampleStateCreateInfo  multisampling{ .rasterizationSamples = vk::SampleCountFlagBits::e1,
														  .sampleShadingEnable = vk::False };
	vk::PipelineDepthStencilStateCreateInfo depthStencil{ .depthTestEnable = vk::False,
														 .depthWriteEnable = vk::False,
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
		vk::DynamicState::eDepthCompareOp };

	vk::PipelineDynamicStateCreateInfo dynamicState{ .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
													.pDynamicStates = dynamicStates.data() };

	vk::Format hdrFormat = vk::Format::eR16G16B16A16Sfloat;

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
		 .layout = layout,
		 .renderPass = nullptr},
		{.viewMask = 0b00111111,
		 .colorAttachmentCount = 1,
		 .pColorAttachmentFormats = &hdrFormat,
		 .depthAttachmentFormat = vk::Format::eUndefined} };


	_pipelines.push_back(PipelineEntry{
		.pipeline = vk::raii::Pipeline(_vkCtx.GetDevice(), nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>()),
		.layout = layout
		});

	return static_cast<uint32_t>(_pipelines.size() - 1);
}

uint32_t VulkanRenderer::CreateBRDFLUTPipeline(const std::string& vertPath, const std::string& fragPath, vk::PipelineLayout layout)
{
	// Fullscreen-triangle pipeline (no vertex input), no depth, no multiview, R16G16B16A16 RT.
	vk::raii::ShaderModule vertModule = CreateShaderModule(ReadFile(vertPath));
	vk::raii::ShaderModule fragModule = CreateShaderModule(ReadFile(fragPath));

	vk::PipelineShaderStageCreateInfo vertShaderStageInfo{ .stage = vk::ShaderStageFlagBits::eVertex, .module = vertModule, .pName = "main" };
	vk::PipelineShaderStageCreateInfo fragShaderStageInfo{ .stage = vk::ShaderStageFlagBits::eFragment, .module = fragModule, .pName = "main" };
	vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
	vk::PipelineInputAssemblyStateCreateInfo inputAssembly{ .topology = vk::PrimitiveTopology::eTriangleList, .primitiveRestartEnable = vk::False };
	vk::PipelineViewportStateCreateInfo viewportState{ .viewportCount = 1, .scissorCount = 1 };
	vk::PipelineRasterizationStateCreateInfo rasterizer{ .depthClampEnable = vk::False,
														.rasterizerDiscardEnable = vk::False,
														.polygonMode = vk::PolygonMode::eFill,
														.cullMode = vk::CullModeFlagBits::eNone,
														.frontFace = vk::FrontFace::eCounterClockwise,
														.depthBiasEnable = vk::False,
														.lineWidth = 1.0f };
	vk::PipelineMultisampleStateCreateInfo multisampling{ .rasterizationSamples = vk::SampleCountFlagBits::e1, .sampleShadingEnable = vk::False };
	vk::PipelineDepthStencilStateCreateInfo depthStencil{ .depthTestEnable = vk::False, .depthWriteEnable = vk::False };
	vk::PipelineColorBlendAttachmentState colorBlendAttachment;
	colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
		vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	colorBlendAttachment.blendEnable = vk::False;

	vk::PipelineColorBlendStateCreateInfo colorBlending{ .logicOpEnable = vk::False,
														.logicOp = vk::LogicOp::eCopy,
														.attachmentCount = 1,
														.pAttachments = &colorBlendAttachment };

	std::vector dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	vk::PipelineDynamicStateCreateInfo dynamicState{ .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
													.pDynamicStates = dynamicStates.data() };

	vk::Format hdrFormat = vk::Format::eR16G16B16A16Sfloat;

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
		 .layout = layout,
		 .renderPass = nullptr},
		{.colorAttachmentCount = 1,
		 .pColorAttachmentFormats = &hdrFormat,
		 .depthAttachmentFormat = vk::Format::eUndefined} };

	_pipelines.push_back(PipelineEntry{
		.pipeline = vk::raii::Pipeline(_vkCtx.GetDevice(), nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>()),
		.layout = layout
		});

	return static_cast<uint32_t>(_pipelines.size() - 1);
}

uint32_t VulkanRenderer::CreateDeferredGBufferPipeline(const std::string& vertPath, const std::string& fragPath, vk::PipelineLayout layout)
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
														.cullMode = vk::CullModeFlagBits::eNone,
														.frontFace = vk::FrontFace::eCounterClockwise,
														.depthBiasEnable = vk::False,
														.lineWidth = 1.0f };

	vk::PipelineMultisampleStateCreateInfo  multisampling{ .rasterizationSamples = vk::SampleCountFlagBits::e1,
														  .sampleShadingEnable = vk::False };
	vk::PipelineDepthStencilStateCreateInfo depthStencil{ .depthTestEnable = vk::True,
														 .depthWriteEnable = vk::True,
														 .depthCompareOp = vk::CompareOp::eLess,
														 .depthBoundsTestEnable = vk::False,
														 .stencilTestEnable = vk::False };

	std::vector dynamicStates = {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor,
		vk::DynamicState::eCullMode,
		vk::DynamicState::eFrontFace,
		vk::DynamicState::eDepthTestEnable,
		vk::DynamicState::eDepthWriteEnable,
		vk::DynamicState::eDepthCompareOp };

	vk::PipelineDynamicStateCreateInfo dynamicState{ .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
													.pDynamicStates = dynamicStates.data() };

	vk::Format depthFormat = FindDepthFormat();

	vk::PipelineColorBlendAttachmentState   colorBlendAttachment;
	colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
		vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	colorBlendAttachment.blendEnable = vk::False;

	std::vector< vk::PipelineColorBlendAttachmentState> colorBlendAttachments;
	colorBlendAttachments.reserve(_gBufferColorTargetFormats.size());
	
	for (size_t i = 0; i < _gBufferColorTargetFormats.size(); i++)
	{
		colorBlendAttachments.push_back(colorBlendAttachment);
	}

	vk::PipelineColorBlendStateCreateInfo colorBlending{ .logicOpEnable = vk::False,
														.logicOp = vk::LogicOp::eCopy,
														.attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size()),
														.pAttachments = colorBlendAttachments.data() };

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
		 .layout = layout,
		 .renderPass = nullptr},
		{.colorAttachmentCount = static_cast<uint32_t>(_gBufferColorTargetFormats.size()),
		 .pColorAttachmentFormats = _gBufferColorTargetFormats.data(),
		 .depthAttachmentFormat = depthFormat} };


	_pipelines.push_back(PipelineEntry{
		.pipeline = vk::raii::Pipeline(_vkCtx.GetDevice(), nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>()),
		.layout = layout
		});

	return static_cast<uint32_t>(_pipelines.size() - 1);
}

uint32_t VulkanRenderer::CreateDeferredLightingPipeline(const std::string& vertPath, const std::string& fragPath, vk::PipelineLayout layout)
{
	vk::raii::ShaderModule vertModule = CreateShaderModule(ReadFile(vertPath));
	vk::raii::ShaderModule fragModule = CreateShaderModule(ReadFile(fragPath));


	vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
		.stage = vk::ShaderStageFlagBits::eVertex, .module = vertModule, .pName = "main" };
	vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
		.stage = vk::ShaderStageFlagBits::eFragment, .module = fragModule, .pName = "main" };
	vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };


	vk::PipelineVertexInputStateCreateInfo vertexInputInfo{ };

	vk::PipelineInputAssemblyStateCreateInfo inputAssembly{ .topology = vk::PrimitiveTopology::eTriangleList,
														   .primitiveRestartEnable = vk::False };
	vk::PipelineViewportStateCreateInfo      viewportState{ .viewportCount = 1, .scissorCount = 1 };
	vk::PipelineRasterizationStateCreateInfo rasterizer{ .depthClampEnable = vk::False,
														.rasterizerDiscardEnable = vk::False,
														.polygonMode = vk::PolygonMode::eFill,
														.cullMode = vk::CullModeFlagBits::eNone,
														.frontFace = vk::FrontFace::eCounterClockwise,
														.depthBiasEnable = vk::False,
														.lineWidth = 1.0f };

	vk::PipelineMultisampleStateCreateInfo  multisampling{ .rasterizationSamples = vk::SampleCountFlagBits::e1,
														  .sampleShadingEnable = vk::False };
	vk::PipelineDepthStencilStateCreateInfo depthStencil{ .depthTestEnable = vk::False,
														 .depthWriteEnable = vk::False, };
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
		vk::DynamicState::eDepthTestEnable,
		vk::DynamicState::eDepthWriteEnable,
		vk::DynamicState::eDepthCompareOp};

	vk::PipelineDynamicStateCreateInfo dynamicState{ .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
													.pDynamicStates = dynamicStates.data() };

	const vk::Format& hdrFormat = vk::Format::eR16G16B16A16Sfloat;
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
		 .layout = layout,
		 .renderPass = nullptr},
		{.colorAttachmentCount = 1,
		 .pColorAttachmentFormats = &hdrFormat,
		 .depthAttachmentFormat = depthFormat} };


	_pipelines.push_back(PipelineEntry{
		.pipeline = vk::raii::Pipeline(_vkCtx.GetDevice(), nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>()),
		.layout = layout
		});

	return static_cast<uint32_t>(_pipelines.size() - 1);
}

void VulkanRenderer::CreateCommandPool()
{
	vk::CommandPoolCreateInfo poolInfo{ .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
									   .queueFamilyIndex = _vkCtx.GetQueueFamilyIndex() };

	_commandPool = vk::raii::CommandPool(_vkCtx.GetDevice(), poolInfo);
}

void VulkanRenderer::CreateCommandBuffers()
{
	vk::CommandBufferAllocateInfo allocInfo{ .commandPool = _commandPool,
											.level = vk::CommandBufferLevel::ePrimary,
											.commandBufferCount = MAX_FRAMES_IN_FLIGHT };
	vk::raii::CommandBuffers buffers(_vkCtx.GetDevice(), allocInfo);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		_frames[i].commandBuffer = std::move(buffers[i]);
}

void VulkanRenderer::RecordCommandBuffer(uint32_t imageIndex)
{
	const vk::raii::CommandBuffer& cmd = _frames[_currentFrame].commandBuffer;

	cmd.begin({});

	size_t casterCount = std::min(_dirLights.size() + _spotLights.size(), _shadowMapImages.size());
	// Shadow map BEGIN
	for (size_t j = 0; j < casterCount; j++)
	{
		// Transition the depth image to DEPTH_ATTACHMENT_OPTIMAL
		TransitionImageLayout(_shadowMapImages[j].image,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eDepthAttachmentOptimal,
			{},
			vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::ImageAspectFlagBits::eDepth);

		// Reverse Z, cleared 0.0f instead
		vk::ClearValue clearDepth = vk::ClearDepthStencilValue(0.0f, 0);

		// Depth attachment
		vk::RenderingAttachmentInfo depthAttachment = { .imageView = _shadowMapImageViews[j],
													   .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
													   .loadOp = vk::AttachmentLoadOp::eClear,
													   .storeOp = vk::AttachmentStoreOp::eStore,
													   .clearValue = clearDepth };

		vk::RenderingInfo renderingInfo = { .renderArea = {.offset = {0, 0}, .extent = vk::Extent2D(SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION)},
										   .layerCount = 1,
										   .colorAttachmentCount = 0,
										   .pColorAttachments = nullptr,
										   .pDepthAttachment = &depthAttachment };

		cmd.beginRendering(renderingInfo);

		for (uint32_t i = 0; i < _drawList.size(); i++)
		{
			const PipelineEntry& pso = _pipelines[_shadowPipelineIndex];

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pso.pipeline);

			cmd.setViewport(0,
				vk::Viewport(
					0.0f, 0.0f,
					static_cast<float>(SHADOW_MAP_RESOLUTION),
					static_cast<float>(SHADOW_MAP_RESOLUTION),
					0.0f, 1.0f));

			cmd.setScissor(0,
				vk::Rect2D(
					vk::Offset2D(0, 0),
					vk::Extent2D(SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION)));

			cmd.setCullMode(vk::CullModeFlagBits::eNone);
			cmd.setFrontFace(vk::FrontFace::eCounterClockwise);
			cmd.setDepthTestEnable(vk::True);
			cmd.setDepthWriteEnable(vk::True);
			// Reverse Z, use Greater instead
			cmd.setDepthCompareOp(vk::CompareOp::eGreater);

			// Reuse second slot (uint index) to indicate which view proj matrix to use
			PerDrawPC pc{ i, j };

			Mesh mesh = _meshManager.GetMesh(_drawList[i]._renderComp->_mesh);

			cmd.bindVertexBuffers(0, vk::Buffer(mesh.vertexBuffer.buffer), { 0 });

			cmd.bindIndexBuffer(vk::Buffer(mesh.indexBuffer.buffer), 0, vk::IndexType::eUint32);

			cmd.bindDescriptorSets(
				vk::PipelineBindPoint::eGraphics,
				pso.layout,
				0,
				*_frames[_currentFrame].globalDescriptorSet,
				nullptr);

			cmd.pushConstants<PerDrawPC>(
				pso.layout,
				vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
				0,
				pc
			);

			cmd.drawIndexed(static_cast<uint32_t>(mesh.indexCount), 1, 0, 0, 0);
		}

		cmd.endRendering();

		// Make the depth readable (sampling / ImGui display)
		TransitionImageLayout(_shadowMapImages[j].image,
			vk::ImageLayout::eDepthAttachmentOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::AccessFlagBits2::eShaderRead,
			vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::PipelineStageFlagBits2::eFragmentShader,
			vk::ImageAspectFlagBits::eDepth);
	}
	// Shadow map END

	// Shadow Cube map BEGIN
	for (size_t j = 0; j < std::min(_pointLights.size(), _shadowCubeMapImages.size()); j++)
	{
		// Transition the depth image to DEPTH_ATTACHMENT_OPTIMAL
		TransitionImageLayout(_shadowCubeMapImages[j].image,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eDepthAttachmentOptimal,
			{},
			vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::ImageAspectFlagBits::eDepth);

		// Reverse Z, cleared 0.0f instead
		vk::ClearValue clearDepth = vk::ClearDepthStencilValue(0.0f, 0);

		// Depth attachment
		vk::RenderingAttachmentInfo depthAttachment = { .imageView = _shadowCubeMapImageViews[j],
													   .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
													   .loadOp = vk::AttachmentLoadOp::eClear,
													   .storeOp = vk::AttachmentStoreOp::eStore,
													   .clearValue = clearDepth };

		vk::RenderingInfo renderingInfo = { .renderArea = {.offset = {0, 0}, .extent = vk::Extent2D(SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION)},
										   .layerCount = 6,
										   .viewMask = 0b00111111,
										   .colorAttachmentCount = 0,
										   .pColorAttachments = nullptr,
										   .pDepthAttachment = &depthAttachment };

		cmd.beginRendering(renderingInfo);

		for (uint32_t i = 0; i < _drawList.size(); i++)
		{
			const PipelineEntry& pso = _pipelines[_shadowCubeMapPipelineIndex];

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pso.pipeline);

			cmd.setViewport(0,
				vk::Viewport(
					0.0f, 0.0f,
					static_cast<float>(SHADOW_MAP_RESOLUTION),
					static_cast<float>(SHADOW_MAP_RESOLUTION),
					0.0f, 1.0f));

			cmd.setScissor(0,
				vk::Rect2D(
					vk::Offset2D(0, 0),
					vk::Extent2D(SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION)));

			cmd.setCullMode(vk::CullModeFlagBits::eNone);
			cmd.setFrontFace(vk::FrontFace::eCounterClockwise);
			cmd.setDepthTestEnable(vk::True);
			cmd.setDepthWriteEnable(vk::True);
			// Reverse Z, use Greater instead
			cmd.setDepthCompareOp(vk::CompareOp::eGreater);

			PerDrawPC pc{ i, _shadowMapImages.size() + j * 6 };

			Mesh mesh = _meshManager.GetMesh(_drawList[i]._renderComp->_mesh);

			cmd.bindVertexBuffers(0, vk::Buffer(mesh.vertexBuffer.buffer), { 0 });

			cmd.bindIndexBuffer(vk::Buffer(mesh.indexBuffer.buffer), 0, vk::IndexType::eUint32);

			cmd.bindDescriptorSets(
				vk::PipelineBindPoint::eGraphics,
				pso.layout,
				0,
				*_frames[_currentFrame].globalDescriptorSet,
				nullptr);

			cmd.pushConstants<PerDrawPC>(
				pso.layout,
				vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
				0,
				pc
			);

			cmd.drawIndexed(static_cast<uint32_t>(mesh.indexCount), 1, 0, 0, 0);
		}

		cmd.endRendering();

		TransitionImageLayout(_shadowCubeMapImages[j].image,
			vk::ImageLayout::eDepthAttachmentOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::AccessFlagBits2::eShaderRead,
			vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::PipelineStageFlagBits2::eFragmentShader,
			vk::ImageAspectFlagBits::eDepth);
	}
	// Shadow Cube map END


	TransitionImageLayout(_hdrColorImage.image,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eColorAttachmentOptimal,
		{},
		vk::AccessFlagBits2::eColorAttachmentWrite,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::ImageAspectFlagBits::eColor);

	// Transition the depth image to DEPTH_ATTACHMENT_OPTIMAL
	TransitionImageLayout(_depthImage.image,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eDepthAttachmentOptimal,
		vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
		vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
		vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
		vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
		vk::ImageAspectFlagBits::eDepth);

	vk::ClearValue clearColor = vk::ClearColorValue(0.015f, 0.015f, 0.015f, 1.0f);
	// Reverse Z, cleared 0.0f instead
	vk::ClearValue clearDepth = vk::ClearDepthStencilValue(0.0f, 0);

	const auto swapChainExtent = _pSwapChainCtx->GetExtent();

	// Directly render onto hdr color image (no multisampling)
	vk::RenderingAttachmentInfo colorAttachment = { .imageView = _hdrColorImageView,
												   .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
												   .resolveMode = vk::ResolveModeFlagBits::eNone,
												   .loadOp = vk::AttachmentLoadOp::eClear,
												   .storeOp = vk::AttachmentStoreOp::eStore,
												   .clearValue = clearColor };

	// Depth attachment
	vk::RenderingAttachmentInfo depthAttachment = { .imageView = _depthImageView,
												   .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
												   .loadOp = vk::AttachmentLoadOp::eClear,
												   .storeOp = vk::AttachmentStoreOp::eDontCare,
												   .clearValue = clearDepth };

	vk::RenderingInfo renderingInfo = { .renderArea = {.offset = {0, 0}, .extent = swapChainExtent},
									   .layerCount = 1,
									   .colorAttachmentCount = 1,
									   .pColorAttachments = &colorAttachment,
									   .pDepthAttachment = &depthAttachment };

	cmd.beginRendering(renderingInfo);


	// Draw skybox
	{
		const PipelineEntry& pso = _pipelines[_skyboxPipelineIndex];

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pso.pipeline);

		cmd.setViewport(0,
			vk::Viewport(
				0.0f, 0.0f,
				static_cast<float>(swapChainExtent.width),
				static_cast<float>(swapChainExtent.height),
				0.0f, 1.0f));

		cmd.setScissor(0,
			vk::Rect2D(
				vk::Offset2D(0, 0),
				swapChainExtent));

		cmd.setCullMode(vk::CullModeFlagBits::eNone);
		cmd.setFrontFace(vk::FrontFace::eCounterClockwise);
		cmd.setDepthTestEnable(vk::True);
		cmd.setDepthWriteEnable(vk::True);
		// Reverse Z, use Greater instead
		cmd.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);

		// Since topology can't switch across class (triangle <-> line) for debug draws, only set them at pipeline creation time
		// And topology removed from dynamic states
		//cmd.setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);

		PerDrawPC pc{ 0, 0 };

		const Mesh& mesh = _meshManager.GetMesh(_meshManager.GetHandle("unit_cube"));

		cmd.bindVertexBuffers(0, vk::Buffer(mesh.vertexBuffer.buffer), { 0 });

		cmd.bindIndexBuffer(vk::Buffer(mesh.indexBuffer.buffer), 0, vk::IndexType::eUint32);

		cmd.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			pso.layout,
			0,
			*_frames[_currentFrame].globalDescriptorSet,
			nullptr);

		cmd.pushConstants<PerDrawPC>(
			pso.layout,
			vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
			0,
			pc
		);

		cmd.drawIndexed(static_cast<uint32_t>(mesh.indexCount), 1, 0, 0, 0);
	}



	size_t offset = 0;

	for (size_t i = 0; i < _drawList.size(); i++)
	{
		const Material& mat = _materialManager.GetMaterial(_drawList[i]._renderComp->_material);
		const PipelineEntry& pso = _pipelines[mat.pipeline];

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pso.pipeline);

		cmd.setViewport(0,
			vk::Viewport(
				0.0f, 0.0f,
				static_cast<float>(swapChainExtent.width),
				static_cast<float>(swapChainExtent.height),
				0.0f, 1.0f));

		cmd.setScissor(0,
			vk::Rect2D(
				vk::Offset2D(0, 0),
				swapChainExtent));

		cmd.setCullMode(vk::CullModeFlagBits::eBack);
		cmd.setFrontFace(vk::FrontFace::eCounterClockwise);
		cmd.setDepthTestEnable(vk::True);
		cmd.setDepthWriteEnable(vk::True);
		// Reverse Z, use Greater instead
		cmd.setDepthCompareOp(vk::CompareOp::eGreater);

		// Since topology can't switch across class (triangle <-> line) for debug draws, only set them at pipeline creation time
		// And topology removed from dynamic states
		//cmd.setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);

		PerDrawPC pc{ i, _drawList[i]._renderComp->_material.index };

		const Mesh& mesh = _meshManager.GetMesh(_drawList[i]._renderComp->_mesh);

		cmd.bindVertexBuffers(0, vk::Buffer(mesh.vertexBuffer.buffer), { 0 });

		cmd.bindIndexBuffer(vk::Buffer(mesh.indexBuffer.buffer), 0, vk::IndexType::eUint32);

		cmd.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			pso.layout,
			0,
			*_frames[_currentFrame].globalDescriptorSet,
			nullptr);

		cmd.pushConstants<PerDrawPC>(
			pso.layout,
			vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
			0,
			pc
		);

		cmd.drawIndexed(static_cast<uint32_t>(mesh.indexCount), 1, 0, 0, 0);
	}

	offset += _drawList.size();

	// Draw debug AABB boxs
	for (size_t i = 0; i < _debugAABBDrawList.size(); i++)
	{
		const Material& mat = _materialManager.GetMaterial(_debugAABBMaterial);
		const PipelineEntry& pso = _pipelines[mat.pipeline];

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pso.pipeline);

		cmd.setViewport(0,
			vk::Viewport(
				0.0f, 0.0f,
				static_cast<float>(swapChainExtent.width),
				static_cast<float>(swapChainExtent.height),
				0.0f, 1.0f));

		cmd.setScissor(0,
			vk::Rect2D(
				vk::Offset2D(0, 0),
				swapChainExtent));

		cmd.setCullMode(vk::CullModeFlagBits::eBack);
		cmd.setFrontFace(vk::FrontFace::eCounterClockwise);
		cmd.setDepthTestEnable(vk::True);
		cmd.setDepthWriteEnable(vk::True);
		// Reverse Z, use Greater instead
		cmd.setDepthCompareOp(vk::CompareOp::eGreater);

		// Since topology can't switch across class (triangle <-> line) for debug draws, only set them at pipeline creation time
		// And topology removed from dynamic states
		//cmd.setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);

		PerDrawPC pc{ i + offset, _debugAABBMaterial.index };

		const Mesh& mesh = _meshManager.GetDebugMesh(DebugMeshType::AABB);

		cmd.bindVertexBuffers(0, vk::Buffer(mesh.vertexBuffer.buffer), { 0 });

		cmd.bindIndexBuffer(vk::Buffer(mesh.indexBuffer.buffer), 0, vk::IndexType::eUint32);

		cmd.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			pso.layout,
			0,
			*_frames[_currentFrame].globalDescriptorSet,
			nullptr);

		cmd.pushConstants<PerDrawPC>(
			pso.layout,
			vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
			0,
			pc
		);

		cmd.drawIndexed(static_cast<uint32_t>(mesh.indexCount), 1, 0, 0, 0);
	}

	offset += _debugAABBDrawList.size();

	// debug lights
	for (uint32_t i = 0; i < _dirLights.size() + _spotLights.size() + _pointLights.size(); i++)
	{
		const Material& mat = _materialManager.GetMaterial(_debugLightMaterial);
		const PipelineEntry& pso = _pipelines[mat.pipeline];

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pso.pipeline);

		cmd.setViewport(0,
			vk::Viewport(
				0.0f, 0.0f,
				static_cast<float>(swapChainExtent.width),
				static_cast<float>(swapChainExtent.height),
				0.0f, 1.0f));

		cmd.setScissor(0,
			vk::Rect2D(
				vk::Offset2D(0, 0),
				swapChainExtent));

		cmd.setCullMode(vk::CullModeFlagBits::eBack);
		cmd.setFrontFace(vk::FrontFace::eCounterClockwise);
		cmd.setDepthTestEnable(vk::True);
		cmd.setDepthWriteEnable(vk::True);
		// Reverse Z, use Greater instead
		cmd.setDepthCompareOp(vk::CompareOp::eGreater);

		// Since topology can't switch across class (triangle <-> line) for debug draws, only set them at pipeline creation time
		// And topology removed from dynamic states
		//cmd.setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);

		PerDrawPC pc{ i + offset, _debugLightMaterial.index };

		// For point lights, use icosphere; pyramid for spot/directional
		DebugMeshType meshType = (i >= _dirLights.size() + _spotLights.size()) ? DebugMeshType::Icosphere : DebugMeshType::Pyramid;

		const Mesh& mesh = _meshManager.GetDebugMesh(meshType);

		cmd.bindVertexBuffers(0, vk::Buffer(mesh.vertexBuffer.buffer), { 0 });

		cmd.bindIndexBuffer(vk::Buffer(mesh.indexBuffer.buffer), 0, vk::IndexType::eUint32);

		cmd.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			pso.layout,
			0,
			*_frames[_currentFrame].globalDescriptorSet,
			nullptr);

		cmd.pushConstants<PerDrawPC>(
			pso.layout,
			vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
			0,
			pc
		);

		cmd.drawIndexed(static_cast<uint32_t>(mesh.indexCount), 1, 0, 0, 0);
	}



	cmd.endRendering();

	// HDR scene image: color-attachment-write -> shader-read, so the tonemap pass can sample it
	TransitionImageLayout(_hdrColorImage.image,
		vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageLayout::eShaderReadOnlyOptimal,
		vk::AccessFlagBits2::eColorAttachmentWrite,
		vk::AccessFlagBits2::eShaderRead,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::PipelineStageFlagBits2::eFragmentShader,
		vk::ImageAspectFlagBits::eColor);

	// Tone map pass
	{
		// Before starting rendering, transition the swapchain image to COLOR_ATTACHMENT_OPTIMAL
		TransitionImageLayout(_pSwapChainCtx->GetSwapChainImage(imageIndex),
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal,
			{},                                                        // srcAccessMask (no need to wait for previous operations)
			vk::AccessFlagBits2::eColorAttachmentWrite,                // dstAccessMask
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // dstStage
			vk::ImageAspectFlagBits::eColor);

		vk::RenderingAttachmentInfo colorAttachment = { .imageView = _pSwapChainCtx->GetSwapChainImageView(imageIndex),
												   .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
												   .loadOp = vk::AttachmentLoadOp::eClear,
												   .storeOp = vk::AttachmentStoreOp::eStore,
												   .clearValue = clearColor };

		vk::RenderingInfo renderingInfo = { .renderArea = {.offset = {0, 0}, .extent = swapChainExtent},
									   .layerCount = 1,
									   .colorAttachmentCount = 1,
									   .pColorAttachments = &colorAttachment };

		cmd.beginRendering(renderingInfo);

		const PipelineEntry& pso = _pipelines[_hdrOutputPipelineIndex];

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pso.pipeline);

		cmd.setViewport(0,
			vk::Viewport(
				0.0f, 0.0f,
				static_cast<float>(swapChainExtent.width),
				static_cast<float>(swapChainExtent.height),
				0.0f, 1.0f));

		cmd.setScissor(0,
			vk::Rect2D(
				vk::Offset2D(0, 0),
				swapChainExtent));

		cmd.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			pso.layout,
			0,
			*_frames[_currentFrame].globalDescriptorSet,
			nullptr);

		cmd.draw(3, 1, 0, 0);

	}

#if USE_DEAR_IMGUI_INTERFACE
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *cmd);
#endif

	cmd.endRendering();

	
	if (_programCtx.GetRequestScreenshot())
	{
		CaptureScreenshot(imageIndex);
	}
	else
	{
		// After rendering, transition the swapchain image to PRESENT_SRC
		TransitionImageLayout(_pSwapChainCtx->GetSwapChainImage(imageIndex),
			vk::ImageLayout::eColorAttachmentOptimal,
			vk::ImageLayout::ePresentSrcKHR,
			vk::AccessFlagBits2::eColorAttachmentWrite,                // srcAccessMask
			{},                                                        // dstAccessMask
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
			vk::PipelineStageFlagBits2::eNone,							// dstStage
			vk::ImageAspectFlagBits::eColor);
	}

	cmd.end();
}

void VulkanRenderer::RecordCommandBufferDeferredRendering(uint32_t imageIndex)
{
	const vk::raii::CommandBuffer& cmd = _frames[_currentFrame].commandBuffer;

	cmd.begin({});

	size_t casterCount = std::min(_dirLights.size() + _spotLights.size(), _shadowMapImages.size());
	// Shadow map BEGIN
	for (size_t j = 0; j < casterCount; j++)
	{
		// Transition the depth image to DEPTH_ATTACHMENT_OPTIMAL
		TransitionImageLayout(_shadowMapImages[j].image,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eDepthAttachmentOptimal,
			{},
			vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::PipelineStageFlagBits2::eFragmentShader | vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::ImageAspectFlagBits::eDepth);

		// Reverse Z, cleared 0.0f instead
		vk::ClearValue clearDepth = vk::ClearDepthStencilValue(0.0f, 0);

		// Depth attachment
		vk::RenderingAttachmentInfo depthAttachment = { .imageView = _shadowMapImageViews[j],
													   .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
													   .loadOp = vk::AttachmentLoadOp::eClear,
													   .storeOp = vk::AttachmentStoreOp::eStore,
													   .clearValue = clearDepth };

		vk::RenderingInfo renderingInfo = { .renderArea = {.offset = {0, 0}, .extent = vk::Extent2D(SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION)},
										   .layerCount = 1,
										   .colorAttachmentCount = 0,
										   .pColorAttachments = nullptr,
										   .pDepthAttachment = &depthAttachment };

		cmd.beginRendering(renderingInfo);

		for (uint32_t i = 0; i < _drawList.size(); i++)
		{
			const PipelineEntry& pso = _pipelines[_shadowPipelineIndex];

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pso.pipeline);

			cmd.setViewport(0,
				vk::Viewport(
					0.0f, 0.0f,
					static_cast<float>(SHADOW_MAP_RESOLUTION),
					static_cast<float>(SHADOW_MAP_RESOLUTION),
					0.0f, 1.0f));

			cmd.setScissor(0,
				vk::Rect2D(
					vk::Offset2D(0, 0),
					vk::Extent2D(SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION)));

			cmd.setCullMode(vk::CullModeFlagBits::eNone);
			cmd.setFrontFace(vk::FrontFace::eCounterClockwise);
			cmd.setDepthTestEnable(vk::True);
			cmd.setDepthWriteEnable(vk::True);
			// Reverse Z, use Greater instead
			cmd.setDepthCompareOp(vk::CompareOp::eGreater);

			// Reuse second slot (uint index) to indicate which view proj matrix to use
			PerDrawPC pc{ i, j };

			Mesh mesh = _meshManager.GetMesh(_drawList[i]._renderComp->_mesh);

			cmd.bindVertexBuffers(0, vk::Buffer(mesh.vertexBuffer.buffer), { 0 });

			cmd.bindIndexBuffer(vk::Buffer(mesh.indexBuffer.buffer), 0, vk::IndexType::eUint32);

			cmd.bindDescriptorSets(
				vk::PipelineBindPoint::eGraphics,
				pso.layout,
				0,
				*_frames[_currentFrame].globalDescriptorSet,
				nullptr);

			cmd.pushConstants<PerDrawPC>(
				pso.layout,
				vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
				0,
				pc
			);

			cmd.drawIndexed(static_cast<uint32_t>(mesh.indexCount), 1, 0, 0, 0);
		}

		cmd.endRendering();

		// Make the depth readable (sampling / ImGui display)
		TransitionImageLayout(_shadowMapImages[j].image,
			vk::ImageLayout::eDepthAttachmentOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::AccessFlagBits2::eShaderRead,
			vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::PipelineStageFlagBits2::eFragmentShader,
			vk::ImageAspectFlagBits::eDepth);
	}
	// Shadow map END

	// Shadow Cube map BEGIN
	for (size_t j = 0; j < std::min(_pointLights.size(), _shadowCubeMapImages.size()); j++)
	{
		// Transition the depth image to DEPTH_ATTACHMENT_OPTIMAL
		TransitionImageLayout(_shadowCubeMapImages[j].image,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eDepthAttachmentOptimal,
			{},
			vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::PipelineStageFlagBits2::eFragmentShader | vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::ImageAspectFlagBits::eDepth);

		// Reverse Z, cleared 0.0f instead
		vk::ClearValue clearDepth = vk::ClearDepthStencilValue(0.0f, 0);

		// Depth attachment
		vk::RenderingAttachmentInfo depthAttachment = { .imageView = _shadowCubeMapImageViews[j],
													   .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
													   .loadOp = vk::AttachmentLoadOp::eClear,
													   .storeOp = vk::AttachmentStoreOp::eStore,
													   .clearValue = clearDepth };

		vk::RenderingInfo renderingInfo = { .renderArea = {.offset = {0, 0}, .extent = vk::Extent2D(SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION)},
										   .layerCount = 6,
										   .viewMask = 0b00111111,
										   .colorAttachmentCount = 0,
										   .pColorAttachments = nullptr,
										   .pDepthAttachment = &depthAttachment };

		cmd.beginRendering(renderingInfo);

		for (uint32_t i = 0; i < _drawList.size(); i++)
		{
			const PipelineEntry& pso = _pipelines[_shadowCubeMapPipelineIndex];

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pso.pipeline);

			cmd.setViewport(0,
				vk::Viewport(
					0.0f, 0.0f,
					static_cast<float>(SHADOW_MAP_RESOLUTION),
					static_cast<float>(SHADOW_MAP_RESOLUTION),
					0.0f, 1.0f));

			cmd.setScissor(0,
				vk::Rect2D(
					vk::Offset2D(0, 0),
					vk::Extent2D(SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION)));

			cmd.setCullMode(vk::CullModeFlagBits::eNone);
			cmd.setFrontFace(vk::FrontFace::eCounterClockwise);
			cmd.setDepthTestEnable(vk::True);
			cmd.setDepthWriteEnable(vk::True);
			// Reverse Z, use Greater instead
			cmd.setDepthCompareOp(vk::CompareOp::eGreater);

			PerDrawPC pc{ i, _shadowMapImages.size() + j * 6 };

			Mesh mesh = _meshManager.GetMesh(_drawList[i]._renderComp->_mesh);

			cmd.bindVertexBuffers(0, vk::Buffer(mesh.vertexBuffer.buffer), { 0 });

			cmd.bindIndexBuffer(vk::Buffer(mesh.indexBuffer.buffer), 0, vk::IndexType::eUint32);

			cmd.bindDescriptorSets(
				vk::PipelineBindPoint::eGraphics,
				pso.layout,
				0,
				*_frames[_currentFrame].globalDescriptorSet,
				nullptr);

			cmd.pushConstants<PerDrawPC>(
				pso.layout,
				vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
				0,
				pc
			);

			cmd.drawIndexed(static_cast<uint32_t>(mesh.indexCount), 1, 0, 0, 0);
		}

		cmd.endRendering();

		TransitionImageLayout(_shadowCubeMapImages[j].image,
			vk::ImageLayout::eDepthAttachmentOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::AccessFlagBits2::eShaderRead,
			vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::PipelineStageFlagBits2::eFragmentShader,
			vk::ImageAspectFlagBits::eDepth);
	}
	// Shadow Cube map END

	// Shared by late passes
	vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
	// Reverse Z, cleared 0.0f instead
	vk::ClearValue clearDepth = vk::ClearDepthStencilValue(0.0f, 0);
	const auto swapChainExtent = _pSwapChainCtx->GetExtent();
	// offset in model matrix (ObjectSSBO), shared by objects & debug draws
	size_t offset = 0;

	// G Buffer Pass BEGIN
	{
		// Rendering onto color & depth targets, transit to attachment layout
		for (size_t i = 0; i < _gBufferColorTargetImages.size(); i++)
		{
			TransitionImageLayout(_gBufferColorTargetImages[i].image,
				vk::ImageLayout::eUndefined,
				vk::ImageLayout::eColorAttachmentOptimal,
				{},	// no memory dependency
				vk::AccessFlagBits2::eColorAttachmentWrite,
				vk::PipelineStageFlagBits2::eFragmentShader, // read by last frame's lighting pass
				vk::PipelineStageFlagBits2::eColorAttachmentOutput,
				vk::ImageAspectFlagBits::eColor);
		}

		TransitionImageLayout(_depthImage.image,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eDepthAttachmentOptimal,
			{}, // no memory dependency
			vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::PipelineStageFlagBits2::eFragmentShader | vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::ImageAspectFlagBits::eDepth);

		// Multiple color attachments
		std::vector< vk::RenderingAttachmentInfo> colorAttachments;
		colorAttachments.reserve(_gBufferColorTargetImageViews.size());

		for (size_t i = 0; i < _gBufferColorTargetImageViews.size(); i++)
		{
			vk::RenderingAttachmentInfo attachment = { .imageView = _gBufferColorTargetImageViews[i],
													   .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
													   .resolveMode = vk::ResolveModeFlagBits::eNone,
													   .loadOp = vk::AttachmentLoadOp::eClear,
													   .storeOp = vk::AttachmentStoreOp::eStore,
													   .clearValue = clearColor };
			colorAttachments.push_back(attachment);
		}

		// Depth attachment
		vk::RenderingAttachmentInfo depthAttachment = { .imageView = _depthImageView,
													   .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
													   .loadOp = vk::AttachmentLoadOp::eClear,
													   .storeOp = vk::AttachmentStoreOp::eStore,
													   .clearValue = clearDepth };

		vk::RenderingInfo renderingInfo = { .renderArea = {.offset = {0, 0}, .extent = swapChainExtent},
										   .layerCount = 1,
										   .colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size()),
										   .pColorAttachments = colorAttachments.data(),
										   .pDepthAttachment = &depthAttachment };

		cmd.beginRendering(renderingInfo);

		// Objects
		for (size_t i = 0; i < _drawList.size(); i++)
		{
			const Material& mat = _materialManager.GetMaterial(_drawList[i]._renderComp->_material);
			const PipelineEntry& pso = _pipelines[_deferredGBufferPipelineIndex];

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pso.pipeline);

			cmd.setViewport(0,
				vk::Viewport(
					0.0f, 0.0f,
					static_cast<float>(swapChainExtent.width),
					static_cast<float>(swapChainExtent.height),
					0.0f, 1.0f));

			cmd.setScissor(0,
				vk::Rect2D(
					vk::Offset2D(0, 0),
					swapChainExtent));

			cmd.setCullMode(vk::CullModeFlagBits::eBack);
			cmd.setFrontFace(vk::FrontFace::eCounterClockwise);
			cmd.setDepthTestEnable(vk::True);
			cmd.setDepthWriteEnable(vk::True);
			// Reverse Z, use Greater instead
			cmd.setDepthCompareOp(vk::CompareOp::eGreater);

			PerDrawPC pc{ i, _drawList[i]._renderComp->_material.index };

			const Mesh& mesh = _meshManager.GetMesh(_drawList[i]._renderComp->_mesh);

			cmd.bindVertexBuffers(0, vk::Buffer(mesh.vertexBuffer.buffer), { 0 });

			cmd.bindIndexBuffer(vk::Buffer(mesh.indexBuffer.buffer), 0, vk::IndexType::eUint32);

			cmd.bindDescriptorSets(
				vk::PipelineBindPoint::eGraphics,
				pso.layout,
				0,
				*_frames[_currentFrame].globalDescriptorSet,
				nullptr);

			cmd.pushConstants<PerDrawPC>(
				pso.layout,
				vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
				0,
				pc
			);

			cmd.drawIndexed(static_cast<uint32_t>(mesh.indexCount), 1, 0, 0, 0);
		}

		offset += _drawList.size();

		cmd.endRendering();
	}
	// G Buffer Pass END

	// Lighting + Skybox + Debug Draws BEGIN
	{
		// Sampling from color/depth targets, transit to shader read layout
		for (size_t i = 0; i < _gBufferColorTargetImages.size(); i++)
		{
			TransitionImageLayout(_gBufferColorTargetImages[i].image,
				vk::ImageLayout::eColorAttachmentOptimal,
				vk::ImageLayout::eShaderReadOnlyOptimal,
				vk::AccessFlagBits2::eColorAttachmentWrite,
				vk::AccessFlagBits2::eShaderRead,
				vk::PipelineStageFlagBits2::eColorAttachmentOutput,
				vk::PipelineStageFlagBits2::eFragmentShader,
				vk::ImageAspectFlagBits::eColor);
		}

		TransitionImageLayout(_depthImage.image,
			vk::ImageLayout::eDepthAttachmentOptimal,
			vk::ImageLayout::eDepthReadOnlyOptimal,
			vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::AccessFlagBits2::eShaderSampledRead | vk::AccessFlagBits2::eDepthStencilAttachmentRead,
			vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::PipelineStageFlagBits2::eFragmentShader | vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::ImageAspectFlagBits::eDepth);

		// Render onto hdr image
		TransitionImageLayout(_hdrColorImage.image,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal,
			{},
			vk::AccessFlagBits2::eColorAttachmentWrite,
			vk::PipelineStageFlagBits2::eFragmentShader,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::ImageAspectFlagBits::eColor);

		vk::RenderingAttachmentInfo colorAttachment = { .imageView = _hdrColorImageView,
												   .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
												   .loadOp = vk::AttachmentLoadOp::eClear,
												   .storeOp = vk::AttachmentStoreOp::eStore,
												   .clearValue = clearColor };

		// Needs depth information, but do not store result
		vk::RenderingAttachmentInfo depthAttachment = { .imageView = _depthImageView,
												   .imageLayout = vk::ImageLayout::eDepthReadOnlyOptimal,
												   .loadOp = vk::AttachmentLoadOp::eLoad,
												   .storeOp = vk::AttachmentStoreOp::eNone };

		vk::RenderingInfo renderingInfo = { .renderArea = {.offset = {0, 0}, .extent = swapChainExtent},
									   .layerCount = 1,
									   .colorAttachmentCount = 1,
									   .pColorAttachments = &colorAttachment,
									   .pDepthAttachment = &depthAttachment };

		cmd.beginRendering(renderingInfo);

		// Deferred Lighting
		{
			const PipelineEntry& pso = _pipelines[_deferredLightingPipelineIndex];

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pso.pipeline);

			cmd.setViewport(0,
				vk::Viewport(
					0.0f, 0.0f,
					static_cast<float>(swapChainExtent.width),
					static_cast<float>(swapChainExtent.height),
					0.0f, 1.0f));

			cmd.setScissor(0,
				vk::Rect2D(
					vk::Offset2D(0, 0),
					swapChainExtent));

			// Deferred lighting pass, no need for both of these
			cmd.setDepthTestEnable(vk::False);
			cmd.setDepthWriteEnable(vk::False);
			// Reverse Z, use Greater instead
			cmd.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);

			cmd.bindDescriptorSets(
				vk::PipelineBindPoint::eGraphics,
				pso.layout,
				0,
				*_frames[_currentFrame].globalDescriptorSet,
				nullptr);

			cmd.draw(3, 1, 0, 0);
		}

		// Skybox
		{
			const PipelineEntry& pso = _pipelines[_skyboxPipelineIndex];

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pso.pipeline);

			cmd.setViewport(0,
				vk::Viewport(
					0.0f, 0.0f,
					static_cast<float>(swapChainExtent.width),
					static_cast<float>(swapChainExtent.height),
					0.0f, 1.0f));

			cmd.setScissor(0,
				vk::Rect2D(
					vk::Offset2D(0, 0),
					swapChainExtent));

			cmd.setCullMode(vk::CullModeFlagBits::eNone);
			cmd.setFrontFace(vk::FrontFace::eCounterClockwise);
			// Test against existing depth info, but do not write
			cmd.setDepthTestEnable(vk::True);
			cmd.setDepthWriteEnable(vk::False);
			// Reverse Z, use Greater instead
			cmd.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);

			PerDrawPC pc{ 0, 0 };

			const Mesh& mesh = _meshManager.GetMesh(_meshManager.GetHandle("unit_cube"));

			cmd.bindVertexBuffers(0, vk::Buffer(mesh.vertexBuffer.buffer), { 0 });

			cmd.bindIndexBuffer(vk::Buffer(mesh.indexBuffer.buffer), 0, vk::IndexType::eUint32);

			cmd.bindDescriptorSets(
				vk::PipelineBindPoint::eGraphics,
				pso.layout,
				0,
				*_frames[_currentFrame].globalDescriptorSet,
				nullptr);

			cmd.pushConstants<PerDrawPC>(
				pso.layout,
				vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
				0,
				pc
			);

			cmd.drawIndexed(static_cast<uint32_t>(mesh.indexCount), 1, 0, 0, 0);
		}

		// Draw debug AABB boxs
		for (size_t i = 0; i < _debugAABBDrawList.size(); i++)
		{
			const Material& mat = _materialManager.GetMaterial(_debugAABBMaterial);
			const PipelineEntry& pso = _pipelines[mat.pipeline];

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pso.pipeline);

			cmd.setViewport(0,
				vk::Viewport(
					0.0f, 0.0f,
					static_cast<float>(swapChainExtent.width),
					static_cast<float>(swapChainExtent.height),
					0.0f, 1.0f));

			cmd.setScissor(0,
				vk::Rect2D(
					vk::Offset2D(0, 0),
					swapChainExtent));

			cmd.setCullMode(vk::CullModeFlagBits::eBack);
			cmd.setFrontFace(vk::FrontFace::eCounterClockwise);
			// Test against existing depth info, but do not write
			cmd.setDepthTestEnable(vk::True);
			cmd.setDepthWriteEnable(vk::False);
			// Reverse Z, use Greater instead
			cmd.setDepthCompareOp(vk::CompareOp::eGreater);

			// Since topology can't switch across class (triangle <-> line) for debug draws, only set them at pipeline creation time
			// And topology removed from dynamic states
			//cmd.setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);

			PerDrawPC pc{ i + offset, _debugAABBMaterial.index };

			const Mesh& mesh = _meshManager.GetDebugMesh(DebugMeshType::AABB);

			cmd.bindVertexBuffers(0, vk::Buffer(mesh.vertexBuffer.buffer), { 0 });

			cmd.bindIndexBuffer(vk::Buffer(mesh.indexBuffer.buffer), 0, vk::IndexType::eUint32);

			cmd.bindDescriptorSets(
				vk::PipelineBindPoint::eGraphics,
				pso.layout,
				0,
				*_frames[_currentFrame].globalDescriptorSet,
				nullptr);

			cmd.pushConstants<PerDrawPC>(
				pso.layout,
				vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
				0,
				pc
			);

			cmd.drawIndexed(static_cast<uint32_t>(mesh.indexCount), 1, 0, 0, 0);
		}

		offset += _debugAABBDrawList.size();

		// Draw debug lights
		for (uint32_t i = 0; i < _dirLights.size() + _spotLights.size() + _pointLights.size(); i++)
		{
			const Material& mat = _materialManager.GetMaterial(_debugLightMaterial);
			const PipelineEntry& pso = _pipelines[mat.pipeline];

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pso.pipeline);

			cmd.setViewport(0,
				vk::Viewport(
					0.0f, 0.0f,
					static_cast<float>(swapChainExtent.width),
					static_cast<float>(swapChainExtent.height),
					0.0f, 1.0f));

			cmd.setScissor(0,
				vk::Rect2D(
					vk::Offset2D(0, 0),
					swapChainExtent));

			cmd.setCullMode(vk::CullModeFlagBits::eBack);
			cmd.setFrontFace(vk::FrontFace::eCounterClockwise);
			cmd.setDepthTestEnable(vk::True);
			cmd.setDepthWriteEnable(vk::False);
			// Reverse Z, use Greater instead
			cmd.setDepthCompareOp(vk::CompareOp::eGreater);

			// Since topology can't switch across class (triangle <-> line) for debug draws, only set them at pipeline creation time
			// And topology removed from dynamic states
			//cmd.setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);

			PerDrawPC pc{ i + offset, _debugLightMaterial.index };

			// For point lights, use icosphere; pyramid for spot/directional
			DebugMeshType meshType = (i >= _dirLights.size() + _spotLights.size()) ? DebugMeshType::Icosphere : DebugMeshType::Pyramid;

			const Mesh& mesh = _meshManager.GetDebugMesh(meshType);

			cmd.bindVertexBuffers(0, vk::Buffer(mesh.vertexBuffer.buffer), { 0 });

			cmd.bindIndexBuffer(vk::Buffer(mesh.indexBuffer.buffer), 0, vk::IndexType::eUint32);

			cmd.bindDescriptorSets(
				vk::PipelineBindPoint::eGraphics,
				pso.layout,
				0,
				*_frames[_currentFrame].globalDescriptorSet,
				nullptr);

			cmd.pushConstants<PerDrawPC>(
				pso.layout,
				vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
				0,
				pc
			);

			cmd.drawIndexed(static_cast<uint32_t>(mesh.indexCount), 1, 0, 0, 0);
		}

		cmd.endRendering();
	}
	// Lighting + Skybox + Debug Draws END


	// Tone map pass BEGIN
	{
		// Transit HDR image from attachment -> shader read, sampled by tone mapping pass
		TransitionImageLayout(_hdrColorImage.image,
			vk::ImageLayout::eColorAttachmentOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::AccessFlagBits2::eColorAttachmentWrite,
			vk::AccessFlagBits2::eShaderRead,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::PipelineStageFlagBits2::eFragmentShader,
			vk::ImageAspectFlagBits::eColor);

		TransitionImageLayout(_pSwapChainCtx->GetSwapChainImage(imageIndex),
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal,
			{},                                                        // srcAccessMask (no need to wait for previous operations)
			vk::AccessFlagBits2::eColorAttachmentWrite,                // dstAccessMask
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // dstStage
			vk::ImageAspectFlagBits::eColor);

		vk::RenderingAttachmentInfo colorAttachment = { .imageView = _pSwapChainCtx->GetSwapChainImageView(imageIndex),
												   .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
												   .loadOp = vk::AttachmentLoadOp::eClear,
												   .storeOp = vk::AttachmentStoreOp::eStore,
												   .clearValue = clearColor };

		vk::RenderingInfo renderingInfo = { .renderArea = {.offset = {0, 0}, .extent = swapChainExtent},
									   .layerCount = 1,
									   .colorAttachmentCount = 1,
									   .pColorAttachments = &colorAttachment };

		cmd.beginRendering(renderingInfo);

		const PipelineEntry& pso = _pipelines[_hdrOutputPipelineIndex];

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pso.pipeline);

		cmd.setViewport(0,
			vk::Viewport(
				0.0f, 0.0f,
				static_cast<float>(swapChainExtent.width),
				static_cast<float>(swapChainExtent.height),
				0.0f, 1.0f));

		cmd.setScissor(0,
			vk::Rect2D(
				vk::Offset2D(0, 0),
				swapChainExtent));

		cmd.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			pso.layout,
			0,
			*_frames[_currentFrame].globalDescriptorSet,
			nullptr);

		cmd.draw(3, 1, 0, 0);

#if USE_DEAR_IMGUI_INTERFACE
		ImGui::Render();
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *cmd);
#endif

		cmd.endRendering();
	}
	// Tone map pass END


	// If requested screenshot, blit over the image; also handles swapchain image transition
	if (_programCtx.GetRequestScreenshot())
	{
		CaptureScreenshot(imageIndex);
	}
	else
	{
		// After rendering, transition the swapchain image to PRESENT_SRC
		TransitionImageLayout(_pSwapChainCtx->GetSwapChainImage(imageIndex),
			vk::ImageLayout::eColorAttachmentOptimal,
			vk::ImageLayout::ePresentSrcKHR,
			vk::AccessFlagBits2::eColorAttachmentWrite,                // srcAccessMask
			{},                                                        // dstAccessMask
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
			vk::PipelineStageFlagBits2::eNone,							// dstStage
			vk::ImageAspectFlagBits::eColor);
	}

	cmd.end();
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
	VmaMemoryUsage memUsage,
	VkImageCreateFlags imageCreationFlags,
	uint32_t arrayLayers,
	VmaAllocationCreateFlags allocFlags)
{
	VkImageCreateInfo imageInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.flags = imageCreationFlags,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = static_cast<VkFormat>(format),
		.extent = { width, height, 1 },
		.mipLevels = mipLevels,
		.arrayLayers = arrayLayers,
		.samples = static_cast<VkSampleCountFlagBits>(numSamples),
		.tiling = static_cast<VkImageTiling>(tiling),
		.usage = static_cast<VkImageUsageFlags>(usage),
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED };

	VmaAllocationCreateInfo allocCreateInfo{};
	allocCreateInfo.usage = memUsage;
	allocCreateInfo.flags = allocFlags;

	AllocatedImage result{};
	if (vmaCreateImage(_vkCtx.GetAllocator(), &imageInfo, &allocCreateInfo,
		&result.image, &result.allocation, nullptr) != VK_SUCCESS)
	{
		throw std::runtime_error("vmaCreateImage failed");
	}
	return result;
}

[[nodiscard]] vk::raii::ImageView VulkanRenderer::CreateImageView(vk::Image image, vk::Format format,
	vk::ImageAspectFlags aspectFlags, uint32_t mipLevels, vk::ImageViewType viewType, uint32_t subresourceLayerCount) const
{
	vk::ImageViewCreateInfo viewInfo{ .image = image,
									 .viewType = viewType,
									 .format = format,
									 .subresourceRange = {aspectFlags, 0, mipLevels, 0, subresourceLayerCount} };
	return vk::raii::ImageView(_vkCtx.GetDevice(), viewInfo);
}


void VulkanRenderer::TransitionImageLayout(vk::Image image, vk::ImageLayout oldLayout,
	vk::ImageLayout newLayout, uint32_t mipLevels, uint32_t layerCount)
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
		.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, layerCount} };

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

	_frames[_currentFrame].commandBuffer.pipelineBarrier2(dependency_info);
}

void VulkanRenderer::CopyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width,
	uint32_t height, uint32_t layerCount)
{
	std::unique_ptr<vk::raii::CommandBuffer> commandBuffer = BeginSingleTimeCommands();

	vk::BufferImageCopy region{ .bufferOffset = 0,
							   .bufferRowLength = 0,
							   .bufferImageHeight = 0,
							   .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, layerCount},
							   .imageOffset = {0, 0, 0},
							   .imageExtent = {width, height, 1} };

	commandBuffer->copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, { region });

	EndSingleTimeCommands(*commandBuffer);
}

void VulkanRenderer::GenerateMipmaps(vk::Image image, vk::Format imageFormat, int32_t texWidth, int32_t texHeight,
	uint32_t mipLevels, uint32_t layerCount)
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
	barrier.subresourceRange.layerCount = layerCount;
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
	auto swapChainExtent = _pSwapChainCtx->GetExtent();

	_depthImage = CreateImage(swapChainExtent.width, swapChainExtent.height, 1, vk::SampleCountFlagBits::e1, depthFormat,
		vk::ImageTiling::eOptimal, 
		vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eDepthStencilAttachment);

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
	VkResult res = vmaCreateBuffer(_vkCtx.GetAllocator(), &bufferInfo, &allocCreateInfo,
		&result.buffer, &result.allocation, &result.info);
	if (res != VK_SUCCESS)
	{
		throw std::runtime_error("vmaCreateBuffer failed: " + std::to_string(res));
	}
	return result;
}

AllocatedBuffer VulkanRenderer::CreateDeviceLocalBuffer(const void* data, vk::DeviceSize size, vk::BufferUsageFlags usage)
{
	AllocatedBuffer staging = CreateBuffer(size, vk::BufferUsageFlagBits::eTransferSrc,
		VMA_MEMORY_USAGE_AUTO,
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
	memcpy(staging.info.pMappedData, data, size);

	AllocatedBuffer result = CreateBuffer(size,
		vk::BufferUsageFlagBits::eTransferDst | usage, VMA_MEMORY_USAGE_AUTO);
	CopyBuffer(staging.buffer, result.buffer, size);
	DestroyBuffer(staging);
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
	for (auto& frame : _frames)
	{
		for (const auto& desc : bindingDescs)
		{
			// Image/sampler, skip
			if (desc.bufferSize == 0)
				continue;

			vk::BufferUsageFlags usage{};
			switch (desc.type)
			{
			case vk::DescriptorType::eUniformBuffer:
				usage = vk::BufferUsageFlagBits::eUniformBuffer;
				break;
			case vk::DescriptorType::eStorageBuffer:
				usage = vk::BufferUsageFlagBits::eStorageBuffer;
				break;
			default:
				throw std::runtime_error("unhandled buffer type");
			}

			frame.globalBuffers[desc.bindingIndex] = CreateBuffer(
				desc.bufferSize,
				usage,
				VMA_MEMORY_USAGE_AUTO,
				VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
		}
	}
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
	// Render-finished semaphores: waited on by present, tied to swapchain images -> per IMAGE
	_renderFinishedSemaphores.clear();

	for (size_t i = 0; i < _pSwapChainCtx->GetSwapChainImagesSize(); i++)
	{
		_renderFinishedSemaphores.emplace_back(_vkCtx.GetDevice(), vk::SemaphoreCreateInfo());
	}

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		_frames[i].presentCompleteSemaphore = vk::raii::Semaphore(_vkCtx.GetDevice(), vk::SemaphoreCreateInfo());
		_frames[i].inFlightFence = vk::raii::Fence(_vkCtx.GetDevice(), vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled });
	}
}

std::vector<char> VulkanRenderer::ReadFile(const std::string& filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open())
		throw std::runtime_error("Failed to open file: " + filename);

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

// Deprecated
//vk::SampleCountFlagBits VulkanRenderer::GetMaxUsableSampleCount() const
//{
//	vk::PhysicalDeviceProperties physicalDeviceProperties = _vkCtx.GetPhysicalDevice().getProperties();
//
//	vk::SampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts &
//		physicalDeviceProperties.limits.framebufferDepthSampleCounts;
//
//	if (counts & vk::SampleCountFlagBits::e64)
//		return vk::SampleCountFlagBits::e64;
//	if (counts & vk::SampleCountFlagBits::e32)
//		return vk::SampleCountFlagBits::e32;
//	if (counts & vk::SampleCountFlagBits::e16)
//		return vk::SampleCountFlagBits::e16;
//	if (counts & vk::SampleCountFlagBits::e8)
//		return vk::SampleCountFlagBits::e8;
//	if (counts & vk::SampleCountFlagBits::e4)
//		return vk::SampleCountFlagBits::e4;
//	if (counts & vk::SampleCountFlagBits::e2)
//		return vk::SampleCountFlagBits::e2;
//
//	return vk::SampleCountFlagBits::e1;
//}

void VulkanRenderer::CreateColorResources()
{
	//vk::Format colorFormat = _swapChainSurfaceFormat.format;
	vk::Format hdrFormat = vk::Format::eR16G16B16A16Sfloat;

	const auto swapChainExtent = _pSwapChainCtx->GetExtent();

	_hdrColorImage = CreateImage(swapChainExtent.width, swapChainExtent.height, 1, vk::SampleCountFlagBits::e1, hdrFormat,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment);  //Also need transient?

	_hdrColorImageView = CreateImageView(_hdrColorImage.image, hdrFormat, vk::ImageAspectFlagBits::eColor, 1);
}

void VulkanRenderer::CreateShadowMapResources()
{
	//TODO: Calculate shadow caster count
	for (size_t i = 0; i < 3; i++)
	{
		//TODO: shadow map resolution (vary based on setting & light type)
		_shadowMapImages.emplace_back(CreateImage(SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION, 1, vk::SampleCountFlagBits::e1, vk::Format::eD32Sfloat,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled));

		_shadowMapImageViews.emplace_back(CreateImageView(_shadowMapImages[i].image, vk::Format::eD32Sfloat, vk::ImageAspectFlagBits::eDepth, 1));
	}

	// For viewing the shadow map in imgui window
	//_shadowMapImGuiDS = ImGui_ImplVulkan_AddTexture(_textureManger.GetTextureSampler(0), *_shadowMapImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	for (size_t i = 0; i < 2; i++)
	{
		_shadowCubeMapImages.emplace_back(
			CreateImage(SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION, 1, vk::SampleCountFlagBits::e1,
				vk::Format::eD32Sfloat,
				vk::ImageTiling::eOptimal,
				vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
				VMA_MEMORY_USAGE_AUTO,
				VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
				6));

		_shadowCubeMapImageViews.emplace_back(
			CreateImageView(_shadowCubeMapImages[i].image, vk::Format::eD32Sfloat,
				vk::ImageAspectFlagBits::eDepth, 1, vk::ImageViewType::eCube, 6));
	}

	//TODO: Separate this out too? No need to recreate
	vk::PhysicalDeviceProperties properties = _vkCtx.GetPhysicalDevice().getProperties();
	//TODO: double check this comparison creation info
	vk::SamplerCreateInfo samplerInfo{ .magFilter = vk::Filter::eLinear,
										.minFilter = vk::Filter::eLinear,
										.mipmapMode = vk::SamplerMipmapMode::eLinear,
										.addressModeU = vk::SamplerAddressMode::eClampToBorder,
										.addressModeV = vk::SamplerAddressMode::eClampToBorder,
										.addressModeW = vk::SamplerAddressMode::eClampToBorder,
										.mipLodBias = 0.0f,
										.anisotropyEnable = vk::False,
										.compareEnable = vk::True,
										.compareOp = vk::CompareOp::eGreaterOrEqual,  // Reverse Z
										.maxLod = vk::LodClampNone };
	//TODO: check out border color

	_shadowMapSampler = vk::raii::Sampler(_vkCtx.GetDevice(), samplerInfo);
}

void VulkanRenderer::HotReloadShaders()
{
	std::filesystem::path batPath = std::filesystem::path(PROJECT_ROOT) / "shaders/hlslHotReload.bat";
	std::string cmd = "\"" + batPath.string() + "\"";
	int result = std::system(cmd.c_str());

	_vkCtx.GetDevice().waitIdle();

	CreatePipelines();

	_vkCtx.GetDevice().waitIdle();
}

void VulkanRenderer::SaveScreenshot()
{
	FrameData& frame = _frames[_currentFrame];
	const auto swapChainExtent = _pSwapChainCtx->GetExtent();

	while (vk::Result::eTimeout == _vkCtx.GetDevice().waitForFences(*frame.inFlightFence, vk::True, UINT64_MAX))
		;

	VmaAllocationInfo allocInfo;
	vmaGetAllocationInfo(_vkCtx.GetAllocator(), _screenshotImage.allocation, &allocInfo);

	vk::SubresourceLayout layout = (*_vkCtx.GetDevice()).getImageSubresourceLayout(
		vk::Image(_screenshotImage.image),
		vk::ImageSubresource{ .aspectMask = vk::ImageAspectFlagBits::eColor },
		*_vkCtx.GetDevice().getDispatcher()
	);

	std::filesystem::create_directories("screenshots");

	auto now = std::chrono::system_clock::now();
	// Truncate to nearest whole second; Explicit use "-" to connect time
	std::string timeStr = std::format("{:%F-%H-%M-%S}", floor<std::chrono::seconds>(now));

	std::string pathName = std::format("screenshots/Screenshot-{}.png", timeStr);


	if (stbi_write_png(pathName.c_str(), swapChainExtent.width, swapChainExtent.height, 4, 
		(const char*)allocInfo.pMappedData + layout.offset, layout.rowPitch))
	{
		printf("Screenshot saved at: %s\n", pathName.c_str());
	}
	else
	{
		printf("Screenshot failed to save!\n");
	}
	
	DestroyImage(_screenshotImage);
}

void VulkanRenderer::CaptureScreenshot(uint32_t imageIndex)
{
	vk::Image srcImage = _pSwapChainCtx->GetSwapChainImage(imageIndex);

	const auto swapChainExtent = _pSwapChainCtx->GetExtent();

	_screenshotImage = CreateImage(swapChainExtent.width, swapChainExtent.height,
		1, 
		vk::SampleCountFlagBits::e1, 
		vk::Format::eR8G8B8A8Unorm,
		vk::ImageTiling::eLinear,
		vk::ImageUsageFlagBits::eTransferDst,
		VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO,
		0,
		1,
		VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

	TransitionImageLayout(srcImage,
		vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageLayout::eTransferSrcOptimal,
		vk::AccessFlagBits2::eMemoryRead,						// srcAccessMask
		vk::AccessFlagBits2::eTransferRead,                     // dstAccessMask
		vk::PipelineStageFlagBits2::eNone,						// srcStage
		vk::PipelineStageFlagBits2::eTransfer,                  // dstStage
		vk::ImageAspectFlagBits::eColor);

	TransitionImageLayout(_screenshotImage.image,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eTransferDstOptimal,
		{},														// srcAccessMask
		vk::AccessFlagBits2::eTransferWrite,                    // dstAccessMask
		vk::PipelineStageFlagBits2::eTransfer,					// srcStage
		vk::PipelineStageFlagBits2::eTransfer,                  // dstStage
		vk::ImageAspectFlagBits::eColor);

	vk::Offset3D blitSize;
	blitSize.x = swapChainExtent.width;
	blitSize.y = swapChainExtent.height;
	blitSize.z = 1;
	vk::ImageBlit imageBlitRegion{
		.srcSubresource{.aspectMask = vk::ImageAspectFlagBits::eColor,
						.layerCount = 1},
		.dstSubresource{.aspectMask = vk::ImageAspectFlagBits::eColor,
						.layerCount = 1},
	};

	imageBlitRegion.srcOffsets[1] = blitSize;
	imageBlitRegion.dstOffsets[1] = blitSize;

	_frames[_currentFrame].commandBuffer.blitImage
	(
		srcImage, vk::ImageLayout::eTransferSrcOptimal,
		_screenshotImage.image, vk::ImageLayout::eTransferDstOptimal,
		imageBlitRegion,
		vk::Filter::eNearest
	);

	TransitionImageLayout(srcImage,
		vk::ImageLayout::eTransferSrcOptimal,
		vk::ImageLayout::ePresentSrcKHR,
		vk::AccessFlagBits2::eTransferRead,						// srcAccessMask
		{},														// dstAccessMask
		vk::PipelineStageFlagBits2::eTransfer,					// srcStage
		vk::PipelineStageFlagBits2::eNone,						// dstStage
		vk::ImageAspectFlagBits::eColor);

	TransitionImageLayout(_screenshotImage.image,
		vk::ImageLayout::eTransferDstOptimal,
		vk::ImageLayout::eGeneral,
		vk::AccessFlagBits2::eTransferWrite,					// srcAccessMask
		vk::AccessFlagBits2::eHostRead,							// dstAccessMask
		vk::PipelineStageFlagBits2::eTransfer,					// srcStage
		vk::PipelineStageFlagBits2::eHost,						// dstStage
		vk::ImageAspectFlagBits::eColor);
}

void VulkanRenderer::CreateGBufferImages()
{
	const auto swapChainExtent = _pSwapChainCtx->GetExtent();

	assert(static_cast<size_t>(GBufferColorTargetType::Count) == _gBufferColorTargetFormats.size());

	_gBufferColorTargetImages.reserve(_gBufferColorTargetFormats.size());
	_gBufferColorTargetImageViews.reserve(_gBufferColorTargetFormats.size());

	for (size_t i = 0; i < _gBufferColorTargetFormats.size(); i++)
	{
		_gBufferColorTargetImages.emplace_back(
			CreateImage(swapChainExtent.width, swapChainExtent.height, 1, vk::SampleCountFlagBits::e1, _gBufferColorTargetFormats[i],
				vk::ImageTiling::eOptimal,
				vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment)); //Also need transient?

		_gBufferColorTargetImageViews.emplace_back(
			CreateImageView(_gBufferColorTargetImages[i].image, _gBufferColorTargetFormats[i], vk::ImageAspectFlagBits::eColor, 1));
	}
}

void VulkanRenderer::ConvertEquirectToCubeMap()
{
	// Reuse shadow map ubo for this
	FrameData& frame = _frames[_currentFrame];
	std::vector<glm::mat4> viewProjMatrices;
	viewProjMatrices.reserve(6);
	glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);  //no need for reverse z on this one?

	// +X
	glm::vec3 eye = glm::vec3(0.0f);
	glm::vec3 dir = glm::vec3(1.0f, 0.0f, 0.0f);
	glm::vec3 up = glm::vec3(0.0f, -1.0f, 0.0f);
	glm::mat4 view = glm::lookAt(eye, eye + dir, up);
	viewProjMatrices.emplace_back(proj * view);

	// -X
	dir = glm::vec3(-1.0f, 0.0f, 0.0f);
	view = glm::lookAt(eye, eye + dir, up);
	viewProjMatrices.emplace_back(proj * view);

	// +Y
	dir = glm::vec3(0.0f, 1.0f, 0.0f);
	up = glm::vec3(0.0f, 0.0f, 1.0f);
	view = glm::lookAt(eye, eye + dir, up);
	viewProjMatrices.emplace_back(proj * view);

	// -Y
	dir = glm::vec3(0.0f, -1.0f, 0.0f);
	up = glm::vec3(0.0f, 0.0f, -1.0f);
	view = glm::lookAt(eye, eye + dir, up);
	viewProjMatrices.emplace_back(proj * view);

	// +Z
	dir = glm::vec3(0.0f, 0.0f, 1.0f);
	up = glm::vec3(0.0f, -1.0f, 0.0f);
	view = glm::lookAt(eye, eye + dir, up);
	viewProjMatrices.emplace_back(proj * view);

	// -Z
	dir = glm::vec3(0.0f, 0.0f, -1.0f);
	up = glm::vec3(0.0f, -1.0f, 0.0f);
	view = glm::lookAt(eye, eye + dir, up);
	viewProjMatrices.emplace_back(proj * view);
	
	memcpy(frame.Mapped(GlobalBinding::ShadowMapUBO), viewProjMatrices.data(), viewProjMatrices.size() * sizeof(glm::mat4));

	const Texture& cubemapTex = _textureManger.GetTexture(cubemapRTHandle.index);
	const Texture& equirectTex = _textureManger.GetTexture(equirectHandle.index);
	

	const auto& cmd = frame.commandBuffer;
	vk::CommandBufferBeginInfo beginInfo{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
	
	
	cmd.begin(beginInfo);

	// Transition the skybox image to write and color attachment optimal
	TransitionImageLayout(cubemapTex.texImage.image,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eColorAttachmentOptimal,
		{},
		vk::AccessFlagBits2::eColorAttachmentWrite,
		{},
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::ImageAspectFlagBits::eColor);


	vk::ClearValue clearColor = vk::ClearColorValue(0.015f, 0.015f, 0.015f, 1.0f);


	vk::RenderingAttachmentInfo colorAttachment = { .imageView = cubemapTex.texImageView,
												   .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
												   .resolveMode = vk::ResolveModeFlagBits::eNone,
												   .loadOp = vk::AttachmentLoadOp::eClear,
												   .storeOp = vk::AttachmentStoreOp::eStore,
												   .clearValue = clearColor };

	//TODO: skybox resolution?
	vk::RenderingInfo renderingInfo = { .renderArea = {.offset = {0, 0}, .extent = vk::Extent2D(2048, 2048)},
									   .layerCount = 6,
									   .viewMask = 0b00111111,
									   .colorAttachmentCount = 1,
									   .pColorAttachments = &colorAttachment,
									   .pDepthAttachment = nullptr };


	cmd.beginRendering(renderingInfo);

	const PipelineEntry& pso = _pipelines[_equirectToCubePipelineIndex];

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pso.pipeline);

	cmd.setViewport(0,
		vk::Viewport(
			0.0f, 0.0f,
			static_cast<float>(2048),
			static_cast<float>(2048),
			0.0f, 1.0f));

	cmd.setScissor(0,
		vk::Rect2D(
			vk::Offset2D(0, 0),
			vk::Extent2D(2048, 2048)));

	cmd.setCullMode(vk::CullModeFlagBits::eNone);
	cmd.setFrontFace(vk::FrontFace::eCounterClockwise);
	cmd.setDepthTestEnable(vk::False);
	cmd.setDepthWriteEnable(vk::False);
	// Reverse Z, use Greater instead
	cmd.setDepthCompareOp(vk::CompareOp::eGreater);

	PerDrawPC pc{ equirectHandle.index, equirectTex.samplerIndex };

	const Mesh& mesh = _meshManager.GetMesh(_meshManager.GetHandle("unit_cube"));

	cmd.bindVertexBuffers(0, vk::Buffer(mesh.vertexBuffer.buffer), { 0 });

	cmd.bindIndexBuffer(vk::Buffer(mesh.indexBuffer.buffer), 0, vk::IndexType::eUint32);

	cmd.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		pso.layout,
		0,
		*_frames[_currentFrame].globalDescriptorSet,
		nullptr);

	cmd.pushConstants<PerDrawPC>(
		pso.layout,
		vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
		0,
		pc
	);

	cmd.drawIndexed(static_cast<uint32_t>(mesh.indexCount), 1, 0, 0, 0);

	cmd.endRendering();

	TransitionImageLayout(cubemapTex.texImage.image,
		vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageLayout::eShaderReadOnlyOptimal,
		vk::AccessFlagBits2::eColorAttachmentWrite,
		vk::AccessFlagBits2::eShaderRead,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::PipelineStageFlagBits2::eFragmentShader,
		vk::ImageAspectFlagBits::eColor);

	cmd.end();

	vk::SubmitInfo submitInfo{ .commandBufferCount = 1, .pCommandBuffers = &*cmd };
	_vkCtx.GetQueue().submit(submitInfo, nullptr);
	_vkCtx.GetQueue().waitIdle();
}

void VulkanRenderer::ConvolveIrradianceMap()
{
	// Reuse shadow map ubo for this
	FrameData& frame = _frames[_currentFrame];
	std::vector<glm::mat4> viewProjMatrices;
	viewProjMatrices.reserve(6);
	glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);  //no need for reverse z on this one?

	// +X
	glm::vec3 eye = glm::vec3(0.0f);
	glm::vec3 dir = glm::vec3(1.0f, 0.0f, 0.0f);
	glm::vec3 up = glm::vec3(0.0f, -1.0f, 0.0f);
	glm::mat4 view = glm::lookAt(eye, eye + dir, up);
	viewProjMatrices.emplace_back(proj * view);

	// -X
	dir = glm::vec3(-1.0f, 0.0f, 0.0f);
	view = glm::lookAt(eye, eye + dir, up);
	viewProjMatrices.emplace_back(proj * view);

	// +Y
	dir = glm::vec3(0.0f, 1.0f, 0.0f);
	up = glm::vec3(0.0f, 0.0f, 1.0f);
	view = glm::lookAt(eye, eye + dir, up);
	viewProjMatrices.emplace_back(proj * view);

	// -Y
	dir = glm::vec3(0.0f, -1.0f, 0.0f);
	up = glm::vec3(0.0f, 0.0f, -1.0f);
	view = glm::lookAt(eye, eye + dir, up);
	viewProjMatrices.emplace_back(proj * view);

	// +Z
	dir = glm::vec3(0.0f, 0.0f, 1.0f);
	up = glm::vec3(0.0f, -1.0f, 0.0f);
	view = glm::lookAt(eye, eye + dir, up);
	viewProjMatrices.emplace_back(proj * view);

	// -Z
	dir = glm::vec3(0.0f, 0.0f, -1.0f);
	up = glm::vec3(0.0f, -1.0f, 0.0f);
	view = glm::lookAt(eye, eye + dir, up);
	viewProjMatrices.emplace_back(proj * view);

	memcpy(frame.Mapped(GlobalBinding::ShadowMapUBO), viewProjMatrices.data(), viewProjMatrices.size() * sizeof(glm::mat4));

	const Texture& convolutionTex = _textureManger.GetTexture(convolutionHandle.index);
	const Texture& cubemapTex = _textureManger.GetTexture(cubemapRTHandle.index);


	const auto& cmd = frame.commandBuffer;
	vk::CommandBufferBeginInfo beginInfo{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit };


	cmd.begin(beginInfo);

	// Transition the skybox image to write and color attachment optimal
	TransitionImageLayout(convolutionTex.texImage.image,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eColorAttachmentOptimal,
		{},
		vk::AccessFlagBits2::eColorAttachmentWrite,
		{},
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::ImageAspectFlagBits::eColor);


	vk::ClearValue clearColor = vk::ClearColorValue(0.015f, 0.015f, 0.015f, 1.0f);


	vk::RenderingAttachmentInfo colorAttachment = { .imageView = convolutionTex.texImageView,
												   .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
												   .resolveMode = vk::ResolveModeFlagBits::eNone,
												   .loadOp = vk::AttachmentLoadOp::eClear,
												   .storeOp = vk::AttachmentStoreOp::eStore,
												   .clearValue = clearColor };

	//TODO: convolution resolution
	vk::RenderingInfo renderingInfo = { .renderArea = {.offset = {0, 0}, .extent = vk::Extent2D(32, 32)},
									   .layerCount = 6,
									   .viewMask = 0b00111111,
									   .colorAttachmentCount = 1,
									   .pColorAttachments = &colorAttachment,
									   .pDepthAttachment = nullptr };


	cmd.beginRendering(renderingInfo);

	const PipelineEntry& pso = _pipelines[_convolutionPipelineIndex];

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pso.pipeline);

	cmd.setViewport(0,
		vk::Viewport(
			0.0f, 0.0f,
			static_cast<float>(32),
			static_cast<float>(32),
			0.0f, 1.0f));

	cmd.setScissor(0,
		vk::Rect2D(
			vk::Offset2D(0, 0),
			vk::Extent2D(32, 32)));

	cmd.setCullMode(vk::CullModeFlagBits::eNone);
	cmd.setFrontFace(vk::FrontFace::eCounterClockwise);
	cmd.setDepthTestEnable(vk::False);
	cmd.setDepthWriteEnable(vk::False);
	// Reverse Z, use Greater instead
	cmd.setDepthCompareOp(vk::CompareOp::eGreater);

	PerDrawPC pc{ cubemapRTHandle.index, cubemapTex.samplerIndex };

	const Mesh& mesh = _meshManager.GetMesh(_meshManager.GetHandle("unit_cube"));

	cmd.bindVertexBuffers(0, vk::Buffer(mesh.vertexBuffer.buffer), { 0 });

	cmd.bindIndexBuffer(vk::Buffer(mesh.indexBuffer.buffer), 0, vk::IndexType::eUint32);

	cmd.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		pso.layout,
		0,
		*_frames[_currentFrame].globalDescriptorSet,
		nullptr);

	cmd.pushConstants<PerDrawPC>(
		pso.layout,
		vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
		0,
		pc
	);

	cmd.drawIndexed(static_cast<uint32_t>(mesh.indexCount), 1, 0, 0, 0);

	cmd.endRendering();

	TransitionImageLayout(convolutionTex.texImage.image,
		vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageLayout::eShaderReadOnlyOptimal,
		vk::AccessFlagBits2::eColorAttachmentWrite,
		vk::AccessFlagBits2::eShaderRead,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::PipelineStageFlagBits2::eFragmentShader,
		vk::ImageAspectFlagBits::eColor);

	cmd.end();

	vk::SubmitInfo submitInfo{ .commandBufferCount = 1, .pCommandBuffers = &*cmd };
	_vkCtx.GetQueue().submit(submitInfo, nullptr);
	_vkCtx.GetQueue().waitIdle();
}

void VulkanRenderer::PrefilterEnvironmentMap()
{
	// Reuse shadow map UBO for the 6 face view-proj matrices, same as ConvertEquirectToCubeMap / ConvolveIrradianceMap
	FrameData& frame = _frames[_currentFrame];
	std::vector<glm::mat4> viewProjMatrices;
	viewProjMatrices.reserve(6);
	glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

	glm::vec3 eye = glm::vec3(0.0f);
	{
		glm::vec3 dir = glm::vec3(1.0f, 0.0f, 0.0f);
		glm::vec3 up = glm::vec3(0.0f, -1.0f, 0.0f);
		viewProjMatrices.emplace_back(proj * glm::lookAt(eye, eye + dir, up));
		dir = glm::vec3(-1.0f, 0.0f, 0.0f);
		viewProjMatrices.emplace_back(proj * glm::lookAt(eye, eye + dir, up));
		dir = glm::vec3(0.0f, 1.0f, 0.0f); up = glm::vec3(0.0f, 0.0f, 1.0f);
		viewProjMatrices.emplace_back(proj * glm::lookAt(eye, eye + dir, up));
		dir = glm::vec3(0.0f, -1.0f, 0.0f); up = glm::vec3(0.0f, 0.0f, -1.0f);
		viewProjMatrices.emplace_back(proj * glm::lookAt(eye, eye + dir, up));
		dir = glm::vec3(0.0f, 0.0f, 1.0f); up = glm::vec3(0.0f, -1.0f, 0.0f);
		viewProjMatrices.emplace_back(proj * glm::lookAt(eye, eye + dir, up));
		dir = glm::vec3(0.0f, 0.0f, -1.0f);
		viewProjMatrices.emplace_back(proj * glm::lookAt(eye, eye + dir, up));
	}
	memcpy(frame.Mapped(GlobalBinding::ShadowMapUBO), viewProjMatrices.data(), viewProjMatrices.size() * sizeof(glm::mat4));

	const Texture& prefilterTex = _textureManger.GetTexture(prefilterHandle.index);
	const Texture& cubemapTex = _textureManger.GetTexture(cubemapRTHandle.index);

	const auto& cmd = frame.commandBuffer;
	vk::CommandBufferBeginInfo beginInfo{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
	cmd.begin(beginInfo);

	// Transition all mips of the prefilter cubemap to color attachment
	{
		vk::ImageMemoryBarrier2 barrier{
			.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
			.srcAccessMask = {},
			.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
			.oldLayout = vk::ImageLayout::eUndefined,
			.newLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = prefilterTex.texImage.image,
			.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, PREFILTER_MIP_LEVELS, 0, 6 },
		};
		vk::DependencyInfo dep{ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier };
		cmd.pipelineBarrier2(dep);
	}

	// Per-mip render (one draw per mip; each draw expands to 6 faces via SV_ViewID)
	std::vector<vk::raii::ImageView> perMipViews;
	perMipViews.reserve(PREFILTER_MIP_LEVELS);

	const PipelineEntry& pso = _pipelines[_prefilterPipelineIndex];

	const Mesh& mesh = _meshManager.GetMesh(_meshManager.GetHandle("unit_cube"));
	cmd.bindVertexBuffers(0, vk::Buffer(mesh.vertexBuffer.buffer), { 0 });
	cmd.bindIndexBuffer(vk::Buffer(mesh.indexBuffer.buffer), 0, vk::IndexType::eUint32);
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pso.layout, 0,
		*_frames[_currentFrame].globalDescriptorSet, nullptr);
	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pso.pipeline);

	for (uint32_t mip = 0; mip < PREFILTER_MIP_LEVELS; ++mip)
	{
		uint32_t mipResolution = PREFILTER_RESOLUTION >> mip;

		vk::ImageViewCreateInfo viewInfo{
			.image = prefilterTex.texImage.image,
			.viewType = vk::ImageViewType::eCube,
			.format = vk::Format::eR16G16B16A16Sfloat,
			.subresourceRange = {vk::ImageAspectFlagBits::eColor, mip, 1, 0, 6}
		};
		perMipViews.emplace_back(_vkCtx.GetDevice(), viewInfo);

		vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
		vk::RenderingAttachmentInfo colorAttachment{ .imageView = perMipViews.back(),
													 .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
													 .resolveMode = vk::ResolveModeFlagBits::eNone,
													 .loadOp = vk::AttachmentLoadOp::eClear,
													 .storeOp = vk::AttachmentStoreOp::eStore,
													 .clearValue = clearColor };

		vk::RenderingInfo renderingInfo{ .renderArea = {.offset = {0, 0}, .extent = vk::Extent2D(mipResolution, mipResolution)},
										 .layerCount = 6,
										 .viewMask = 0b00111111,
										 .colorAttachmentCount = 1,
										 .pColorAttachments = &colorAttachment,
										 .pDepthAttachment = nullptr };

		cmd.beginRendering(renderingInfo);

		cmd.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(mipResolution), static_cast<float>(mipResolution), 0.0f, 1.0f));
		cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(mipResolution, mipResolution)));
		cmd.setCullMode(vk::CullModeFlagBits::eNone);
		cmd.setFrontFace(vk::FrontFace::eCounterClockwise);
		cmd.setDepthTestEnable(vk::False);
		cmd.setDepthWriteEnable(vk::False);
		cmd.setDepthCompareOp(vk::CompareOp::eGreater);

		struct PrefilterPC { uint32_t env; uint32_t samp; float roughness; } pc{
			cubemapRTHandle.index,
			cubemapTex.samplerIndex,
			static_cast<float>(mip) / static_cast<float>(PREFILTER_MIP_LEVELS - 1)
		};
		cmd.pushConstants<PrefilterPC>(pso.layout,
			vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, pc);

		cmd.drawIndexed(static_cast<uint32_t>(mesh.indexCount), 1, 0, 0, 0);
		cmd.endRendering();
	}

	// Transition all mips to shader read
	{
		vk::ImageMemoryBarrier2 barrier{
			.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
			.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
			.dstAccessMask = vk::AccessFlagBits2::eShaderRead,
			.oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = prefilterTex.texImage.image,
			.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, PREFILTER_MIP_LEVELS, 0, 6 },
		};
		vk::DependencyInfo dep{ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier };
		cmd.pipelineBarrier2(dep);
	}

	cmd.end();
	vk::SubmitInfo submitInfo{ .commandBufferCount = 1, .pCommandBuffers = &*cmd };
	_vkCtx.GetQueue().submit(submitInfo, nullptr);
	_vkCtx.GetQueue().waitIdle();
	// perMipViews destroyed here, which is fine since queue is idle.
}

void VulkanRenderer::BakeBRDFLUT()
{
	const Texture& brdfTex = _textureManger.GetTexture(brdfLUTHandle.index);

	FrameData& frame = _frames[_currentFrame];
	const auto& cmd = frame.commandBuffer;
	vk::CommandBufferBeginInfo beginInfo{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
	cmd.begin(beginInfo);

	TransitionImageLayout(brdfTex.texImage.image,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eColorAttachmentOptimal,
		{},
		vk::AccessFlagBits2::eColorAttachmentWrite,
		{},
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::ImageAspectFlagBits::eColor);

	vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
	vk::RenderingAttachmentInfo colorAttachment{ .imageView = brdfTex.texImageView,
												 .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
												 .resolveMode = vk::ResolveModeFlagBits::eNone,
												 .loadOp = vk::AttachmentLoadOp::eClear,
												 .storeOp = vk::AttachmentStoreOp::eStore,
												 .clearValue = clearColor };

	vk::RenderingInfo renderingInfo{ .renderArea = {.offset = {0, 0}, .extent = vk::Extent2D(BRDF_LUT_RESOLUTION, BRDF_LUT_RESOLUTION)},
									 .layerCount = 1,
									 .viewMask = 0,
									 .colorAttachmentCount = 1,
									 .pColorAttachments = &colorAttachment,
									 .pDepthAttachment = nullptr };

	cmd.beginRendering(renderingInfo);

	const PipelineEntry& pso = _pipelines[_brdfLutPipelineIndex];
	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pso.pipeline);
	cmd.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(BRDF_LUT_RESOLUTION), static_cast<float>(BRDF_LUT_RESOLUTION), 0.0f, 1.0f));
	cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(BRDF_LUT_RESOLUTION, BRDF_LUT_RESOLUTION)));

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pso.layout, 0,
		*_frames[_currentFrame].globalDescriptorSet, nullptr);

	// fullscreen triangle (tonemap_vert)
	cmd.draw(3, 1, 0, 0);

	cmd.endRendering();

	TransitionImageLayout(brdfTex.texImage.image,
		vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageLayout::eShaderReadOnlyOptimal,
		vk::AccessFlagBits2::eColorAttachmentWrite,
		vk::AccessFlagBits2::eShaderRead,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::PipelineStageFlagBits2::eFragmentShader,
		vk::ImageAspectFlagBits::eColor);

	cmd.end();
	vk::SubmitInfo submitInfo{ .commandBufferCount = 1, .pCommandBuffers = &*cmd };
	_vkCtx.GetQueue().submit(submitInfo, nullptr);
	_vkCtx.GetQueue().waitIdle();
}
