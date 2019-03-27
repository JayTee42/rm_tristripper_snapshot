#ifndef __RM_ASSERT_H__
#define __RM_ASSERT_H__

#include <stdarg.h>
#include <stdlib.h>

#include "rm_log.h"
#include "rm_macro.h"
#include "rm_type.h"

//Print the given formatted error string to stderr.
//Terminate the application and return a bad exit code.
rm_void rm_vexit(const rm_char* format, va_list args) rm_no_return;
rm_void rm_exit(const rm_char* format, ...) rm_no_return;

//Perform an exit if the given condition is not met.
//This is only evaluated in debug builds!
#ifdef DEBUG_BUILD
#define rm_assert(condition, format, ...)                                                     	\
({                                                                                            	\
	if (rm_unlikely(!(condition)))                                                            	\
	{                                                                                         	\
		rm_log(RM_LOG_TYPE_ERROR, "Assertion triggered (%s at line %d):", __FILE__, __LINE__);	\
		rm_exit((format), ##__VA_ARGS__);                                                     	\
	}                                                                                         	\
})
#else
#define rm_assert(condition, format, ...)
#endif

//Perform an exit if the given condition is not met.
//This is always evaluated!
#ifdef DEBUG_BUILD
#define rm_precond(condition, format, ...)                                                       	\
({                                                                                               	\
	if (rm_unlikely(!(condition)))                                                               	\
	{                                                                                            	\
		rm_log(RM_LOG_TYPE_ERROR, "Precondition triggered (%s at line %d):", __FILE__, __LINE__);	\
		rm_exit((format), ##__VA_ARGS__);                                                        	\
	}                                                                                            	\
})
#else
#define rm_precond(condition, format, ...)	\
({                                        	\
	if (rm_unlikely(!(condition)))        	\
	{                                     	\
		rm_exit((format), ##__VA_ARGS__); 	\
	}                                     	\
})
#endif

#endif
