#pragma once


// Not used for now
class IRenderer
{
public:
	virtual ~IRenderer() = default;
	
	virtual void Initialize() = 0;

	virtual void DrawFrame() = 0;

	virtual void WaitForIdle() = 0;
};
