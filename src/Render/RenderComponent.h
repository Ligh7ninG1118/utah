#pragma once
#include <cstdint>
#include "RenderCommons.h"


struct RenderComponent
{
	ModelHandle _model{};
	MaterialHandle _material{};
};