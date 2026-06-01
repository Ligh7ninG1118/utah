#pragma once
#include <cstdint>
#include <glm/glm.hpp>

constexpr uint32_t WINDOW_WIDTH = 1920;
constexpr uint32_t WINDOW_HEIGHT = 1080;

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

constexpr int MAX_OBJECTS = 10000;

constexpr int MAX_TEXTURES = 1024;

constexpr uint32_t MAX_POINT_LIGHTS = 32;
constexpr uint32_t MAX_DIR_LIGHTS = 4;
constexpr uint32_t MAX_SPOT_LIGHTS = 32;

constexpr uint32_t INVALID_HANDLE = 0xFFFFFFFF;

struct Plane
{
	glm::vec3 _normal;
	float _distance;
};