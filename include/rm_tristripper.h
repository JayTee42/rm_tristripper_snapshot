#ifndef __RM_TRISTRIPPER_H__
#define __RM_TRISTRIPPER_H__

#include "rm_tristripper_common.h"
#include "rm_tristripper_stats.h"
#include "rm_tristripper_verifier.h"

//Execute the stripification operation.
//The resulting tristrips must be freed using "rm_tristripper_dispose_strips(...)".
//Note: The config is passed non-const because we might rectify / optimize some parts of it.
rm_void rm_tristripper_create_strips(const rm_tristripper_id* ids, rm_size ids_count, rm_tristripper_config* config, rm_tristripper_strip** strips, rm_size* strips_count);

//Dispose a given array of tristrip pointers that has been created by "rm_tristripper_create_strips(...)".
//"rm_tristripper_dispose_strips(null, 0)" is a no-op.
rm_void rm_tristripper_dispose_strips(const rm_tristripper_strip* strips, rm_size strips_count);

#endif
