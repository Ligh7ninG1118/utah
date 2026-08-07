#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <cassert>
#include "Registry.h"

#define USE_DEAR_IMGUI_INTERFACE 1


class System;
class RenderSystem;
class FlyCameraSystem;

class UtahCtx
{
public:
	static UtahCtx& Get()
	{
		assert(_pInstance && "UtahCtx::Get() called before construction");
		return *_pInstance;
	}

	UtahCtx();
	~UtahCtx();

	void Run();

	GLFWwindow* GetContextWindow() const { return _pWindow; }
	bool GetRequestShaderReload() const { return _requestedShaderReload; }
	void SetRequestShaderReload(bool val) { _requestedShaderReload = val; }
	bool GetRequestScreenshot() const { return _requestedScreenshot; }
	void SetRequestScreenshot(bool val) { _requestedScreenshot = val; }
	void NotifyFramebufferResized();

private:
	void InitWindow();
	void InitSystems();
	void InitDemo1();
	void InitDemo2();
	void MainLoop();
	void CleanUp();

	void ToggleCursorMode();
	void ToggleFastFlyMode();

	static void MousePositionCallback(GLFWwindow* window, double xpos, double ypos);
	static void KeyInputCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

	static UtahCtx* _pInstance;

	GLFWwindow* _pWindow = nullptr;

	std::vector<System*> _systems;

	RenderSystem* _pRenderSystem;

	FlyCameraSystem* _pFlyCamera;

	Registry _registry;

	double _lastFrameTimestamp = 0.0;

	// Only update telemetry every 1s
	float _telemetryUpdateInterval = 1.0f;
	float _telemetryUpdateTimer = 0.0f;
	float _telemetryDeltaTime = 0.0f;

	// Using cursor to interact with ImGUI, and block camera input
	bool _isCursorMode = false;

	bool _requestedShaderReload = false;

	bool _requestedScreenshot = false;

	bool _holdMode = false;
};
