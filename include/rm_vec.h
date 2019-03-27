#ifndef __RM_VEC_H__
#define __RM_VEC_H__

//This vector implementation is loosely based on https://github.com/rxi/vec.
//Don't call any of those __xyz__ functions / macros manually!

#include <stdlib.h>
#include <string.h>

#include "rm_mem.h"
#include "rm_type.h"

//If an element is pushed to an empty vector, this start capacity is assigned:
#define RM_VEC_START_CAPACITY ((rm_size)8)

//A vector of type "t".
//"count" is the number of currently contained elements.
//"capacity" is the number of elements that could be contained without a realloc (which is always >= "count").
#define rm_vec(t) struct					\
{											\
	t* data;								\
	public_interface_get rm_size count;		\
	public_interface_get rm_size capacity;	\
}

//Initialize a vector as empty:
#define rm_vec_init(v) 	\
({                     	\
	typeof(v) _v = (v);	\
	_v->data = null;   	\
	_v->count = 0;     	\
	_v->capacity = 0;  	\
})

//Dispose a vector (calling free on a "null" is a nop):
#define rm_vec_dispose(v)	\
({                       	\
	typeof(v) _v = (v);  	\
	rm_free(_v->data);   	\
	_v->count = 0;       	\
	_v->capacity = 0;    	\
})

//Dispose a vector, but return its data as heap-allocated, trimmed array.
//The caller must free the result using rm_free(...).
//For an empty vector, null is returned.
#define rm_vec_unwrap(v)                                                             	\
({                                                                                   	\
	typeof(v) _v = (v);                                                              	\
	__rm_vec_trim_capacity__(&_v->data, _v->count, &_v->capacity, sizeof(*_v->data));	\
	_v->count = 0;                                                                   	\
	_v->capacity = 0;                                                                	\
	_v->data;                                                                        	\
})

//Duplicate the contents of the vector.
//The caller must free the result using rm_free(...).
//For an empty vector, null is returned.
#define rm_vec_dup(v)                                                                        	\
({                                                                                           	\
	typeof(v) _v = (v);                                                                      	\
	(_v->count > 0) ? (rm_mem_dup((rm_void*)_v->data, _v->count * sizeof(*_v->data))) : null;	\
})

//If the capacity is below "min_capacity", it is raised to "min_capacity":
#define rm_vec_ensure_capacity(v, min_capacity)                                                      	\
({                                                                                                   	\
	typeof(v) _v = (v);                                                                              	\
	__rm_vec_ensure_capacity__(&_v->data, &_v->capacity, (rm_size)(min_capacity), sizeof(*_v->data));	\
})

//Trim the capacity to the count. Release unused memory.
//After this operation, a push to the vector will trigger a reallocation.
#define rm_vec_trim_capacity(v)                                                      	\
({                                                                                   	\
	typeof(v) _v = (v);                                                              	\
	__rm_vec_trim_capacity__(&_v->data, _v->count, &_v->capacity, sizeof(*_v->data));	\
})

//Get the first element of the vector.
//It must be present.
#define rm_vec_first(v)                                                            	\
({                                                                                 	\
	typeof(v) _v = (v);                                                            	\
	rm_assert(_v->count > 0, "Cannot get the first element from an empty vector.");	\
	_v->data[0];                                                                   	\
})

//Get the last element of the vector.
//It must be present.
#define rm_vec_last(v)                                                            	\
({                                                                                	\
	typeof(v) _v = (v);                                                           	\
	rm_assert(_v->count > 0, "Cannot get the last element from an empty vector.");	\
	_v->data[_v->count - 1];                                                      	\
})

//Get the nth element of the vector.
//It must be present.
#define rm_vec_at(v, n)                                                                                 	\
({                                                                                                      	\
	typeof(v) _v = (v);                                                                                 	\
	rm_size _n = (rm_size)(n);                                                                          	\
	rm_assert(_v->count > _n, "Cannot get element %zu from a vector with %zu elements.", _n, _v->count);	\
	_v->data[_n];                                                                                       	\
})

//Get a pointer to the nth element of the vector.
//It must be present.
//_Use with caution_! This pointer might be invalid after the next ensure_capacity / trim_capacity / push / push_empty.
#define rm_vec_ptr_at(v, n)                                                                                    	\
({                                                                                                             	\
	typeof(v) _v = (v);                                                                                        	\
	rm_size _n = (rm_size)(n);                                                                                 	\
	rm_assert(_v->count > _n, "Cannot get ptr to element %zu from a vector with %zu elements.", _n, _v->count);	\
	&_v->data[_n];                                                                                             	\
})

//Push an element to the end of the vector.
//This is accumulated O(1).
#define rm_vec_push(v, new_element)                                                     	\
({                                                                                      	\
	typeof(v) _v = (v);                                                                 	\
	__rm_vec_increment_count__(&_v->data, &_v->count, &_v->capacity, sizeof(*_v->data));	\
	_v->data[_v->count - 1] = (new_element);                                            	\
})

//Push an empty element to the end of the vector and return a pointer to it.
//This is accumulated O(1).
#define rm_vec_push_empty(v)                                                            	\
({                                                                                      	\
	typeof(v) _v = (v);                                                                 	\
	__rm_vec_increment_count__(&_v->data, &_v->count, &_v->capacity, sizeof(*_v->data));	\
	&_v->data[_v->count - 1];                                                           	\
})

//Pop an element from the end of the vector.
//This is always O(1).
#define rm_vec_pop(v)                                                       	\
({                                                                          	\
	typeof(v) _v = (v);                                                     	\
	rm_assert(_v->count > 0, "Cannot pop an element from an empty vector.");	\
	_v->data[--(_v->count)];                                                	\
})

