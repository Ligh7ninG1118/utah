#include "UtahCtx.h"
#include "Render/RenderSystem.h"
#include "Gameplay/FlyCameraSystem.h"
#include "Gameplay/TransformComponent.h"
#include "Render/RenderComponent.h"
#include "Render/CPUTypes.h"
#include "Render/MeshManager.h"
#include "Render/MaterialManager.h"
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
	InitSystems();
	InitDemoSponza();
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


#if USE_DEAR_IMGUI_INTERFACE
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	ImGui::StyleColorsDark();
#endif
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


// Multiple .obj models, blinn phong textured and pure color, multiple light types (point, directional, spot)
void UtahCtx::InitDemo1()
{
	MeshManager* meshMgr = _pRenderSystem->GetMeshManager();
	MaterialManager* matMgr = _pRenderSystem->GetMaterialManager();

	ModelHandle vikingRoomMesh = meshMgr->GetHandle("viking_room");
	ModelHandle teapotMesh = meshMgr->GetHandle("teapot");
	ModelHandle planeMesh = meshMgr->GetHandle("plane");

	MaterialHandle vikingRoomMat = matMgr->GetHandle("viking_room");
	MaterialHandle greenMat = matMgr->GetHandle("pure_green");
	MaterialHandle whiteMat = matMgr->GetHandle("pure_white");

	std::array<glm::vec3, 5> definedPos
	{
		glm::vec3(0.0f, 0.5f, 0.0f),
		glm::vec3(2.0f, 0.5f, 0.0f),
		glm::vec3(4.0f, 0.5f, 0.0f),

		glm::vec3(0.0f, 0.5f, 2.0f),
		glm::vec3(0.0f, 0.5f, 4.0f),
	};

	// Models
	for (int i = 0; i < 5; i++)
	{
		Entity e = _registry.CreateEntity();

		TransformComponent t;
		RenderComponent r;
		t._pos = definedPos[i];
		if (i < 3) // Viking room
		{
			t._rot.x = -90.0f;
			t._rot.z = 180.0f;
			r._model = vikingRoomMesh;
			r._material = vikingRoomMat;
		}
		else // Teapot
		{
			t._scale = glm::vec3(0.3f);
			r._model = teapotMesh;
			r._material = greenMat;
		}

		_registry.AddComponent<TransformComponent>(e, std::move(t));
		_registry.AddComponent<RenderComponent>(e, std::move(r));
	}

	// Floor Plane
	{
		Entity e = _registry.CreateEntity();

		TransformComponent t;
		RenderComponent r;

		t._scale = glm::vec3(50.0f, 1.0f, 50.0f);

		r._model = planeMesh;
		r._material = whiteMat;

		_registry.AddComponent<TransformComponent>(e, std::move(t));
		_registry.AddComponent<RenderComponent>(e, std::move(r));
	}

	//// Ceiling Plane
	//{
	//	Entity e = _registry.CreateEntity();

	//	TransformComponent t;
	//	RenderComponent r;
	//	t._pos.y = 5.0f;
	//	t._rot = glm::vec3(180.0f, 0.0f, 0.0f);
	//	t._scale = glm::vec3(50.0f, 1.0f, 50.0f);

	//	r._mesh = planeMesh;
	//	r._material = whiteMat;

	//	_registry.AddComponent<TransformComponent>(e, std::move(t));
	//	_registry.AddComponent<RenderComponent>(e, std::move(r));
	//}

	// Fly camera
	{
		Entity flyCam = _registry.CreateEntity();

		CameraComponent c;
		c._pos.x = -2.0f;
		c._pos.y = 2.0f;
		c._pos.z = 3.5f;
		c._rot.y = -70.0f;
		c._rot.z = -30.f;

		_registry.AddComponent<CameraComponent>(flyCam, std::move(c));
	}

	// Point lights
	for (int i = 0; i < 2; i++)
	{
		Entity e = _registry.CreateEntity();

		TransformComponent t;
		t._pos.y = 2.0f;
		t._pos.x = i * 3.0f;

		PointLightCPU p;
		p.color = glm::vec3(0.0f, 1.0f, 0.0f);
		p.intensity = 5.0f;

		_registry.AddComponent<TransformComponent>(e, std::move(t));
		_registry.AddComponent<PointLightCPU>(e, std::move(p));
	}

	// Directional Light 1
	{
		Entity e = _registry.CreateEntity();

		TransformComponent t;
		t._pos.y = 3.0f;

		DirectionalLightCPU p;
		p.pitch = -45.0f;
		p.yaw = 90.0f;

		p.color = glm::vec3(1.0f, 0.0f, 0.0f);
		p.intensity = 1.5f;


		_registry.AddComponent<TransformComponent>(e, std::move(t));
		_registry.AddComponent<DirectionalLightCPU>(e, std::move(p));
	}

	// Directional Light 2
	{
		Entity e = _registry.CreateEntity();

		TransformComponent t;
		t._pos.y = 3.0f;

		DirectionalLightCPU p;
		p.pitch = -45.0f;
		p.yaw = -90.0f;

		p.color = glm::vec3(0.0f, 0.0f, 1.0f);
		p.intensity = 2.5f;


		_registry.AddComponent<TransformComponent>(e, std::move(t));
		_registry.AddComponent<DirectionalLightCPU>(e, std::move(p));
	}

	//// SpotLight (flashlight)
	//{
	//	Entity e = _registry.CreateEntity();

	//	TransformComponent t;

	//	SpotLightCPU p;
	//	p.color = glm::vec3(1.0f);
	//	p.intensity = 20.0f;
	//	p.range = 30.0f;
	//	p.cutoff = 12.5f;
	//	p.outerCutoff = 17.5f;

	//	_registry.AddComponent<TransformComponent>(e, std::move(t));
	//	_registry.AddComponent<SpotLightCPU>(e, std::move(p));
	//}
}


