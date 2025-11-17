#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

struct Vertex
{
    glm::vec3                                pos;
    glm::vec3                                color;
    glm::vec2                                texCoord;

    static vk::VertexInputBindingDescription GetBindingDescription()
    {
        return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
        // Input rate can be changed for instanced drawing
    }

    static std::array<vk::VertexInputAttributeDescription, 3> GetAttributeDescriptions()
    {
        return {
            vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)),
            vk::VertexInputAttributeDescription(0, 1, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)),
            vk::VertexInputAttributeDescription(0, 2, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord)),
        };
    }

    bool operator==(const Vertex &other) const
    {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};

namespace std
{
template <> struct hash<Vertex>
{
    size_t operator()(Vertex const &vertex) const
    {
        return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
               (hash<glm::vec2>()(vertex.texCoord) << 1);
    }
};
} // namespace std

struct UniformBufferObject
{
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

class AppCtx
{
  public:
    AppCtx();
    ~AppCtx();

    void Run();

  private:
    // App Specific Functions
    void                      InitWindow();
    void                      InitVulkan();
    void                      MainLoop();
    void                      DrawFrame();
    void                      CleanUp();

    // Runtime Update
    void                      UpdateUniformBuffer(uint32_t currentImage);

    // Vulkan Specific Functions
    void                      CreateInstance();

    // Validation Layer & Debugging
    bool                      CheckValidationLayerSupprt();

    std::vector<const char *> GetRequiredExtensions();

    void                      PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);
    void                      SetupDebugMessenger();
    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                          const VkAllocationCallbacks *pAllocator,
                                          VkDebugUtilsMessengerEXT    *pDebugMessenger);
    void     DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
                                           const VkAllocationCallbacks *pAllocator);
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
                                                        VkDebugUtilsMessageTypeFlagsEXT             messageType,
                                                        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                        void                                       *pUserData);

    // Physical Device
    void                                  PickPhysicalDevice();
    bool                                  IsDeviceSuitable(VkPhysicalDevice device);
    bool                                  CheckDeviceExtensionSupport(VkPhysicalDevice device);

    struct QueueFamilyIndices
    {
        // use optional wrapper: no value if not assigned anything
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool                    IsComplete()
        {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);

    // Logical Device
    void               CreateLogicalDevice();

    // Surface Creation
    void               CreateSurface();

    // Swap Chain
    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR        capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR>   presentModes;
    };

    // Resize Callback
    static void             FramebufferResizeCallback(GLFWwindow *window, int width, int height);
    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR      ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
    VkPresentModeKHR        ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
    VkExtent2D              ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);
    void                    CreateSwapChain();

    // Image Views
    VkImageView     CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
    void            CreateImageViews();

    // Descriptor Creation
    void            CreateDescriptorSetLayout();
    void            CreateDescriptorPool();
    void            CreateDescriptorSets();

    // Graphics Pipeline
    void            CreateGraphicsPipeline();

    // Render Pass
    void            CreateRenderPass();

    // Framebuffers
    void            CreateFramebuffers();

    // Command Pool & Buffers
    void            CreateCommandPool();
    void            CreateCommandBuffers();
    void            RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    VkCommandBuffer BeginSingleTimeCommands();
    void            EndSingleTimeCommands(VkCommandBuffer commandBuffer);

    // Texture
    void            CreateTextureImage();
    void            CreateTextureImageView();
    void            CreateTextureSampler();
    void            CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples,
                                VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                                VkImage &image, VkDeviceMemory &imageMemory);
    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,
                               uint32_t mipLevels);
    void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    void GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

    // Model
    void LoadModel();

    // Depth Buffer
    void CreateDepthResources();
    VkFormat FindSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling,
                                 VkFormatFeatureFlags features);
    VkFormat FindDepthFormat();
    bool     HasStencilComponent(VkFormat format);

    // Buffer Creation and Data Transfer
    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer,
                      VkDeviceMemory &bufferMemory);
    void CreateVertexBuffer();
    void CreateIndexBuffer();
    void CreateUniformBuffers();
    uint32_t                         FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void                             CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    // Synchronization
    void                             CreateSyncObjects();

    // Swap Chain Recreation
    void                             CleanupSwapChain();
    void                             RecreateSwapChain();

    // Shader Loading
    std::vector<char>                ReadFile(const std::string &filename);

    VkShaderModule                   CreateShaderModule(const std::vector<char> &code);

    // Multiple Sampling
    VkSampleCountFlagBits            GetMaxUsableSampleCount();
    void                             CreateColorResources();

    // Variables
    GLFWwindow                      *_pWindow;
    vk::raii::Context                _context;
    vk::raii::Instance               _instance = nullptr;
    vk::raii::DebugUtilsMessengerEXT _debugMessenger = nullptr;
    vk::raii::PhysicalDevice         _physicalDevice = nullptr;
    vk::raii::Device                 _device = nullptr;
    vk::raii::SurfaceKHR             _surface = nullptr;

    // TODO: What's this?
    uint32_t                         _queueIndex = ~0;
    vk::raii::Queue                  _queue = nullptr;

    VkQueue                          _graphicsQueue;
    VkQueue                          _presentQueue;

    VkSwapchainKHR                   _swapChain;
    std::vector<VkImage>             _swapChainImages;
    VkFormat                         _swapChainImageFormat;
    VkExtent2D                       _swapChainExtent;
    std::vector<VkImageView>         _swapChainImageViews;
    std::vector<VkFramebuffer>       _swapChainFramebuffers;

    VkRenderPass                     _renderPass;

    VkDescriptorSetLayout            _descriptorSetLayout;
    VkDescriptorPool                 _descriptorPool;
    std::vector<VkDescriptorSet>     _descriptorSets;

    VkPipelineLayout                 _pipelineLayout;
    VkPipeline                       _graphicsPipeline;

    VkCommandPool                    _commandPool;
    std::vector<VkCommandBuffer>     _commandBuffers;

    std::vector<VkSemaphore>         _imageAvailableSemaphores;
    std::vector<VkSemaphore>         _renderFinishedSemaphores;
    std::vector<VkFence>             _inFlightFences;

    uint32_t                         _currentFrame = 0;

    bool                             _framebufferResized = false;

    VkBuffer                         _vertexBuffer;
    VkDeviceMemory                   _vertexBufferMemory;
    VkBuffer                         _indexBuffer;
    VkDeviceMemory                   _indexBufferMemory;
    std::vector<VkBuffer>            _uniformBuffers;
    std::vector<VkDeviceMemory>      _uniformBuffersMemory;
    std::vector<void *>              _uniformBuffersMapped;

    uint32_t                         _mipLevels;
    VkImage                          _textureImage;
    VkDeviceMemory                   _textureImageMemory;
    VkImageView                      _textureImageView;
    VkSampler                        _textureSampler;

    VkImage                          _depthImage;
    VkDeviceMemory                   _depthImageMemory;
    VkImageView                      _depthImageView;

    std::vector<Vertex>              _vertices;
    std::vector<uint32_t>            _indices;

    vk::SampleCountFlagBits          _msaaSamples = vk::SampleCountFlagBits::e1;
    VkImage                          _colorImage;
    VkDeviceMemory                   _colorImageMemory;
    VkImageView                      _colorImageView;
};
#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

