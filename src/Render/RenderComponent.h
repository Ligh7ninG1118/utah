#pragma once
#include <cstdint>

using MeshHandle = uint32_t;
using MaterialHandle = uint32_t;


struct RenderComponent
{
	MeshHandle _mesh = 0;
	MaterialHandle _material = 0;
};