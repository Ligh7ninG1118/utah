#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL 1
#include <glm/gtx/quaternion.hpp>

constexpr uint32_t WINDOW_WIDTH = 1920;
constexpr uint32_t WINDOW_HEIGHT = 1080;

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

constexpr uint32_t MAX_OBJECTS = 10000;
constexpr uint32_t MAX_TEXTURES = 1024;
constexpr uint32_t MAX_TEXTURE_SAMPLERS = 8;

constexpr uint32_t MAX_POINT_LIGHTS = 32;
constexpr uint32_t MAX_DIR_LIGHTS = 4;
constexpr uint32_t MAX_SPOT_LIGHTS = 32;

constexpr uint32_t SHADOW_2D_SLOT_COUNT = 4;   // dir + spot
constexpr uint32_t SHADOW_CUBE_SLOT_COUNT = 4;   // point
constexpr uint32_t CUBE_FACE_COUNT = 6;
constexpr uint32_t CUBE_FACE_VIEWMASK = 0x3F; // 0b00111111
constexpr int32_t  SHADOW_INDEX_NONE = -1;
constexpr uint32_t SHADOW_CUBE_MATRIX_BASE = SHADOW_2D_SLOT_COUNT;
constexpr uint32_t SHADOW_MATRIX_COUNT = SHADOW_2D_SLOT_COUNT + SHADOW_CUBE_SLOT_COUNT * CUBE_FACE_COUNT;

constexpr float DIR_SHADOW_ORTHO_HALF_EXTENT = 20.0f;
constexpr float DIR_SHADOW_EYE_DISTANCE = 2.0f;
constexpr float SPOT_SHADOW_FOV_PAD = 1.1f;


constexpr uint32_t INVALID_HANDLE = 0xFFFFFFFF;

struct Plane
{
	glm::vec3 _normal;
	float _distance;
};

// typed handle, preventing cross use
struct ModelHandle
{
	uint32_t index = INVALID_HANDLE;
	[[nodiscard]] bool IsValid() const { return index != INVALID_HANDLE; }
	bool operator==(const ModelHandle&) const = default;
};

struct MaterialHandle
{
	uint32_t index = INVALID_HANDLE;
	[[nodiscard]] bool IsValid() const { return index != INVALID_HANDLE; }
	bool operator==(const MaterialHandle&) const = default;
};

struct TextureHandle
{
	uint32_t index = INVALID_HANDLE;
	[[nodiscard]] bool IsValid() const { return index != INVALID_HANDLE; }
	bool operator==(const TextureHandle&) const = default;
};