#include "rm_tristripper_stats.h"

rm_void rm_tristripper_calculate_stats(rm_tristripper_strip* strips, rm_size strips_count, rm_tristripper_stats* stats)
{
	//The number of strips has already been passed:
	stats->strips_count = strips_count;

	//Iterate over the strips and count valid / degenerated tris:
	stats->valid_tris_count = 0;
	stats->swaps_count = 0;

	for (rm_size i = 0; i < strips_count; i++)
	{
		//Get the current strip:
		rm_tristripper_strip* curr_strip = &strips[i];

		//Iterate over its triangles:
		for (rm_size j = 0; j < curr_strip->ids_count - 2; j++)
		{
			//Get the IDs of the current triangle:
			rm_tristripper_id ids[3] =
			{
				curr_strip->ids[j],
				curr_strip->ids[j + 1],
				curr_strip->ids[j + 2]
			};

			//Is it degenerated or not?
			if ((ids[0] == ids[1]) || (ids[1] == ids[2]) || (ids[2] == ids[0]))
			{
				stats->swaps_count++;
			}
			else
			{
				stats->valid_tris_count++;
			}
		}
	}

	//Calculate the different cost models:
	rm_size strips_vertex_count = (strips_count * 2) + stats->valid_tris_count;
	rm_size primitive_restarts_count = (strips_count == 0) ? 0 : (strips_count - 1);

	for (rm_size cost_per_swap = 0; cost_per_swap <= 1; cost_per_swap++)
	{
		for (rm_size cost_per_primitive_restart = 0; cost_per_primitive_restart <= 2; cost_per_primitive_restart++)
		{
			stats->vertex_cost_models[cost_per_swap][cost_per_primitive_restart] = strips_vertex_count + (stats->swaps_count * cost_per_swap) + (primitive_restarts_count * cost_per_primitive_restart);
		}
	}
}
