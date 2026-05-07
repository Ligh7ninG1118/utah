#pragma once
#include <cstdint>

using MeshHandle = uint32_t;
using MaterialHandle = uint32_t;
constexpr uint32_t INVALID_HANDLE = 0xFFFFFFFF;


struct RenderComponent
{
	MeshHandle _mesh = 0;
	MaterialHandle _material = 0;
};