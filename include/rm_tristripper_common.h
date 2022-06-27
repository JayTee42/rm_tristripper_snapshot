#ifndef __RM_TRISTRIPPER_COMMON_H__
#define __RM_TRISTRIPPER_COMMON_H__

#include "rm_type.h"
#include "rm_vec.h"

//Use this constant to denote an absent neighbour index:
#define RM_TRISTRIPPER_NEIGHBOUR_INDEX_NOT_FOUND ((rm_size)3)

//Get the two remaining indices of a triangle (i = 0 and i = 1):
#define rm_tristripper_tri_remaining_index(except_index, i) (((except_index) + 1 + (i)) % 3)

//The ID type for vertices:
typedef rm_uint32 rm_tristripper_id;

//A single tristrip:
typedef struct __rm_tristripper_strip__
{
	//How many IDs do we have?
	rm_size ids_count;

	//A pointer to the IDs:
	rm_tristripper_id* ids;
} rm_tristripper_strip;

//The tunneling process in directed by the following parameters:
//
// - "use_tunneling":              Shall we use tunneling or fall back to stripify-only?
// - "preserve_orientation":       Shall the orientation of strips be preserved? Might introduce some more degenerated triangles ...
//                                 This parameter is available for stripify and for tunneling.
//
// Everything below here is only relevant for tunneling!
//
// - "preproc_algorithm":          Which algorithm shall be used to create the initial strips that are tunneled?
// - "max_count":                  What is the maximum number of triangles that form a tunnel?
//                                 This must be >= 2 and <= UINT16_MAX and should be even.
// - "incremental":                If this is set to "false", we perform DFS with a depth of "max_count".
//                                 Otherwise, we perform incremental DFS, starting at a depth of 2, and increase it to "max_count".
// - "loop_limit":                 Do we limit the tunnel search to a maximum number of loop iterations per tunnel?
//                                 If this is set to "RM_TRISTRIPPER_NO_LOOP_LIMIT",
//                                 we loop until all DFS paths of depth "max_count" have been evaluated.
//                                 Otherwise, the behavior depends on "backtrack_after_loop_limit" (see below).
// - "backtrack_after_loop_limit": Only valid if "loop_limit" is != "RM_TRISTRIPPER_NO_LOOP_LIMIT".
//                                 If this is set to "false" and the limit is reached, we fail instantly.
//                                 Otherwise, we backtrack at the first tunnel member to change the direction.
//                                 On success, the loop count is reset and we search again.
// - "dest_count":                 Stop tunneling as soon as the specified number of strips has been reached.
//                                 Use RM_TRISTRIPPER_NO_DEST_COUNT to keep tunneling until all paths have been discovered.

typedef enum __rm_tristripper_preproc_algorithm__
{
	//Treat each triangle as an isolated strip:
	RM_TRISTRIPPER_PREPROC_ALGORITHM_ISOLATED,

	//Try to pair up to two triangles together.
	//Prefer isolated triangles.
	RM_TRISTRIPPER_PREPROC_ALGORITHM_PAIRS,

	//Use the stripify algorithm (identical to the "non-tunneled" version).
	RM_TRISTRIPPER_PREPROC_ALGORITHM_STRIPIFY
} rm_tristripper_preproc_algorithm;

#define RM_TRISTRIPPER_NO_LOOP_LIMIT ((rm_size)0)
#define RM_TRISTRIPPER_NO_DEST_COUNT ((rm_size)0)

typedef struct __rm_tristripper_config__
{
	rm_bool use_tunneling;
	rm_bool preserve_orientation;
	rm_tristripper_preproc_algorithm preproc_algorithm;
	rm_size max_count;
	rm_bool incremental;
	rm_size loop_limit;
	rm_bool backtrack_after_loop_limit;
	rm_size dest_count;
} rm_tristripper_config;

//Vectors for indices and strips:
typedef rm_vec(rm_tristripper_id) rm_tristripper_id_vec;
typedef rm_vec(rm_tristripper_strip) rm_tristripper_strip_vec;

#endif
