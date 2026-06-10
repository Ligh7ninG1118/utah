#pragma once
#include <cstdint>
#include "RenderCommons.h"


struct RenderComponent
{
	MeshHandle _mesh{};
	MaterialHandle _material{};
};