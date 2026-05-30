#include "UtahCtx.h"
#include "Render/RenderSystem.h"
#include "Gameplay/FlyCameraSystem.h"
#include "Gameplay/TransformComponent.h"
#include "Render/RenderComponent.h"
#include "Render/CPUTypes.h"
#include <utility>
#include <iostream>


#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

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

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	ImGui::StyleColorsDark();
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
	for (int i = 0; i < 6; i++)
	{
		Entity e = _registry.CreateEntity();

		TransformComponent t;
		RenderComponent r;
		t._pos.x = -4.0f + i * 2.0f;
		if (i % 2 == 0) // Viking room
		{
			t._rot.x = -90.0f;
			r._mesh = 0;
			r._material = 0;
		}
		else // Teapot
		{
			t._scale = glm::vec3(0.3f);
			r._mesh = 1;
			r._material = 1;
		}

		_registry.AddComponent<TransformComponent>(e, std::move(t));
		_registry.AddComponent<RenderComponent>(e, std::move(r));
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

		PointLightCPU p;
		p.ambient = glm::vec3(0.1f, 0.1f, 0.1f);
		p.diffuse = glm::vec3(0.5f, 0.5f, 0.5f);
		p.specular = glm::vec3(1.0f, 1.0f, 1.0f);

		p.constant = 1.0f;
		p.linear = 0.07f;
		p.quadratic = 0.017f;

		_registry.AddComponent<TransformComponent>(e, std::move(t));
		_registry.AddComponent<PointLightCPU>(e, std::move(p));
	}
}

void UtahCtx::MainLoop()
{
	while (!glfwWindowShouldClose(_pWindow))
	{
		glfwPollEvents();
		
		double currentTimestamp = glfwGetTime();
		double deltaTime = currentTimestamp - _lastFrameTimestamp;
		_lastFrameTimestamp = currentTimestamp;

		_telemetryUpdateTimer += deltaTime;
		// Only update telemetry display every 1s
		if (_telemetryUpdateTimer >= _telemetryUpdateInterval)
		{
			_telemetryUpdateTimer = 0.0f;
			_telemetryDeltaTime = deltaTime;
		}

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGui::Begin("Telemetry", 0, ImGuiWindowFlags_AlwaysAutoResize);
		ImGui::Text("FPS: %.1f\t\t\tTotal Frame Time: %.3f ms\n", 1.0f / _telemetryDeltaTime, _telemetryDeltaTime * 1000.0f);
		ImGui::End();

		/*auto& pool = _registry.GetPool<TransformComponent>()->GetPool();
		for (auto& obj : pool)
		{
			obj._rot.z += 10.0f * deltaTime;
		}*/

		_pFlyCamera->Update(deltaTime);

		_pRenderSystem->Update(deltaTime);
	}

}

void UtahCtx::CleanUp()
{
	_pRenderSystem->WaitForRendererIdle();
	ImGui_ImplVulkan_Shutdown();

	_registry.Clear();

	for (auto* sys : _systems)
		delete sys;
	_systems.clear();

	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(_pWindow);

	glfwTerminate();
}

void UtahCtx::ToggleCursorMode()
{
	_isCursorMode = !_isCursorMode;
	if(_isCursorMode)
		glfwSetInputMode(_pWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	else
		glfwSetInputMode(_pWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void UtahCtx::MousePositionCallback(GLFWwindow* window, double xpos, double ypos)
{
	UtahCtx* ctx = static_cast<UtahCtx*>(glfwGetWindowUserPointer(window));
	if (!ctx)
		return;

	if(!ctx->_isCursorMode)
		ctx->_pFlyCamera->HandleMouseInput(xpos, ypos);
}

void UtahCtx::KeyInputCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	UtahCtx* ctx = static_cast<UtahCtx*>(glfwGetWindowUserPointer(window));
	if (!ctx)
		return;

	if (key == GLFW_KEY_ESCAPE && action == GLFW_RELEASE)
		glfwSetWindowShouldClose(ctx->_pWindow, GLFW_TRUE);

	if (key == GLFW_KEY_F1 && action == GLFW_RELEASE)
	{
		ctx->ToggleCursorMode();
	}
}

void UtahCtx::NotifyFramebufferResized()
{
	_pRenderSystem->NotifyResized();
}
