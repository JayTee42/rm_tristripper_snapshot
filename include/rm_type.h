#ifndef __RM_TYPE_H__
#define __RM_TYPE_H__

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

//Fixed-size int types:
typedef uint8_t rm_uint8;
typedef int8_t rm_int8;

typedef uint16_t rm_uint16;
typedef int16_t rm_int16;

typedef uint32_t rm_uint32;
typedef int32_t rm_int32;

typedef uint64_t rm_uint64;
typedef int64_t rm_int64;

typedef unsigned __int128 rm_uint128;
typedef __int128 rm_int128;

//Machine-dependent int types:
typedef unsigned int rm_uint;
typedef int rm_int;

typedef unsigned long rm_uword;
typedef long rm_word;

//Important: We expect sizeof(size_t) to be >= sizeof(uint32_t) because it stores counts and sizes all the time.
//This should hold true for all memory models on at-least-a-little-bit-modern machines.
//Don't expect it to take 64 bit values, though!
typedef size_t rm_size;

//Floating point types:
typedef float rm_float;
typedef double rm_double;

//Booleans:
typedef bool rm_bool;

//Characters:
typedef unsigned char rm_uchar;
typedef char rm_char;

//Void:
typedef void rm_void;

//A NULL pointer.
//Make sure nobody else had that idea before ...
#ifndef null
#define null NULL
#endif

//Boolean values:
#ifndef true
#define true ((rm_bool)1)
#endif

#ifndef false
#define false ((rm_bool)0)
#endif

//If we use structs as state, this flag is used to mark parts of the public interface:
#define public_interface_get
#define public_interface_get_set

#endif
