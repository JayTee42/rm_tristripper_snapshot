#ifndef __RM_FILE_H__
#define __RM_FILE_H__

#include <errno.h>

//Make all format codes always accessible:
#include <inttypes.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "rm_assert.h"
#include "rm_macro.h"
#include "rm_type.h"

typedef FILE* rm_file;
typedef off_t rm_file_offset;

//Files for stdin, stdout and stderr:
#define rm_stdin ((rm_file)stdin)
#define rm_stdout ((rm_file)stdout)
#define rm_stderr ((rm_file)stderr)

typedef enum __rm_file_mode__
{
	//Open file for input operations.
	//The file must exist.
	RM_FILE_MODE_READ,

	//Create an empty file for output operations.
	//If a file with the same name already exists,
	// its contents are discarded and the file is treated as a new empty file.
	RM_FILE_MODE_WRITE,

	//Open file for output at the end of a file.
	//Output operations always write data at the end of the file, expanding it.
	//Repositioning operations (fseek, fsetpos, rewind) are ignored.
	//The file is created if it does not exist.
	RM_FILE_MODE_APPEND,

	//Open a file for update (both for input and output).
	//The file must exist.
	RM_FILE_MODE_READ_UPDATE,

	//Create an empty file and open it for update (both for input and output).
	//If a file with the same name already exists,
	// its contents are discarded and the file is treated as a new empty file.
	RM_FILE_MODE_WRITE_UPDATE,

	//Open a file for update (both for input and output) with all output operations writing data at the end of the file.
	//Repositioning operations (fseek, fsetpos, rewind) affects the next input operations,
	// but output operations move the position back to the end of file.
	//The file is created if it does not exist.
	RM_FILE_MODE_APPEND_UPDATE
} rm_file_mode;

typedef enum __rm_file_enc__
{
	//Open as text file:
	RM_FILE_ENC_TEXT,

	//Open as binary file:
	RM_FILE_ENC_BINARY
} rm_file_enc;

typedef enum __rm_file_seek_origin__
{
	RM_FILE_SEEK_ORIGIN_START,
	RM_FILE_SEEK_ORIGIN_CURRENT,
	RM_FILE_SEEK_ORIGIN_END
} rm_file_seek_origin;

//Open a file:
rm_file rm_must_check rm_file_open(const rm_char* path, rm_file_mode mode, rm_file_enc enc);

//Close a file:
rm_void rm_file_close(rm_file file);

//Seek in a file:
rm_void rm_file_seek(rm_file file, rm_file_offset offset, rm_file_seek_origin origin);

//Get the current offset in an opened file:
rm_file_offset rm_file_tell(rm_file file);

//Flush a file opened for writing:
rm_void rm_file_flush(rm_file file);

//Read "size" bytes from "file" to "dest_ptr".
//The number of read bytes is returned. If it is != "size", the EOF has been reached prematurely.
//If the function returns, reading has succeeded (IO errors automatically trigger a precondition).
inline rm_size rm_file_read(rm_file file, rm_void* dest_ptr, rm_size size);

//Read up to "count - 1" characters into "buf" until EoF or a newline is reached.
//Append a binary zero after that.
//The newline character will still be integrated is present (see "fgets(...)" documentation).
//If the line has successfully been read, "true" is returned.
//If we have reached the end of the file, "false" is returned and "buf" is unchanged.
//Other errors are caught via preconditions.
inline rm_bool rm_file_read_line(rm_file file, rm_char* buf, rm_size count);

//Write formatted text to the given file.
inline rm_void rm_file_vprint(rm_file file, const rm_char* format, va_list args);
inline rm_void rm_file_print(rm_file file, const rm_char* format, ...);

//Write "size" bytes from "src_ptr" to "file".
//If the function returns, reading has succeeded (IO errors automatically trigger a precondition).
inline rm_void rm_file_write(rm_file file, const rm_void* src_ptr, rm_size size);

//Write different integer types to "file".
//All functions are writing in LE order.
//If the function returns, writing has succeeded (IO errors automatically trigger a precondition).
inline rm_void rm_file_write_uint8(rm_file file, rm_uint8 value);
inline rm_void rm_file_write_int8(rm_file file, rm_int8 value);
inline rm_void rm_file_write_uint16(rm_file file, rm_uint16 value);
inline rm_void rm_file_write_int16(rm_file file, rm_int16 value);
inline rm_void rm_file_write_uint32(rm_file file, rm_uint32 value);
inline rm_void rm_file_write_int32(rm_file file, rm_int32 value);
inline rm_void rm_file_write_uint64(rm_file file, rm_uint64 value);
inline rm_void rm_file_write_int64(rm_file file, rm_int64 value);

inline rm_size rm_file_read(rm_file file, rm_void* dest_ptr, rm_size size)
{
	//Try to read all the bytes:
	rm_size bytes_read = fread(dest_ptr, 1, size, file);
	rm_precond((bytes_read == size) || feof(file), "Error while reading from file: %s", strerror(errno));

	return bytes_read;
}

inline rm_bool rm_file_read_line(rm_file file, rm_char* buf, rm_size count)
{
	//If "fgets(...)" succeeds, we are done:
	if (fgets(buf, (rm_int)count, file))
	{
		return true;
	}

	//Test for EoF:
	rm_precond(feof(file), "Error while reading line from file: %s", strerror(errno));
	return false;
}

inline rm_void rm_file_vprint(rm_file file, const rm_char* format, va_list args)
{
	//Delegate to "vfprintf(...)":
	rm_precond(vfprintf(file, format, args) >= 0, "Failed to print to file: %s", strerror(errno));
}

inline rm_void rm_file_print(rm_file file, const rm_char* format, ...)
{
	//Extract the variable arguments:
	va_list args;
	va_start(args, format);

	//Delegate to rm_file_vprint(...):
	rm_file_vprint(file, format, args);

	//End the variable argument list:
    va_end(args);
}

inline rm_void rm_file_write(rm_file file, const rm_void* src_ptr, rm_size size)
{
	//Try to write all the bytes:
	rm_size bytes_written = fwrite(src_ptr, 1, size, file);
	rm_precond(bytes_written == size, "Error while writing to file: %s", strerror(errno));
}

inline rm_void rm_file_write_uint8(rm_file file, rm_uint8 value)
{
	rm_int result = fputc((rm_int)value, file);
	rm_precond(result != EOF, "Error while writing to file: %s", strerror(errno));
}

inline rm_void rm_file_write_int8(rm_file file, rm_int8 value)
{
	rm_file_write_uint8(file, (rm_uint8)value);
}

inline rm_void rm_file_write_uint16(rm_file file, rm_uint16 value)
{
	value = rm_flip_host_to_le_16(value);
	rm_file_write(file, &value, sizeof(rm_uint16));
}

inline rm_void rm_file_write_int16(rm_file file, rm_int16 value)
{
	rm_file_write_uint16(file, (rm_uint16)value);
}

inline rm_void rm_file_write_uint32(rm_file file, rm_uint32 value)
{
	value = rm_flip_host_to_le_32(value);
	rm_file_write(file, &value, sizeof(rm_uint32));
}

inline rm_void rm_file_write_int32(rm_file file, rm_int32 value)
{
	rm_file_write_uint32(file, (rm_uint32)value);
}

inline rm_void rm_file_write_uint64(rm_file file, rm_uint64 value)
{
	value = rm_flip_host_to_le_64(value);
	rm_file_write(file, &value, sizeof(rm_uint64));
}

inline rm_void rm_file_write_int64(rm_file file, rm_int64 value)
{
	rm_file_write_uint64(file, (rm_uint64)value);
}

#endif
