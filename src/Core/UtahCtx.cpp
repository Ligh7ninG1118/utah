#include "UtahCtx.h"
#include "Render/RenderSystem.h"
#include "Gameplay/FlyCameraSystem.h"
#include "Gameplay/TransformComponent.h"
#include "Render/RenderComponent.h"
#include "Render/PointLightComponent.h"
#include <utility>
#include <iostream>

constexpr uint32_t WINDOW_WIDTH = 1920;
constexpr uint32_t WINDOW_HEIGHT = 1080;

UtahCtx* UtahCtx::_pInstance = nullptr;


UtahCtx::UtahCtx()
	: _pRenderSystem(nullptr)
{
	_pInstance = this;
}

UtahCtx::~UtahCtx()
{
	if (_pInstance == this)
		_pInstance = nullptr;
}

void UtahCtx::Run()
{
	InitWindow();
	InitDemo();
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

	glfwSetInputMode(_pWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	if (glfwRawMouseMotionSupported())
		glfwSetInputMode(_pWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

	glfwSetCursorPosCallback(_pWindow, MousePositionCallback);
	glfwSetKeyCallback(_pWindow, KeyInputCallback);
}

void UtahCtx::InitSystems()
{
	RenderSystem* renderSys = new RenderSystem();
	renderSys->Init(_registry);
	_pRenderSystem = renderSys;
	_systems.push_back(renderSys);

	FlyCameraSystem* flyCamera = new FlyCameraSystem();
	flyCamera->Init(_registry);
	_pFlyCamera = flyCamera;
	_systems.push_back(flyCamera);
}

void UtahCtx::InitDemo()
{
	// Models
	for (int i = 0; i < 5; i++)
	{
		Entity e = _registry.CreateEntity();

		TransformComponent t;
		t._pos.x = -4 + i * 2;
		t._rot.x = -90.0f;

		_registry.AddComponent<TransformComponent>(e, std::move(t));
		_registry.AddComponent<RenderComponent>(e);
	}

	// Fly camera
	Entity flyCam = _registry.CreateEntity();

	CameraComponent c;
	c._pos.x = 4.0f;
	c._pos.y = 2.0f;
	c._pos.z = -2.5f;
	c._rot.y = 130.0f;
	c._rot.z = -30.f;

	_registry.AddComponent<CameraComponent>(flyCam, std::move(c));

	// Point lights
	for (int i = 0; i < 1; i++)
	{
		Entity e = _registry.CreateEntity();

		TransformComponent t;
		t._pos.y = 5.0f;

		PointLightComponent p;
		p.ambient = glm::vec3(0.1f, 0.1f, 0.1f);
		p.diffuse = glm::vec3(0.5f, 0.5f, 0.5f);
		p.specular = glm::vec3(1.0f, 1.0f, 1.0f);

		p.constant = 1.0f;
		p.linear = 0.07f;
		p.quadratic = 0.017f;

		_registry.AddComponent<TransformComponent>(e, std::move(t));
		_registry.AddComponent<PointLightComponent>(e, std::move(p));
	}
}

void UtahCtx::MainLoop()
{
	while (!glfwWindowShouldClose(_pWindow))
	{
		glfwPollEvents();
		
		double currentTimestamp = glfwGetTime();
		double deltaTime = currentTimestamp - lastFrameTimestamp;
		lastFrameTimestamp = currentTimestamp;

		/*auto& pool = _registry.GetPool<TransformComponent>()->GetPool();
		for (auto& obj : pool)
		{
			obj._rot.z += 10.0f * deltaTime;
		}*/

		_pFlyCamera->Update(deltaTime);

		_pRenderSystem->Update(deltaTime);

		_pRenderSystem->WaitForRendererIdle();
	}

}

void UtahCtx::CleanUp() const
{
	glfwDestroyWindow(_pWindow);

	glfwTerminate();

	//TODO: Destroy all entities, systems, components
}

void UtahCtx::MousePositionCallback(GLFWwindow* window, double xpos, double ypos)
{
	UtahCtx* ctx = static_cast<UtahCtx*>(glfwGetWindowUserPointer(window));
	if (!ctx)
		return;

	ctx->_pFlyCamera->HandleMouseInput(xpos, ypos);
}

void UtahCtx::KeyInputCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	UtahCtx* ctx = static_cast<UtahCtx*>(glfwGetWindowUserPointer(window));
	if (!ctx)
		return;

	if (key == GLFW_KEY_ESCAPE && action == GLFW_RELEASE)
		glfwSetWindowShouldClose(ctx->_pWindow, GLFW_TRUE);
}