//Set the nth element of the vector.
//It must be present.
#define rm_vec_set(v, n, element)                                                                 	\
({                                                                                                	\
	typeof(v) _v = (v);                                                                           	\
	rm_assert(_v->count > n, "Cannot set element %d in a vector with %d elements.", n, _v->count);	\
	_v->data[n] = element;                                                                        	\
})

//Clear a vector by setting it's count to 0.
#define rm_vec_clear(v) (v)->count = 0;

//If the capacity is below "min_capacity", it is raised to "min_capacity".
//The data pointer is passed as void pointer for the sake of strict aliasing rules.
inline rm_void __rm_vec_ensure_capacity__(rm_void* data, rm_size* capacity, rm_size min_capacity, rm_size element_size);

//Trim the capacity to the count. Release unused memory.
//After this operation, a push to the vector will trigger a reallocation.
//The data pointer is passed as void pointer for the sake of strict aliasing rules.
inline rm_void __rm_vec_trim_capacity__(rm_void* data, rm_size count, rm_size* capacity, rm_size element_size);

//Increment the count of the vector.
//If there is a capacity overflow, the capacity is doubled and a realloc is performed.
//The data pointer is passed as void pointer for the sake of strict aliasing rules.
inline rm_void __rm_vec_increment_count__(rm_void* data, rm_size* count, rm_size* capacity, rm_size element_size);

inline rm_void __rm_vec_ensure_capacity__(rm_void* data, rm_size* capacity, rm_size min_capacity, rm_size element_size)
{
	//If the existing capacity is large enough to hold the new capacity, we are done:
	if (min_capacity <= *capacity)
	{
		return;
	}

	//Assign the new capacity:
	*capacity = min_capacity;

	//Get the actual data pointer:
	rm_void** actual_data = (rm_void**)data;

	//Reallocate the pointer.
	//If it was null, this works like malloc.
	*actual_data = rm_realloc(*actual_data, *capacity * element_size);
}

inline rm_void __rm_vec_trim_capacity__(rm_void* data, rm_size count, rm_size* capacity, rm_size element_size)
{
	//If the capacity is equal to the count, we are done:
	if (count == *capacity)
	{
		return;
	}

	//Assign the count as new capacity:
	*capacity = count;

	//Get the actual data pointer:
	rm_void** actual_data = (rm_void**)data;

	if (*capacity == 0)
	{
		//If the new capacity is 0, we have to free the memory.
		//No risk of double-free, we never get here twice without allocating new memory.
		rm_free(*actual_data);
		*actual_data = null;
	}
	else
	{
		//Perform a reallocation:
		*actual_data = rm_realloc(*actual_data, *capacity * element_size);
	}
}

inline rm_void __rm_vec_increment_count__(rm_void* data, rm_size* count, rm_size* capacity, rm_size element_size)
{
	//Increment the count.
	//If we are still below the capacity, everything's fine:
	if (++(*count) <= *capacity)
	{
		return;
	}

	//Double the capacity (or set the start value):
	if (*capacity == 0)
	{
		*capacity = RM_VEC_START_CAPACITY;
	}
	else
	{
		*capacity <<= 1;
	}

	//Get the actual data pointer:
	rm_void** actual_data = (rm_void**)data;

	//Reallocate the pointer.
	//If it was null, this works like malloc.
	*actual_data = rm_realloc(*actual_data, *capacity * element_size);
}

//Some typical vector declarations:
typedef rm_vec(rm_uint8) rm_uint8_vec;
typedef rm_vec(rm_int8) rm_int8_vec;
typedef rm_vec(rm_uint16) rm_uint16_vec;
typedef rm_vec(rm_int16) rm_int16_vec;
typedef rm_vec(rm_uint32) rm_uint32_vec;
typedef rm_vec(rm_int32) rm_int32_vec;
typedef rm_vec(rm_uint64) rm_uint64_vec;
typedef rm_vec(rm_int64) rm_int64_vec;
typedef rm_vec(rm_uint) rm_uint_vec;
typedef rm_vec(rm_int) rm_int_vec;
typedef rm_vec(rm_uword) rm_uword_vec;
typedef rm_vec(rm_word) rm_word_vec;
typedef rm_vec(rm_size) rm_size_vec;
typedef rm_vec(rm_float) rm_float_vec;
typedef rm_vec(rm_double) rm_double_vec;
typedef rm_vec(rm_bool) rm_bool_vec;
typedef rm_vec(rm_char) rm_char_vec;

typedef rm_vec(rm_uint8*) rm_uint8_ptr_vec;
typedef rm_vec(rm_int8*) rm_int8_ptr_vec;
typedef rm_vec(rm_uint16*) rm_uint16_ptr_vec;
typedef rm_vec(rm_int16*) rm_int16_ptr_vec;
typedef rm_vec(rm_uint32*) rm_uint32_ptr_vec;
typedef rm_vec(rm_int32*) rm_int32_ptr_vec;
typedef rm_vec(rm_uint64*) rm_uint64_ptr_vec;
typedef rm_vec(rm_int64*) rm_int64_ptr_vec;
typedef rm_vec(rm_uint*) rm_uint_ptr_vec;
typedef rm_vec(rm_int*) rm_int_ptr_vec;
typedef rm_vec(rm_uword*) rm_uword_ptr_vec;
typedef rm_vec(rm_word*) rm_word_ptr_vec;
typedef rm_vec(rm_size*) rm_size_ptr_vec;
typedef rm_vec(rm_float*) rm_float_ptr_vec;
typedef rm_vec(rm_double*) rm_double_ptr_vec;
typedef rm_vec(rm_bool*) rm_bool_ptr_vec;
typedef rm_vec(rm_char*) rm_char_ptr_vec;
typedef rm_vec(rm_void*) rm_void_ptr_vec;

#endif