struct Vertex
{
    glm::vec3                                pos;
    glm::vec3                                color;
    glm::vec2                                texCoord;

    static vk::VertexInputBindingDescription GetBindingDescription()
    {
        return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
        // Input rate can be changed for instanced drawing
    }

    static std::array<vk::VertexInputAttributeDescription, 3> GetAttributeDescriptions()
    {
        return {
            vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)),
            vk::VertexInputAttributeDescription(0, 1, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)),
            vk::VertexInputAttributeDescription(0, 2, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord)),
        };
    }

    bool operator==(const Vertex &other) const
    {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};

namespace std
{
template <> struct hash<Vertex>
{
    size_t operator()(Vertex const &vertex) const
    {
        return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
               (hash<glm::vec2>()(vertex.texCoord) << 1);
    }
};
} // namespace std

struct UniformBufferObject
{
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

class AppCtx
{
  public:
    AppCtx();
    ~AppCtx();

    void Run();

  private:
    // App Specific Functions
    void                      InitWindow();
    void                      InitVulkan();
    void                      MainLoop();
    void                      DrawFrame();
    void                      CleanUp();

    // Runtime Update
    void                      UpdateUniformBuffer(uint32_t currentImage);

    // Vulkan Specific Functions
    void                      CreateInstance();

    // Validation Layer & Debugging
    bool                      CheckValidationLayerSupprt();

    std::vector<const char *> GetRequiredExtensions();

