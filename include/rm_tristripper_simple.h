#ifndef __RM_TRISTRIPPER_SIMPLE_H__
#define __RM_TRISTRIPPER_SIMPLE_H__

#include "rm_tristripper_tri.h"

//Apply the "stripify" algorithm to the triangles and return strips:
rm_void rm_tristripper_create_strips_simple(rm_tristripper_tri* tris, rm_size tris_count, rm_bool preserve_orientation, rm_tristripper_strip** strips, rm_size* strips_count);

#endif
