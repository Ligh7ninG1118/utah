#include "Core/UtahCtx.h"
#include <iostream>

int main()
{
	UtahCtx ctx;

	try
	{
		ctx.Run();
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