    void                      SetupDebugMessenger();
    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                          const VkAllocationCallbacks *pAllocator,
                                          VkDebugUtilsMessengerEXT    *pDebugMessenger);
    void     DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
                                           const VkAllocationCallbacks *pAllocator);
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
                                                        VkDebugUtilsMessageTypeFlagsEXT             messageType,
                                                        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                        void                                       *pUserData);

    // Physical Device
    void                                  PickPhysicalDevice();
    bool                                  IsDeviceSuitable(VkPhysicalDevice device);
    bool                                  CheckDeviceExtensionSupport(VkPhysicalDevice device);

    struct QueueFamilyIndices
    {
        // use optional wrapper: no value if not assigned anything
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool                    IsComplete()
        {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);

    // Logical Device
    void               CreateLogicalDevice();

    // Surface Creation
    void               CreateSurface();

    // Swap Chain
    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR        capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR>   presentModes;
    };

    // Resize Callback
    static void             FramebufferResizeCallback(GLFWwindow *window, int width, int height);
    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR      ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
    VkPresentModeKHR        ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
    VkExtent2D              ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);
    void                    CreateSwapChain();

    // Image Views
    VkImageView     CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
    void            CreateImageViews();

    // Descriptor Creation
    void            CreateDescriptorSetLayout();
    void            CreateDescriptorPool();
    void            CreateDescriptorSets();

    // Graphics Pipeline
    void            CreateGraphicsPipeline();

    // Render Pass
    void            CreateRenderPass();

    // Framebuffers
    void            CreateFramebuffers();

    // Command Pool & Buffers
    void            CreateCommandPool();
    void            CreateCommandBuffers();
    void            RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    VkCommandBuffer BeginSingleTimeCommands();
    void            EndSingleTimeCommands(VkCommandBuffer commandBuffer);

    // Texture
    void            CreateTextureImage();
    void            CreateTextureImageView();
    void            CreateTextureSampler();
    void            CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples,
                                VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                                VkImage &image, VkDeviceMemory &imageMemory);
    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,
                               uint32_t mipLevels);
    void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    void GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

    // Model
    void LoadModel();

    // Depth Buffer
    void CreateDepthResources();
    VkFormat FindSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling,
                                 VkFormatFeatureFlags features);
    VkFormat FindDepthFormat();
    bool     HasStencilComponent(VkFormat format);

    // Buffer Creation and Data Transfer
    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer,
                      VkDeviceMemory &bufferMemory);
    void CreateVertexBuffer();
    void CreateIndexBuffer();
    void CreateUniformBuffers();
    uint32_t                             FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void                                 CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    // Synchronization
    void                                 CreateSyncObjects();

    // Swap Chain Recreation
    void                                 CleanupSwapChain();
    void                                 RecreateSwapChain();

    // Shader Loading
    static std::vector<char>                    ReadFile(const std::string &filename);

    VkShaderModule                       CreateShaderModule(const std::vector<char> &code);

    // Multiple Sampling
    vk::SampleCountFlagBits              GetMaxUsableSampleCount();
    void                                 CreateColorResources();

    // Variables
    GLFWwindow                          *_pWindow;
    vk::raii::Context                    _context;
    vk::raii::Instance                   _instance = nullptr;
    vk::raii::DebugUtilsMessengerEXT     _debugMessenger = nullptr;
    vk::raii::PhysicalDevice             _physicalDevice = nullptr;
    vk::raii::Device                     _device = nullptr;
    vk::raii::SurfaceKHR                 _surface = nullptr;

    uint32_t                             _queueIndex = ~0;
    vk::raii::Queue                      _queue = nullptr;

    vk::raii::SwapchainKHR               _swapChain = nullptr;
    std::vector<vk::Image>               _swapChainImages;
    vk::SurfaceFormatKHR                 _swapChainSurfaceFormat;
    vk::Extent2D                         _swapChainExtent;
    std::vector<vk::ImageView>           _swapChainImageViews;

    vk::raii::DescriptorSetLayout        _descriptorSetLayout = nullptr;
    vk::raii::PipelineLayout             _pipelineLayout = nullptr;
    vk::raii::Pipeline                   _graphicsPipeline = nullptr;

    vk::raii::Image                      _colorImage = nullptr;
    vk::raii::DeviceMemory               _colorImageMemory = nullptr;
    vk::raii::ImageView                  _colorImageView = nullptr;

    vk::raii::Image                      _depthImage = nullptr;
    vk::raii::DeviceMemory               _depthImageMemory = nullptr;
    vk::raii::ImageView                  _depthImageView = nullptr;

    uint32_t                             _mipLevels = 0;
    vk::raii::Image                      _textureImage = nullptr;
    vk::raii::DeviceMemory               _textureImageMemory = nullptr;
    vk::raii::ImageView                  _textureImageView = nullptr;
    vk::raii::Sampler                    _textureSampler = nullptr;

    std::vector<Vertex>                  _vertices;
    std::vector<uint32_t>                _indices;
    vk::raii::Buffer                     _vertexBuffer = nullptr;
    vk::raii::DeviceMemory               _vertexBufferMemory = nullptr;

    vk::raii::Buffer                     _indexBuffer = nullptr;
    vk::raii::DeviceMemory               _indexBufferMemory = nullptr;

    std::vector<vk::raii::Buffer>        _uniformBuffers;
    std::vector<vk::raii::DeviceMemory>  _uniformBuffersMemory;
    std::vector<void *>                  _uniformBuffersMapped;

    vk::raii::DescriptorPool             _descriptorPool = nullptr;
    std::vector<vk::raii::DescriptorSet> _descriptorSets;

    vk::raii::CommandPool                _commandPool = nullptr;
    std::vector<vk::raii::CommandBuffer> _commandBuffers;

    std::vector<vk::raii::Semaphore>     _presentCompleteSemaphores;
    std::vector<vk::raii::Semaphore>     _renderFinishedSemaphores;
    std::vector<vk::raii::Fence>         _inFlightFences;

    uint32_t                             _semaphoreIndex;
    uint32_t                             _currentFrame = 0;

    bool                                 _framebufferResized = false;

    vk::SampleCountFlagBits              _msaaSamples = vk::SampleCountFlagBits::e1;
};
