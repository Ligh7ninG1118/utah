#pragma once
#include <cstdint>
#include "RenderCommons.h"


struct RenderComponent
{
	ModelHandle _mesh{};
	MaterialHandle _material{};
};