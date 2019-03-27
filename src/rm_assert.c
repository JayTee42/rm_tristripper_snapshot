#include "rm_assert.h"

rm_void rm_vexit(const rm_char* format, va_list args)
{
	//Log the arguments as error if it is there:
	if (format)
	{
		rm_vlog(RM_LOG_TYPE_ERROR, format, args);
	}

	//Kill the application:
	exit(EXIT_FAILURE);
}

rm_void rm_exit(const rm_char* format, ...)
{
	//Extract the variable arguments:
	va_list args;
	va_start(args, format);

	//Delegate to rm_vexit(...):
	rm_vexit(format, args);

	//End the variable argument list:
    va_end(args);
}
