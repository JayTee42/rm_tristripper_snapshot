#ifndef __RM_MEM_H__
#define __RM_MEM_H__

#include <alloca.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>

#include "rm_assert.h"
#include "rm_macro.h"
#include "rm_type.h"

//Memory allocation for structs with flexible array members.
//See ttps://www.geeksforgeeks.org/flexible-array-members-structure-c/ for details.
#define rm_malloc_flex_array_struct(struct_type, array_member_name, count)	\
({                                                                        	\
	rm_size member_size = sizeof(*(((struct_type*)0)->array_member_name));	\
	rm_malloc(sizeof(struct_type) + ((count) * member_size));             	\
})

#define rm_malloc_zero_flex_array_struct(struct_type, array_member_name, count)	\
({                                                                             	\
	rm_size member_size = sizeof(*(((struct_type*)0)->array_member_name));     	\
	rm_malloc_zero(sizeof(struct_type) + ((count) * member_size));             	\
})

//Allocate memory on the heap.
//Size must be >= 1.
//Guaranteed to return a valid pointer.
inline rm_void* rm_must_check rm_malloc(rm_size size) rm_force_inline;
inline rm_void* rm_must_check rm_malloc_zero(rm_size size) rm_force_inline;

//Reallocate memory on the heap that has been allocated via rm_malloc*(...).
//Size must be >= 1.
//If null is passed, the behavior is identical to rm_malloc(size).
inline rm_void* rm_must_check rm_realloc(rm_void* ptr, rm_size size) rm_force_inline;

//Free memory that has been allocated via rm_malloc*(...) or rm_realloc*(...).
//Freeing a null pointer is a nop.
inline rm_void rm_free(const rm_void* ptr) rm_force_inline;

//Allocate aligned memory on the heap that is aligned as stated.
//Size must be >= 1.
//Guaranteed to return a valid pointer.
inline rm_void* rm_must_check rm_malloc_aligned(rm_size alignment, rm_size size) rm_force_inline;
inline rm_void* rm_must_check rm_malloc_aligned_zero(rm_size alignment, rm_size size) rm_force_inline;

//Reallocate memory on the heap that has been allocated via rm_malloc_aligned*(...).
//Size must be >= 1.
//We need the size of the old block and the alignment here because not every platform can realloc with alignment.
//If null is passed, the behavior is identical to "rm_malloc_aligned(alignment, size)".
inline rm_void* rm_must_check rm_realloc_aligned(rm_void* ptr, rm_size alignment, rm_size old_size, rm_size size) rm_force_inline;

//Free memory that has been allocated via "rm_malloc_aligned*(...)"" or "rm_realloc_aligned*(...)".
//Freeing a null pointer is a nop.
inline rm_void rm_free_aligned(const rm_void* ptr) rm_force_inline;

//A memset wrapper.
//"count" bytes a "ptr" are set to "value".
inline rm_void rm_mem_set(rm_void* ptr, rm_uint8 value, rm_size count) rm_force_inline;

//A memcpy wrapper.
//Size must be >= 1.
inline rm_void rm_mem_copy(rm_void* dst_ptr, const rm_void* src_ptr, rm_size size) rm_force_inline;

//A memmove wrapper.
//Size must be >= 1.
inline rm_void rm_mem_move(rm_void* dst_ptr, const rm_void* src_ptr, rm_size size) rm_force_inline;

//Duplicate the given memory.
//Size must be >= 1.
//The resulting pointer must be freed using rm_free(...).
inline rm_void* rm_must_check rm_mem_dup(const rm_void* ptr, rm_size size) rm_force_inline;

//Compare the given memory byte-by-byte.
//If size is > 0, both pointers must be non-null.
inline rm_bool rm_mem_compare(const rm_void* ptr0, const rm_void* ptr1, rm_size size) rm_force_inline;

//Query the page size of the system in bytes.
inline rm_size rm_mem_get_page_bytes(rm_void) rm_force_inline;

//Query the amount of physical memory of the system in bytes.
inline rm_size rm_mem_get_physical_bytes(rm_void) rm_force_inline;

//Lock the specified amount of memory into physical memory to avoid paging.
//The address must be page-aligned.
inline rm_void rm_mem_lock(const rm_void* ptr, rm_size size) rm_force_inline;