// PBR, damaged helmet
void UtahCtx::InitDemo2()
{
	// Fly camera
	{
		Entity flyCam = _registry.CreateEntity();

		CameraComponent c;
		c._pos.x = -0.4;
		c._pos.y = 0.1f;
		c._pos.z = 9.0f;
		c._rot.y = -90.0f;
		c._rot.z = 0.0f;

		_registry.AddComponent<CameraComponent>(flyCam, std::move(c));
	}

	MeshManager* meshMgr = _pRenderSystem->GetMeshManager();
	MaterialManager* matMgr = _pRenderSystem->GetMaterialManager();

	ModelHandle helmetModel = meshMgr->GetHandle("helmet");
	ModelHandle bustModel = meshMgr->GetHandle("marble_bust");

	ModelHandle planeMesh = meshMgr->GetHandle("plane");
	MaterialHandle whiteMat = matMgr->GetHandle("pure_white");
	MaterialHandle redGlass = matMgr->GetHandle("red_glass");
	MaterialHandle blueGlass = matMgr->GetHandle("blue_glass");

	ModelHandle sponzaMesh = meshMgr->GetHandle("sponza");

	// Models
	// Damaged helmet
	{
		Entity e = _registry.CreateEntity();

		TransformComponent t;

		RenderComponent r;
		r._model = helmetModel;

		_registry.AddComponent<TransformComponent>(e, std::move(t));
		_registry.AddComponent<RenderComponent>(e, std::move(r));
	}
	// Marble bust
	{
		Entity e = _registry.CreateEntity();

		TransformComponent t;
		t._pos.y = -1.0f;
		t._pos.z = 5.0f;
		t._scale = glm::vec3(3.0f);

		RenderComponent r;
		r._model = bustModel;

		_registry.AddComponent<TransformComponent>(e, std::move(t));
		_registry.AddComponent<RenderComponent>(e, std::move(r));
	}
	// Red Glass
	{
		Entity e = _registry.CreateEntity();

		TransformComponent t;
		t._pos.x = -0.25f;
		t._pos.y = -0.25f;
		t._pos.z = 3.0f;
		t._scale = glm::vec3(2.0f);
		t._rot.x = 90.0f;

		RenderComponent r;
		r._model = planeMesh;
		r._material = redGlass;

		_registry.AddComponent<TransformComponent>(e, std::move(t));
		_registry.AddComponent<RenderComponent>(e, std::move(r));
	}
	// Blue Glass
	{
		Entity e = _registry.CreateEntity();

		TransformComponent t;
		t._pos.x = 0.25f;
		t._pos.y = 0.25f;
		t._pos.z = 3.5f;
		t._scale = glm::vec3(2.0f);
		t._rot.x = 90.0f;

		RenderComponent r;
		r._model = planeMesh;
		r._material = blueGlass;

		_registry.AddComponent<TransformComponent>(e, std::move(t));
		_registry.AddComponent<RenderComponent>(e, std::move(r));
	}
	// Sponza
	/*{
		Entity e = _registry.CreateEntity();

		TransformComponent t;

		RenderComponent r;
		r._model = sponzaMesh;

		_registry.AddComponent<TransformComponent>(e, std::move(t));
		_registry.AddComponent<RenderComponent>(e, std::move(r));
	}*/

	//// Floor Plane
	/*{
		Entity e = _registry.CreateEntity();

		TransformComponent t;
		RenderComponent r;
		t._pos.y = -0.66f;
		t._scale = glm::vec3(50.0f, 1.0f, 50.0f);

		r._model = planeMesh;
		r._material = whiteMat;

		_registry.AddComponent<TransformComponent>(e, std::move(t));
		_registry.AddComponent<RenderComponent>(e, std::move(r));
	}*/

	// Point lights
	{
		Entity e = _registry.CreateEntity();

		TransformComponent t;
		t._pos.y = 2.0f;
		t._pos.x = -3.0f;

		PointLightCPU p;
		p.color = glm::vec3(1.0f, 0.0f, 0.0f);
		p.intensity = 15.0f;

		_registry.AddComponent<TransformComponent>(e, std::move(t));
		_registry.AddComponent<PointLightCPU>(e, std::move(p));
	}

	{
		Entity e = _registry.CreateEntity();

		TransformComponent t;
		t._pos.y = 2.0f;
		t._pos.x = 3.0f;

		PointLightCPU p;
		p.color = glm::vec3(0.0f, 0.0f, 1.0f);
		p.intensity = 15.0f;

		_registry.AddComponent<TransformComponent>(e, std::move(t));
		_registry.AddComponent<PointLightCPU>(e, std::move(p));
	}

}

