#include "rm_mem.h"

//Emit non-inline versions:
extern rm_void* rm_malloc(rm_size size);
extern rm_void* rm_malloc_zero(rm_size size);
extern rm_void* rm_realloc(rm_void* ptr, rm_size size);
extern rm_void rm_free(const rm_void* ptr);
extern rm_void* rm_malloc_aligned(rm_size alignment, rm_size size);
extern rm_void* rm_malloc_aligned_zero(rm_size alignment, rm_size size);
extern rm_void* rm_realloc_aligned(rm_void* ptr, rm_size alignment, rm_size old_size, rm_size size);
extern rm_void rm_free_aligned(const rm_void* ptr);
extern rm_void rm_mem_set(rm_void* ptr, rm_uint8 value, rm_size count);
extern rm_void rm_mem_copy(rm_void* dst_ptr, const rm_void* src_ptr, rm_size size);
extern rm_void rm_mem_move(rm_void* dst_ptr, const rm_void* src_ptr, rm_size size);
extern rm_void* rm_mem_dup(const rm_void* ptr, rm_size size);
extern rm_bool rm_mem_compare(const rm_void* ptr0, const rm_void* ptr1, rm_size size);
extern rm_size rm_mem_get_page_bytes(rm_void);
extern rm_size rm_mem_get_physical_bytes(rm_void);
extern rm_void rm_mem_lock(const rm_void* ptr, rm_size size);
extern rm_void rm_mem_unlock(const rm_void* ptr, rm_size size);
