#include <iostream>
#include "AppCtx.h"


int main()
{
    AppCtx ctx;

	try
	{
		ctx.Run();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}


    return EXIT_SUCCESS;
}
