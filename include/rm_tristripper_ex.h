#ifndef __RM_TRISTRIPPER_EX_H__
#define __RM_TRISTRIPPER_EX_H__

#include "rm_tristripper_tri.h"

//Create tristrips with a preprocessing algorithm and reduce their number with tunneling.
//Details about the configuration can be found in "rm_tristripper_common.h".
rm_void rm_tristripper_create_strips_ex(rm_tristripper_tri* tris, rm_size tris_count, rm_tristripper_config* config, rm_tristripper_strip** strips, rm_size* strips_count);

#endif
