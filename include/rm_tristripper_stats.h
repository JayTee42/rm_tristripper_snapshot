#ifndef __RM_TRISTRIPPER_STATS_H__
#define __RM_TRISTRIPPER_STATS_H__

#include "rm_type.h"

#include "rm_tristripper_common.h"

//Interesting statistics about a collection of triangle strips:
typedef struct __rm_tristripper_stats__
{
	//The number of strips:
	rm_size strips_count;

	//The number of non-degenerated triangles that is described by the strips:
	rm_size valid_tris_count;

	//The number of swaps (= swap-induced degenerated triangles):
	rm_size swaps_count;

	/*
		The different cost models for swaps and PRs:

		+-----+-----+-----+-----+
		|     | PR0 | PR1 | PR2 |
		+=====+=====+=====+=====+
		| SW0 |     |     |     |
		| SW1 |     |     |     |
		+-----+-----+-----+-----+

		- SW0: Swaps are free
		- SW1: Swaps cost one vertex
		- PR0: Primitive restarts are free
		- PR1: Primitive restarts cost 1 vertex
		- PR2: Primitive restarts cost 2 vertices
	*/
	rm_size vertex_cost_models[2][3];
} rm_tristripper_stats;

//Calculate the statistics for a given strip collection:
rm_void rm_tristripper_calculate_stats(rm_tristripper_strip* strips, rm_size strips_count, rm_tristripper_stats* stats);

#endif
