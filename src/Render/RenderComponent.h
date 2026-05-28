#pragma once
#include <cstdint>
#include "RenderCommons.h"


struct RenderComponent
{
	uint32_t _mesh = INVALID_HANDLE;
	uint32_t _material = INVALID_HANDLE;
};