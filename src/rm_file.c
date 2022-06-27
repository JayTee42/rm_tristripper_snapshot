#include "rm_file.h"

#define RM_FILE_ASSERT_OFF_T_64_BIT() rm_assert(sizeof(rm_file_offset) >= 8, "off_t is not sufficient for 64 bit offsets.");

rm_file rm_file_open(const rm_char* path, rm_file_mode mode, rm_file_enc enc)
{
	//Build a mode string for fopen:
	rm_char mode_str[4];
	rm_size mode_str_idx = 0;

	switch (mode)
	{
	case RM_FILE_MODE_READ:

		mode_str[mode_str_idx++] = 'r';
		break;

	case RM_FILE_MODE_WRITE:

		mode_str[mode_str_idx++] = 'w';
		break;

	case RM_FILE_MODE_APPEND:

		mode_str[mode_str_idx++] = 'a';
		break;

	case RM_FILE_MODE_READ_UPDATE:

		mode_str[mode_str_idx++] = 'r';
		mode_str[mode_str_idx++] = '+';

		break;

	case RM_FILE_MODE_WRITE_UPDATE:

		mode_str[mode_str_idx++] = 'w';
		mode_str[mode_str_idx++] = '+';

		break;

	case RM_FILE_MODE_APPEND_UPDATE:

		mode_str[mode_str_idx++] = 'a';
		mode_str[mode_str_idx++] = '+';

		break;
	}

	//Text or binary?
	switch (enc)
	{
	case RM_FILE_ENC_TEXT:

		break;

	case RM_FILE_ENC_BINARY:

		mode_str[mode_str_idx++] = 'b';
		break;
	}

	//Append null:
	mode_str[mode_str_idx++] = '\0';

	//Delegate to fopen(...):
	rm_file file = fopen(path, mode_str);
	rm_precond(file, "Failed to open or create file: %s", path);

	return file;
}

rm_void rm_file_close(rm_file file)
{
	//Just delegate to fclose(...):
	fclose(file);
}

rm_void rm_file_seek(rm_file file, rm_file_offset offset, rm_file_seek_origin origin)
{
	//Make sure we have 64 bit offsets:
	RM_FILE_ASSERT_OFF_T_64_BIT();

	//Delegate to fseeko(...):
	rm_int whence;

	switch (origin)
	{
	case RM_FILE_SEEK_ORIGIN_START:

		whence = SEEK_SET;
		break;

	case RM_FILE_SEEK_ORIGIN_CURRENT:

		whence = SEEK_CUR;
		break;

	case RM_FILE_SEEK_ORIGIN_END:

		whence = SEEK_END;
		break;

	default:

		rm_exit("Invalid whence");
	}

	rm_int result = fseeko(file, offset, whence);
	rm_precond(result == 0, "fseeko(...) has failed.");
}

rm_file_offset rm_file_tell(rm_file file)
{
	//Make sure we have 64 bit offsets:
	RM_FILE_ASSERT_OFF_T_64_BIT();

	//Delegate to ftello(...):
	rm_file_offset offset = ftello(file);
	rm_precond(offset >= 0, "ftello(...) has failed.");

	return offset;
}

rm_void rm_file_flush(rm_file file)
{
	//Just delegate to fllush(...) and validate the result:
	rm_int result = fflush(file);
	rm_precond(result == 0, "fflush(...) has failed.");
}

//Emit non-inline versions:
extern rm_size rm_file_read(rm_file file, rm_void* dest_ptr, rm_size size);
extern rm_bool rm_file_read_line(rm_file file, rm_char* buf, rm_size count);
extern rm_void rm_file_vprint(rm_file file, const rm_char* format, va_list args);
extern rm_void rm_file_print(rm_file file, const rm_char* format, ...);
extern rm_void rm_file_write(rm_file file, const rm_void* src_ptr, rm_size size);
extern rm_void rm_file_write_uint8(rm_file file, rm_uint8 value);
extern rm_void rm_file_write_int8(rm_file file, rm_int8 value);
extern rm_void rm_file_write_uint16(rm_file file, rm_uint16 value);
extern rm_void rm_file_write_int16(rm_file file, rm_int16 value);
extern rm_void rm_file_write_uint32(rm_file file, rm_uint32 value);
extern rm_void rm_file_write_int32(rm_file file, rm_int32 value);
extern rm_void rm_file_write_uint64(rm_file file, rm_uint64 value);
extern rm_void rm_file_write_int64(rm_file file, rm_int64 value);