void UtahCtx::InitDemoSponza()
{
	// Fly camera
	{
		Entity flyCam = _registry.CreateEntity();

		CameraComponent c;
		c._pos.y = 1.0f;
		c._rot.y = -45.0f;

		_registry.AddComponent<CameraComponent>(flyCam, std::move(c));
	}

	MeshManager* meshMgr = _pRenderSystem->GetMeshManager();
	MaterialManager* matMgr = _pRenderSystem->GetMaterialManager();

	ModelHandle sponzaMesh = meshMgr->GetHandle("sponza");
	// Sponza
	{
		Entity e = _registry.CreateEntity();

		TransformComponent t;

		RenderComponent r;
		r._model = sponzaMesh;

		_registry.AddComponent<TransformComponent>(e, std::move(t));
		_registry.AddComponent<RenderComponent>(e, std::move(r));
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

		CameraComponent& cam = _registry.GetPool<CameraComponent>()->GetPool()[0];

#if USE_DEAR_IMGUI_INTERFACE
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGui::Begin("Telemetry", 0, ImGuiWindowFlags_AlwaysAutoResize);
		ImGui::Text("FPS: %.1f\t\t\tTotal Frame Time: %.3f ms\n", 1.0f / _telemetryDeltaTime, _telemetryDeltaTime * 1000.0f);
		ImGui::Text("POS: (%.2f, %.2f, %.2f)\t\tROT: (%.2f, %.2f, %.2f)\n", cam._pos.x, cam._pos.y, cam._pos.z, cam._rot.x, cam._rot.y, cam._rot.z);
		ImGui::End();
#endif

		/*auto& poolOwner = _registry.GetPool<PointLightCPU>()->GetEntities();
		auto* tfPool = _registry.GetPool<TransformComponent>();
		for (auto& owner : poolOwner)
		{
			static float timeCount = 0.0f;
			timeCount += deltaTime * 100.0f;
			auto& tf = tfPool->Get(owner);
			tf._pos.x = glm::cos(glm::radians(timeCount)) * 2.0f;
			tf._pos.z = glm::sin(glm::radians(timeCount)) * 2.0f;
		}*/

		_pFlyCamera->Update(deltaTime);

		// Flashlight: attach the spot light to the camera before lights are gathered
		if(!_holdMode)
		{
			auto* spotPool = _registry.GetPool<SpotLightCPU>();
			auto* tfPool = _registry.GetPool<TransformComponent>();

			for (Entity owner : spotPool->GetEntities())
			{
				tfPool->Get(owner)._pos = cam._pos;
				spotPool->Get(owner).pitch = cam._rot.z;   // camera pitch
				spotPool->Get(owner).yaw = cam._rot.y;   // camera yaw
			}
		}

		_pRenderSystem->Update(deltaTime);
	}

}

void UtahCtx::CleanUp()
{
	_pRenderSystem->WaitForRendererIdle();

#if USE_DEAR_IMGUI_INTERFACE
	ImGui_ImplVulkan_Shutdown();
#endif

	_registry.Clear();

	for (auto* sys : _systems)
		delete sys;
	_systems.clear();

#if USE_DEAR_IMGUI_INTERFACE
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
#endif

	glfwDestroyWindow(_pWindow);

	glfwTerminate();
}

void UtahCtx::ToggleCursorMode()
{
	_isCursorMode = !_isCursorMode;
	if (_isCursorMode)
		glfwSetInputMode(_pWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	else
		glfwSetInputMode(_pWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void UtahCtx::ToggleFastFlyMode()
{
	_pFlyCamera->ToggleFastFlyMode();
}

void UtahCtx::MousePositionCallback(GLFWwindow* window, double xpos, double ypos)
{
	UtahCtx* ctx = static_cast<UtahCtx*>(glfwGetWindowUserPointer(window));
	if (!ctx)
		return;

	if (!ctx->_isCursorMode)
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

	if (key == GLFW_KEY_F5 && action == GLFW_RELEASE)
	{
		ctx->SetRequestShaderReload(true);
	}

	if (key == GLFW_KEY_F12 && action == GLFW_RELEASE)
	{
		ctx->SetRequestScreenshot(true);
	}

	if (key == GLFW_KEY_F && action == GLFW_RELEASE)
	{
		ctx->_holdMode = !ctx->_holdMode;
	}

	if (key == GLFW_KEY_LEFT_SHIFT && action == GLFW_PRESS)
	{
		ctx->ToggleFastFlyMode();
	}

	if (key == GLFW_KEY_LEFT_SHIFT && action == GLFW_RELEASE)
	{
		ctx->ToggleFastFlyMode();
	}
}

void UtahCtx::NotifyFramebufferResized()
{
	_pRenderSystem->NotifyResized();
}
