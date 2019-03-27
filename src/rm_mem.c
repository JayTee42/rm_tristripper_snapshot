#include "rm_mem.h"

//Emit non-inline versions:
extern rm_void* rm_malloc(rm_size size);
extern rm_void* rm_malloc_zero(rm_size size);
extern rm_void* rm_realloc(rm_void* ptr, rm_size size);
extern rm_void rm_free(const rm_void* ptr);
extern rm_void rm_mem_set(rm_void* ptr, rm_uint8 value, rm_size count);
extern rm_void rm_mem_copy(rm_void* dst_ptr, const rm_void* src_ptr, rm_size size);
extern rm_void* rm_mem_dup(const rm_void* ptr, rm_size size);
