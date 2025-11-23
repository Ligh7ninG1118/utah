#include "UtahCtx.h"
#include "Render/RenderSystem.h"

constexpr uint32_t WINDOW_WIDTH = 1920;
constexpr uint32_t WINDOW_HEIGHT = 1080;

UtahCtx* UtahCtx::_instance = nullptr;


UtahCtx::UtahCtx()
{
	_instance = this;
}

UtahCtx::~UtahCtx()
{
	if (_instance == this)
		_instance = nullptr;
}

void UtahCtx::Run()
{
	InitWindow();
	InitSystems();
	MainLoop();
	CleanUp();
}

void UtahCtx::InitWindow()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	_pWindow = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "utah", nullptr, nullptr);
	glfwSetWindowUserPointer(_pWindow, this);
}

void UtahCtx::InitSystems()
{
	RenderSystem* renderSys = new RenderSystem();
	_systems.push_back(renderSys);
	_renderSystem = renderSys;
}

void UtahCtx::MainLoop()
{
	while (!glfwWindowShouldClose(_pWindow))
	{
		glfwPollEvents();
		
		_renderSystem->Update(0.05f);
	}

	_renderSystem->WaitForRendererIdle();
}

void UtahCtx::CleanUp() const
{
	glfwDestroyWindow(_pWindow);

	glfwTerminate();
}