//Unlock memory that has been locked via rm_mem_lock(...).
inline rm_void rm_mem_unlock(const rm_void* ptr, rm_size size) rm_force_inline;

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

inline rm_void* rm_malloc_aligned(rm_size alignment, rm_size size)
{
	//Size check:
	rm_assert(size >= 1, "Memory size to allocate must be >= 1.");

	//Allocate memory via posix_memalign(...):
	rm_void* ptr = null;
	int result = posix_memalign(&ptr, alignment, size);

	//Always check for errors:
	rm_precond(result == 0, "posix_memalign() has failed: %s", strerror(result));

	//Did the alignment work out?
	rm_assert((((rm_uword)ptr) % alignment) == 0, "Failed to align allocated memory.");

	return ptr;
}

inline rm_void* rm_malloc_aligned_zero(rm_size alignment, rm_size size)
{
	//Size check:
	rm_assert(size >= 1, "Memory size to allocate must be >= 1.");

	//There is no posix_memalign(...) equivalent that zeroes the memory.
	//So we do it manually.
	rm_void* ptr = rm_malloc_aligned(alignment, size);
	memset(ptr, 0, size);

	return ptr;
}

inline rm_void* rm_realloc_aligned(rm_void* ptr, rm_size alignment, rm_size old_size, rm_size size)
{
	//Size check:
	rm_assert(size >= 1, "Memory size to allocate must be >= 1.");

	//This is pretty ugly!
	//Using realloc on posix_memalign(...) ptrs has two problems:
	//
	// 1.) Theoretically, it's undefined behavior:
	//     "[...] Pointer to a memory block previously allocated with malloc, calloc or realloc."
	// 2.) _If_ realloc needs to reallocate the memory, the resulting block does not have to be aligned in any way.
	//
	//So let's stay safe here and perform a complete new allocation:
	rm_void* new_ptr = rm_malloc_aligned(alignment, size);

	//Now copy the old memory over to the new location:
	rm_mem_copy(new_ptr, ptr, rm_min(old_size, size));

	//Free the old memory:
	rm_free_aligned(ptr);

	return new_ptr;
}

inline rm_void rm_free_aligned(const rm_void* ptr)
{
	//The counterpart for posix_memalign(...) is free(...), too.
	rm_free(ptr);
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

inline rm_void rm_mem_move(rm_void* dst_ptr, const rm_void* src_ptr, rm_size size)
{
	//Check:
	rm_assert(dst_ptr && src_ptr, "Source or destination pointer is null.");
	rm_assert(size >= 1, "Memory size to copy must be >= 1.");

	memmove(dst_ptr, src_ptr, size);
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

inline rm_bool rm_mem_compare(const rm_void* ptr0, const rm_void* ptr1, rm_size size)
{
	//Null checks:
	rm_assert((size == 0) || (ptr0 && ptr1), "Memory blobs to compare must be non-null if size is > 0.");

	//Delegate to memcmp(...):
	return (size == 0) || (memcmp(ptr0, ptr1, size) == 0);
}

inline rm_size rm_mem_get_page_bytes(rm_void)
{
	return (rm_size)sysconf(_SC_PAGE_SIZE);
}

inline rm_size rm_mem_get_physical_bytes(rm_void)
{
	//Query the number of physical pages:
	rm_size page_count = (rm_size)sysconf(_SC_PHYS_PAGES);

	//Multiply with the page size:
	return page_count * rm_mem_get_page_bytes();
}

inline rm_void rm_mem_lock(const rm_void* ptr, rm_size size)
{
#ifdef DEBUG_BUILD
	rm_size page_size = rm_mem_get_page_bytes();
	rm_precond(((rm_uword)ptr % page_size) == 0, "The address to lock must be aligned to a page boundary.");
#endif
	//Delegate to mlock(...):
	rm_int result = mlock(ptr, size);
	rm_precond(result == 0, "Failed to lock pages: %s", strerror(errno));
}

inline rm_void rm_mem_unlock(const rm_void* ptr, rm_size size)
{
	//Delegate to munlock(...):
	rm_int result = munlock(ptr, size);
	rm_precond(result == 0, "Failed to unlock pages: %s", strerror(errno));
}

#endif
