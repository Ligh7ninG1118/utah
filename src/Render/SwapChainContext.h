#pragma once
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1

#include <vulkan/vulkan_raii.hpp>

class VulkanContext;

class SwapChainContext
{
public:
	SwapChainContext(VulkanContext& vkCtx, vk::SwapchainKHR oldSwapChain = VK_NULL_HANDLE);

	[[nodiscard]] std::pair<vk::Result, uint32_t> AcquireNextImage(uint64_t timeout, vk::Semaphore semaphore, vk::Fence fence);
	[[nodiscard]] vk::Result Present(const vk::raii::Queue& queue, vk::Semaphore waitSemaphore, uint32_t imageIndex);

	[[nodiscard]] vk::SwapchainKHR GetHandle() const { return *_swapChain; } // for resizing 
	[[nodiscard]] size_t GetSwapChainImagesSize() const { return _swapChainImages.size(); }
	[[nodiscard]] const vk::Image GetSwapChainImage(uint32_t index) { return _swapChainImages[index]; }
	[[nodiscard]] const vk::ImageView GetSwapChainImageView(uint32_t index) { return *_swapChainImageViews[index]; }
	[[nodiscard]] const vk::SurfaceFormatKHR GetSurfaceFormat() const { return _swapChainSurfaceFormat; }
	[[nodiscard]] const vk::Extent2D GetExtent() const { return _swapChainExtent; }

private:
	void CreateSwapChain(const VulkanContext& vkCtx, vk::SwapchainKHR oldSwapChain);
	void CreateImageViews(const VulkanContext& vkCtx);

	[[nodiscard]] vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
	[[nodiscard]] vk::PresentModeKHR ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
	[[nodiscard]] uint32_t ChooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const& surfaceCapabilities);
	[[nodiscard]] vk::Extent2D ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) const;

	vk::raii::SwapchainKHR _swapChain = nullptr;
	std::vector<vk::Image> _swapChainImages;
	std::vector<vk::raii::ImageView> _swapChainImageViews;
	vk::SurfaceFormatKHR _swapChainSurfaceFormat;
	vk::Extent2D _swapChainExtent;
};