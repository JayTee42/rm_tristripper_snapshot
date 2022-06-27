#ifndef __RM_LOG_H__
#define __RM_LOG_H__

#include <stdarg.h>

#include "rm_type.h"

//Fancy console colors ... thx, StackOverflow :)
#define RM_LOG_COLOR_NORMAL  "\x1B[0m"
#define RM_LOG_COLOR_RED     "\x1B[31m"
#define RM_LOG_COLOR_GREEN   "\x1B[32m"
#define RM_LOG_COLOR_YELLOW  "\x1B[33m"
#define RM_LOG_COLOR_BLUE    "\x1B[34m"
#define RM_LOG_COLOR_MAGENTA "\x1B[35m"
#define RM_LOG_COLOR_CYAN    "\x1B[36m"
#define RM_LOG_COLOR_WHITE   "\x1B[37m"

//Different kinds of log info:
typedef enum __rm_log_type__
{
	RM_LOG_TYPE_INFO,
	RM_LOG_TYPE_DEBUG,
	RM_LOG_TYPE_WARNING,
	RM_LOG_TYPE_ERROR
} rm_log_type;

//Different levels of log:
typedef enum __rm_log_level__
{
	RM_LOG_LEVEL_SILENT,
	RM_LOG_LEVEL_NORMAL,
	RM_LOG_LEVEL_VERBOSE
} rm_log_level;

//Get / Set the global log level:
rm_log_level rm_log_get_level(rm_void);
rm_void rm_log_set_level(rm_log_level log_level);

//Print a log message:
rm_void rm_vlog(rm_log_type log_type, const rm_char* format, va_list args);
rm_void rm_log(rm_log_type log_type, const rm_char* format, ...);

#endif
