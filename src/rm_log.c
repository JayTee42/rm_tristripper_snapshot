#include "rm_log.h"

#include <time.h>

#include "rm_assert.h"
#include "rm_file.h"

//The global log level:
static rm_log_level global_log_level = RM_LOG_LEVEL_NORMAL;

//Do we use colors?
static rm_bool use_colors = true;

rm_log_level rm_log_get_level(rm_void)
{
	return global_log_level;
}

rm_void rm_log_set_level(rm_log_level log_level)
{
	global_log_level = log_level;
}

rm_void rm_vlog(rm_log_type log_type, const rm_char* format, va_list args)
{
	//Do we have to print the log message?
	//And if that is the case, how and where do we print it to?
	rm_file file = NULL;
	const rm_char* tag;
	const rm_char* color_code;

	switch (log_type)
	{
	//INFO on NORMAL / VERBOSE -> stdout:
	case RM_LOG_TYPE_INFO:

		if ((global_log_level == RM_LOG_LEVEL_NORMAL) || (global_log_level == RM_LOG_LEVEL_VERBOSE))
		{
			file = rm_stdout;
			tag = "INF";
			color_code = RM_LOG_COLOR_GREEN;
		}

		break;

	//DEBUG on VERBOSE -> stdout:
	case RM_LOG_TYPE_DEBUG:

		if (global_log_level == RM_LOG_LEVEL_VERBOSE)
		{
			file = rm_stdout;
			tag = "DBG";
			color_code = RM_LOG_COLOR_WHITE;
		}

		break;

	//WARNING on NORMAL / VERBOSE -> stderr:
	case RM_LOG_TYPE_WARNING:

		if ((global_log_level == RM_LOG_LEVEL_NORMAL) || (global_log_level == RM_LOG_LEVEL_VERBOSE))
		{
			file = rm_stderr;
			tag = "WRN";
			color_code = RM_LOG_COLOR_YELLOW;
		}

		break;

	//ERROR always -> stderr:
	case RM_LOG_TYPE_ERROR:

		file = rm_stderr;
		tag = "ERR";
		color_code = RM_LOG_COLOR_RED;

		break;

	default:

		rm_exit("Invalid log type: %d", log_type);
		break;
	}

	//Is there a valid file?
	if (file)
	{
		//Get the time:
		time_t timer = time(null);

		struct tm tm_info;
		localtime_r(&timer, &tm_info);

		//Format it:
		rm_char tm_buf[32];
		strftime(tm_buf, 32, "%Y-%m-%d %H:%M:%S", &tm_info);

		//Prompt:
		if (use_colors)
		{
			rm_file_print(file, "[%s%s%s][%s] ", color_code, tag, RM_LOG_COLOR_NORMAL, tm_buf);
		}
		else
		{
			rm_file_print(file, "[%s][%s] ", tag, tm_buf);
		}

		rm_file_vprint(file, format, args);
		rm_file_print(file, "\n");
	}
}

rm_void rm_log(rm_log_type log_type, const rm_char* format, ...)
{
	//Extract the variable arguments:
	va_list args;
	va_start(args, format);

	//Delegate to "rm_vlog(...)":
	rm_vlog(log_type, format, args);

	//End the variable argument list:
    va_end(args);
}
