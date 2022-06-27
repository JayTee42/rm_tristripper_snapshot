#ifndef __RM_MACRO_H__
#define __RM_MACRO_H__

#include "rm_type.h"

//For memcpy (we cannot include rm_mem.h here):
#include <string.h>

//Flip byte orders of integers (macros work for / preserve both signed and unsigned).
//endian.h is a non-standard header.
#ifdef LINUX
#include <endian.h>

#define rm_flip_host_to_be_16(value) ((typeof((value)))htobe16((rm_uint16)(value)))
#define rm_flip_host_to_le_16(value) ((typeof((value)))htole16((rm_uint16)(value)))
#define rm_flip_be_to_host_16(value) ((typeof((value)))be16toh((rm_uint16)(value)))
#define rm_flip_le_to_host_16(value) ((typeof((value)))le16toh((rm_uint16)(value)))

#define rm_flip_host_to_be_32(value) ((typeof((value)))htobe32((rm_uint32)(value)))
#define rm_flip_host_to_le_32(value) ((typeof((value)))htole32((rm_uint32)(value)))
#define rm_flip_be_to_host_32(value) ((typeof((value)))be32toh((rm_uint32)(value)))
#define rm_flip_le_to_host_32(value) ((typeof((value)))le32toh((rm_uint32)(value)))

#define rm_flip_host_to_be_64(value) ((typeof((value)))htobe64((rm_uint64)(value)))
#define rm_flip_host_to_le_64(value) ((typeof((value)))htole64((rm_uint64)(value)))
#define rm_flip_be_to_host_64(value) ((typeof((value)))be64toh((rm_uint64)(value)))
#define rm_flip_le_to_host_64(value) ((typeof((value)))le64toh((rm_uint64)(value)))
#elif defined(MACOS)
#include <libkern/OSByteOrder.h>

#define rm_flip_host_to_be_16(value) ((typeof((value)))OSSwapHostToBigInt16((rm_uint16)(value)))
#define rm_flip_host_to_le_16(value) ((typeof((value)))OSSwapHostToLittleInt16((rm_uint16)(value)))
#define rm_flip_be_to_host_16(value) ((typeof((value)))OSSwapBigToHostInt16((rm_uint16)(value)))
#define rm_flip_le_to_host_16(value) ((typeof((value)))OSSwapLittleToHostInt16((rm_uint16)(value)))

#define rm_flip_host_to_be_32(value) ((typeof((value)))OSSwapHostToBigInt32((rm_uint32)(value)))
#define rm_flip_host_to_le_32(value) ((typeof((value)))OSSwapHostToLittleInt32((rm_uint32)(value)))
#define rm_flip_be_to_host_32(value) ((typeof((value)))OSSwapBigToHostInt32((rm_uint32)(value)))
#define rm_flip_le_to_host_32(value) ((typeof((value)))OSSwapLittleToHostInt32((rm_uint32)(value)))

#define rm_flip_host_to_be_64(value) ((typeof((value)))OSSwapHostToBigInt64((rm_uint64)(value)))
#define rm_flip_host_to_le_64(value) ((typeof((value)))OSSwapHostToLittleInt64((rm_uint64)(value)))
#define rm_flip_be_to_host_64(value) ((typeof((value)))OSSwapBigToHostInt64((rm_uint64)(value)))
#define rm_flip_le_to_host_64(value) ((typeof((value)))OSSwapLittleToHostInt64((rm_uint64)(value)))
#else
#error "Unsupported endian flip functions"
#endif

/*
	There are no helpers for float and double, so we try to manage that by ourselves.
	Is it safe to assume that FP endianness is equal to integer endianness?
	And do all some-kind-of-modern machines use the IEEE 754 standard?
	So many questions on this stuff ...
*/

#define rm_flip_host_to_be_float(value)              	\
({                                                   	\
	rm_float _value = (value);                       	\
	rm_uint32 _uint_value;                           	\
	memcpy(&_uint_value, &_value, sizeof(rm_float)); 	\
	_uint_value = rm_flip_host_to_be_32(_uint_value);	\
	memcpy(&_value, &_uint_value, sizeof(rm_float)); 	\
	_value;                                          	\
})

#define rm_flip_host_to_le_float(value)              	\
({                                                   	\
	rm_float _value = (value);                       	\
	rm_uint32 _uint_value;                           	\
	memcpy(&_uint_value, &_value, sizeof(rm_float)); 	\
	_uint_value = rm_flip_host_to_le_32(_uint_value);	\
	memcpy(&_value, &_uint_value, sizeof(rm_float)); 	\
	_value;                                          	\
})

#define rm_flip_be_to_host_float(value)              	\
({                                                   	\
	rm_float _value = (value);                       	\
	rm_uint32 _uint_value;                           	\
	memcpy(&_uint_value, &_value, sizeof(rm_float)); 	\
	_uint_value = rm_flip_be_to_host_32(_uint_value);	\
	memcpy(&_value, &_uint_value, sizeof(rm_float)); 	\
	_value;                                          	\
})

#define rm_flip_le_to_host_float(value)              	\
({                                                   	\
	rm_float _value = (value);                       	\
	rm_uint32 _uint_value;                           	\
	memcpy(&_uint_value, &_value, sizeof(rm_float)); 	\
	_uint_value = rm_flip_le_to_host_32(_uint_value);	\
	memcpy(&_value, &_uint_value, sizeof(rm_float)); 	\
	_value;                                          	\
})

