#include "SwapChainContext.h"
#include "VulkanContext.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "Core/UtahCtx.h"

SwapChainContext::SwapChainContext(VulkanContext& vkCtx, vk::SwapchainKHR oldSwapChain)
{
	CreateSwapChain(vkCtx, oldSwapChain);
	CreateImageViews(vkCtx);
}

std::pair<vk::Result, uint32_t> SwapChainContext::AcquireNextImage(uint64_t timeout, vk::Semaphore semaphore, vk::Fence fence)
{
	return _swapChain.acquireNextImage(timeout, semaphore, fence);
}

vk::Result SwapChainContext::Present(const vk::raii::Queue& queue, vk::Semaphore waitSemaphore, uint32_t imageIndex)
{
	const vk::PresentInfoKHR info{
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &waitSemaphore,
		.swapchainCount = 1,
		.pSwapchains = &*_swapChain,
		.pImageIndices = &imageIndex
	};
	return queue.presentKHR(info);
}

void SwapChainContext::CreateSwapChain(const VulkanContext& vkCtx, vk::SwapchainKHR oldSwapChain)
{
	auto surfaceCapabilities = vkCtx.GetPhysicalDevice().getSurfaceCapabilitiesKHR(vkCtx.GetSurface());
	_swapChainExtent = ChooseSwapExtent(surfaceCapabilities);
	_swapChainSurfaceFormat = ChooseSwapSurfaceFormat(vkCtx.GetPhysicalDevice().getSurfaceFormatsKHR(vkCtx.GetSurface()));

	vk::SwapchainCreateInfoKHR swapChainCreateInfo{
		.surface = vkCtx.GetSurface(),
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
		.presentMode = ChooseSwapPresentMode(vkCtx.GetPhysicalDevice().getSurfacePresentModesKHR(vkCtx.GetSurface())),
		.clipped = true,
		.oldSwapchain = oldSwapChain};

	_swapChain = vk::raii::SwapchainKHR(vkCtx.GetDevice(), swapChainCreateInfo);
	_swapChainImages = _swapChain.getImages();
}

void SwapChainContext::CreateImageViews(const VulkanContext& vkCtx)
{
	assert(_swapChainImageViews.empty());

	vk::ImageViewCreateInfo imageViewCreateInfo{ .viewType = vk::ImageViewType::e2D,
												.format = _swapChainSurfaceFormat.format,
												.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1} };

	for (auto image : _swapChainImages)
	{
		imageViewCreateInfo.image = image;
		_swapChainImageViews.emplace_back(vkCtx.GetDevice(), imageViewCreateInfo);
	}
}

vk::SurfaceFormatKHR SwapChainContext::ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
{
	assert(!availableFormats.empty());

	const auto formatIt = std::ranges::find_if(availableFormats, [](const auto& format) {
		return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
		});

	return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
}

vk::PresentModeKHR SwapChainContext::ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
{
	assert(std::ranges::any_of(availablePresentModes,
		[](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }));

	return std::ranges::any_of(availablePresentModes,
		[](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; }) ?
		vk::PresentModeKHR::eMailbox :
		vk::PresentModeKHR::eFifo;
}

uint32_t SwapChainContext::ChooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const& surfaceCapabilities)
{
	auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
	if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount))
	{
		minImageCount = surfaceCapabilities.maxImageCount;
	}
	return minImageCount;
}

[[nodiscard]] vk::Extent2D SwapChainContext::ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) const
{
	if (capabilities.currentExtent.width != 0xFFFFFFFF)
		return capabilities.currentExtent;

	int width, height;
	glfwGetFramebufferSize(UtahCtx::Get().GetContextWindow(), &width, &height);

	return { std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
			std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height) };
}
