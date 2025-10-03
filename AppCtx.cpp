#include "AppCtx.h"
#include <iostream>
#include <vector>
#include <stdexcept>
#include <cstdlib>


const uint32_t WINDOW_WIDTH = 1920;
const uint32_t WINDOW_HEIGHT = 1080;


AppCtx::AppCtx()
{
}

AppCtx::~AppCtx()
{
}

void AppCtx::Run()
{
    InitWindow();
    InitVulkan();

    MainLoop();

    CleanUp();
}

void AppCtx::InitWindow()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    _pWindow = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "utah", nullptr, nullptr);
}

void AppCtx::InitVulkan()
{
    CreateInstance();
}

void AppCtx::MainLoop()
{
    while (!glfwWindowShouldClose(_pWindow))
    {
        glfwPollEvents();
    }
}

void AppCtx::CleanUp()
{
    vkDestroyInstance(_vkInstance, nullptr);

    glfwDestroyWindow(_pWindow);

    glfwTerminate();
}

void AppCtx::CreateInstance()
{
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // Desired global extensions
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtension;

    glfwExtension = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtension;

    // Global validation layers to enable
    createInfo.enabledLayerCount = 0;


    if (vkCreateInstance(&createInfo, nullptr, &_vkInstance) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create VK instance");
    }


    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    std::cout << extensionCount << " extension supported\n";

    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

    std::cout << "Available Extensions: \n";
    for (const auto& extension : extensions)
    {
        std::cout << '\t' << extension.extensionName << '\n';
    }
}
