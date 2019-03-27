#ifndef __RM_MACRO_H__
#define __RM_MACRO_H__

#include "rm_type.h"

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

//Branch prediction:
#define rm_likely(x) __builtin_expect((x), true)
#define rm_unlikely(x) __builtin_expect((x), false)

//Explicit fallthrough:
#define rm_fallthrough __attribute__ ((fallthrough))

//Mark some parameter as unused:
#define rm_unused(x) (void)(x)

//Warn if the return value of a function is unused:
#define rm_must_check __attribute__((warn_unused_result))

//A function does not return:
#define rm_no_return __attribute__((noreturn))

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
//This macro tries to catch _some_ misuses by introducing a division by zero on a compiler-time constant.
#define rm_array_count(a) ((sizeof(a) / sizeof(a[0])) / ((size_t)(!(sizeof(a) % sizeof(a[0])))))

#endif
