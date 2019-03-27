#ifndef __RM_MEM_H__
#define __RM_MEM_H__

#include <stdlib.h>
#include <string.h>

#include "rm_assert.h"
#include "rm_macro.h"
#include "rm_type.h"

//Allocate memory on the heap.
//Size must be >= 1.
//Guaranteed to return a valid pointer.
inline rm_void* rm_must_check rm_malloc(rm_size size);
inline rm_void* rm_must_check rm_malloc_zero(rm_size size);

//Reallocate memory on the heap that has been allocated via rm_malloc*(...).
//Size must be >= 1.
//If null is passed, the behavior is identical to rm_malloc(size).
inline rm_void* rm_must_check rm_realloc(rm_void* ptr, rm_size size);

//Free memory that has been allocated via rm_malloc*(...) or rm_realloc*(...).
//Freeing a null pointer is a nop.
inline rm_void rm_free(const rm_void* ptr);

//A memset wrapper.
//"count" bytes a "ptr" are set to "value".
inline rm_void rm_mem_set(rm_void* ptr, rm_uint8 value, rm_size count);

//A memcpy wrapper.
//Size must be >= 1.
inline rm_void rm_mem_copy(rm_void* dst_ptr, const rm_void* src_ptr, rm_size size);

//Duplicate the given memory.
//Size must be >= 1.
//The resulting pointer must be freed using rm_free(...).
inline rm_void* rm_must_check rm_mem_dup(const rm_void* ptr, rm_size size);

inline rm_void* rm_malloc(rm_size size)
{
	//Size check:
	rm_assert(size >= 1, "Memory size to allocate must be >= 1.");

	//Allocate memory:
	rm_void* ptr = malloc(size);

	//Always check for errors:
	rm_precond(ptr, "malloc() has failed.");

	return ptr;
}

inline rm_void* rm_malloc_zero(rm_size size)
{
	//Size check:
	rm_assert(size >= 1, "Memory size to allocate must be >= 1.");

	//Allocate memory via calloc(...) -> will be zeroed:
	rm_void* ptr = calloc(size, sizeof(rm_uint8));

	//Always check for errors:
	rm_precond(ptr, "calloc() has failed.");

	return ptr;
}

inline rm_void* rm_realloc(rm_void* ptr, rm_size size)
{
	//Size check:
	rm_assert(size >= 1, "Memory size to allocate must be >= 1.");

	//Use realloc(...) directly for this:
	ptr = realloc(ptr, size);

	//Always check for errors:
	rm_precond(ptr, "realloc() has failed.");

	return ptr;
}

inline rm_void rm_free(const rm_void* ptr)
{
	/*
		There is a common case where we need to free const pointers.
		Consider a variable or struct member that receives some allocated memory (i. e. the result of strdup(...)), but is never going to change it.
		So it might be a good idea to annotate the pointer as "const".
		But later on we have to free that memory - and got a problem because free(...) only takes non-const pointers.
		Linus talks about that problem, too. That's obviously the reason for kfree(...) to take a const pointer.

		Check it out: https://yarchive.net/comp/const.html

		The downside: We are casting const away. Theoretically, that's a bad idea.
	*/

	free((rm_void*)ptr);
}

inline rm_void rm_mem_set(rm_void* ptr, rm_uint8 value, rm_size count)
{
	//Check:
	rm_assert(ptr, "Destination pointer is null.");

	memset(ptr, value, count);
}

inline rm_void rm_mem_copy(rm_void* dst_ptr, const rm_void* src_ptr, rm_size size)
{
	//Check:
	rm_assert(dst_ptr && src_ptr, "Source or destination pointer is null.");
	rm_assert(size >= 1, "Memory size to copy must be >= 1.");

	memcpy(dst_ptr, src_ptr, size);
}

inline rm_void* rm_mem_dup(const rm_void* ptr, rm_size size)
{
	//Size check:
	rm_assert(size >= 1, "Memory size to duplicate must be >= 1.");

	//Allocate new memory:
	rm_void* new_ptr = rm_malloc(size);

	//Copy the memory over to the new buffer:
	rm_mem_copy(new_ptr, ptr, size);

	//Return the new pointer:
	return new_ptr;
}

#endif