#define rm_flip_host_to_be_double(value)             	\
({                                                   	\
	rm_double _value = (value);                      	\
	rm_uint64 _uint_value;                           	\
	memcpy(&_uint_value, &_value, sizeof(rm_double));	\
	_uint_value = rm_flip_host_to_be_64(_uint_value);	\
	memcpy(&_value, &_uint_value, sizeof(rm_double));	\
	_value;                                          	\
})

#define rm_flip_host_to_le_double(value)             	\
({                                                   	\
	rm_double _value = (value);                      	\
	rm_uint64 _uint_value;                           	\
	memcpy(&_uint_value, &_value, sizeof(rm_double));	\
	_uint_value = rm_flip_host_to_le_64(_uint_value);	\
	memcpy(&_value, &_uint_value, sizeof(rm_double));	\
	_value;                                          	\
})

#define rm_flip_be_to_host_double(value)             	\
({                                                   	\
	rm_double _value = (value);                      	\
	rm_uint64 _uint_value;                           	\
	memcpy(&_uint_value, &_value, sizeof(rm_double));	\
	_uint_value = rm_flip_be_to_host_64(_uint_value);	\
	memcpy(&_value, &_uint_value, sizeof(rm_double));	\
	_value;                                          	\
})

#define rm_flip_le_to_host_double(value)             	\
({                                                   	\
	rm_double _value = (value);                      	\
	rm_uint64 _uint_value;                           	\
	memcpy(&_uint_value, &_value, sizeof(rm_double));	\
	_uint_value = rm_flip_le_to_host_64(_uint_value);	\
	memcpy(&_value, &_uint_value, sizeof(rm_double));	\
	_value;                                          	\
})

//Min, max and clamping:
//Ripped from the Linux kernel ...
#define rm_min(x, y)              	\
({                                	\
	typeof(x) _min1 = (x);        	\
	typeof(y) _min2 = (y);        	\
	(void)(&_min1 == &_min2);     	\
	_min1 < _min2 ? _min1 : _min2;	\
})

#define rm_max(x, y)              	\
({                                	\
	typeof(x) _max1 = (x);        	\
	typeof(y) _max2 = (y);        	\
	(void)(&_max1 == &_max2);     	\
	_max1 > _max2 ? _max1 : _max2;	\
})

#define rm_clamp(x, min, max) rm_min(rm_max((x), (min)), (max))

//Is a value contained in a given range?
//Closed: [lower, upper]
#define rm_contained_closed(x, lower, upper)	\
({                                          	\
	typeof(x) _x = (x);                     	\
	typeof(lower) _lower = (lower);         	\
	typeof(upper) _upper = (upper);         	\
	(void)(&_x == &_lower);                 	\
	(void)(&_lower == &_upper);             	\
	(_lower <= _x) && (_x <= _upper);       	\
})

//Half-open: [lower, upper)
#define rm_contained_half(x, lower, upper)	\
({                                        	\
	typeof(x) _x = (x);                   	\
	typeof(lower) _lower = (lower);       	\
	typeof(upper) _upper = (upper);       	\
	(void)(&_x == &_lower);               	\
	(void)(&_lower == &_upper);           	\
	(_lower <= _x) && (_x < _upper);      	\
})

//Swap two values of the same type.
//This triggers a compile time error if both parameters don't have the same size.
#define rm_swap(ptr_x, ptr_y)                                                       	\
({                                                                                  	\
	typeof(*ptr_x) _tmp[(sizeof(*ptr_x) == sizeof(*ptr_y)) ? 1 : -1] = { (*ptr_x) };	\
	(*ptr_x) = (*ptr_y);                                                            	\
	(*ptr_y) = *_tmp;                                                               	\
})

//Branch prediction:
#define rm_likely(x) __builtin_expect((x), true)
#define rm_unlikely(x) __builtin_expect((x), false)

//Mark some parameter as unused:
#define rm_unused(x) (void)(x)

//Warn if the return value of a function is unused:
#define rm_must_check __attribute__((warn_unused_result))

//A function does not return:
#define rm_no_return __attribute__((noreturn))

//Force inlining:
#define rm_force_inline __attribute__((always_inline))

//Pack a struct (MSVC has some push / pop stuff, so we use a full macro here):
#define packed_struct(_struct_decl) _struct_decl __attribute__((__packed__))

/*
	How to use this with a typedef:

	typedef packed_struct
	(
		struct __rm_my_struct__
		{
			...
		}
	) rm_my_struct;
*/

//Get the size of an array.
//Only use this for stack arrays!
//This macro tries to catch *some* misuses by introducing a division by zero on a compiler-time constant.
#define rm_array_count(a) ((sizeof(a) / sizeof(a[0])) / ((size_t)(!(sizeof(a) % sizeof(a[0])))))

//Get the number of ones in the given data type:
#define rm_pop_count_u8(value) ((rm_size)__builtin_popcount((unsigned int)value))
#define rm_pop_count_u16(value) ((rm_size)__builtin_popcount((unsigned int)value))
#define rm_pop_count_u32(value) ((rm_size)__builtin_popcount((unsigned int)value))
#define rm_pop_count_u64(value) ((rm_size)__builtin_popcountll((unsigned long long)value))

#define rm_pop_count_s8(value) ((rm_size)__builtin_popcount((unsigned int)value))
#define rm_pop_count_s16(value) ((rm_size)__builtin_popcount((unsigned int)value))
#define rm_pop_count_s32(value) ((rm_size)__builtin_popcount((unsigned int)value))
#define rm_pop_count_s64(value) ((rm_size)__builtin_popcountll((unsigned long long)value))

#endif
