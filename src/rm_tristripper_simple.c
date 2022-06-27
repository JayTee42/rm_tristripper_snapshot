#include "rm_tristripper_simple.h"

#include "rm_mem.h"

//Build a single tristrip originating from the given first core triangle.
//Try to advance it in two directions.
//Let's insist on inlining here, it seems to help a little bit for large models.
static inline rm_void rm_tristripper_build_strip(rm_tristripper_tri* first_core_tri, rm_bool preserve_orientation, rm_tristripper_tri** tris_adjacency_lists, rm_tristripper_id_vec* ids_vec, rm_tristripper_strip* strip);
static inline rm_void rm_tristripper_build_strip_loop(rm_tristripper_tri* prev_tri, rm_tristripper_tri* tri, rm_size index_to_prev, rm_tristripper_id prev_entrance_vertex_id, rm_tristripper_id entrance_vertex_id, rm_bool preserve_orientation, rm_bool is_oriented_correctly, rm_tristripper_tri** tris_adjacency_lists, rm_tristripper_id_vec* ids_vec);

static inline rm_void rm_tristripper_build_strip(rm_tristripper_tri* first_core_tri, rm_bool preserve_orientation, rm_tristripper_tri** tris_adjacency_lists, rm_tristripper_id_vec* ids_vec, rm_tristripper_strip* strip)
{
	//Mark the start triangle as stripped:
	rm_tristripper_tri_set_stripped_and_propagate(first_core_tri, tris_adjacency_lists);

	//Get any neighbour of the first core triangle that has not been stripped yet.
	//We also need the shared edge between the two and the index "first -> second".
	rm_tristripper_id first_shared_edge[2];
	rm_size index_first_to_second;

	rm_tristripper_tri* second_core_tri = rm_tristripper_select_next_core_tri(first_core_tri, tris_adjacency_lists, first_shared_edge, &index_first_to_second);

	//If there is no such neighbour, we are done.
	if (!second_core_tri)
	{
		//Allocate a tristrip for a single triangle.
		//Much stupid very sad :(
		*strip = (rm_tristripper_strip)
		{
			.ids_count = 3,
			.ids = rm_mem_dup(first_core_tri->vertices, 3 * sizeof(rm_tristripper_id))
		};

		return;
	}

	//Look at the two neighbours of the second core triangle.
	//Select one of them that has not been stripped yet as the third one, along with the edge shared with the second and the index.
	rm_tristripper_id second_shared_edge[2];
	rm_size index_second_to_third;

	rm_tristripper_tri* third_core_tri = rm_tristripper_select_next_core_tri(second_core_tri, tris_adjacency_lists, second_shared_edge, &index_second_to_third);

	//If there is no such neighbour, we are also done.
	if (!third_core_tri)
	{
		//Allocate a tristrip for two triangles.
		//Still much stupid very sad!
		*strip = (rm_tristripper_strip)
		{
			.ids_count = 4,
			.ids = rm_malloc(4 * sizeof(rm_tristripper_id))
		};

		//Append the vertices of the first core triangle in correct orientation:
		strip->ids[0] = first_core_tri->vertices[(index_first_to_second + 2) % 3];
		strip->ids[1] = first_core_tri->vertices[index_first_to_second];
		strip->ids[2] = first_core_tri->vertices[(index_first_to_second + 1) % 3];

		//Append the last vertex (from the second core triangle):
		rm_size index_second_to_first = (rm_size)first_core_tri->indices_at_neighbours[index_first_to_second];
		strip->ids[3] = second_core_tri->vertices[(index_second_to_first + 2) % 3];

		return;
	}

	//Determine the entrance vertices of the first, second and third core triangle, as viewed from the second one.
	rm_tristripper_id core_entrance_vertex_ids[3];
	rm_tristripper_determine_core_entrance_vertex_ids(first_shared_edge, second_shared_edge, core_entrance_vertex_ids);

	//Determine if the first core triangle would be oriented correctly if we would start the whole strip with it.
	//That is the case if the first call of "rm_tristripper_build_strip_loop(...)" produces only one ID.
	rm_bool is_oriented_correctly = (first_core_tri->vertices[index_first_to_second] == core_entrance_vertex_ids[0]);

	//Grow the strip backward from the second over the first triangle and following.
	//Reuse the same vector for this every time -> we save mallocs :)
	rm_size index_third_to_second = (rm_size)second_core_tri->indices_at_neighbours[index_second_to_third];
	rm_tristripper_build_strip_loop(second_core_tri, first_core_tri, index_first_to_second, core_entrance_vertex_ids[1], core_entrance_vertex_ids[0], preserve_orientation, is_oriented_correctly, tris_adjacency_lists, ids_vec);

	//Remember the prefix count:
	rm_size prefix_count = ids_vec->count;

	//Append the core entrance vertices (they are still missing from the strip):
	for (rm_size i = 0; i < rm_array_count(core_entrance_vertex_ids); i++)
	{
		rm_vec_push(ids_vec, core_entrance_vertex_ids[i]);
	}

	//Grow the strip forward from the second over the third triangle and following:
	rm_tristripper_build_strip_loop(second_core_tri, third_core_tri, index_third_to_second, core_entrance_vertex_ids[1], core_entrance_vertex_ids[2], false, false, tris_adjacency_lists, ids_vec);

	//Allocate memory to hold the final strip:
	*strip = (rm_tristripper_strip)
	{
		.ids_count = ids_vec->count,
		.ids = rm_malloc(ids_vec->count * sizeof(rm_tristripper_id))
	};

	//Insert the prefix while fixing its order.
	//Note: A prefix count of 1 happens quite often which is nice.
	for (rm_size i = 0; i < prefix_count; i++)
	{
		//Get the current ID and insert it:
		strip->ids[i] = rm_vec_at(ids_vec, prefix_count - 1 - i);
	}

	//Copy the postfix:
	rm_mem_copy(&strip->ids[prefix_count], rm_vec_ptr_at(ids_vec, prefix_count), (ids_vec->count - prefix_count) *sizeof(rm_tristripper_id));

	//Clear the ID vector for the next strip:
	rm_vec_clear(ids_vec);
}

static inline rm_void rm_tristripper_build_strip_loop(rm_tristripper_tri* prev_tri, rm_tristripper_tri* tri, rm_size index_to_prev, rm_tristripper_id prev_entrance_vertex_id, rm_tristripper_id entrance_vertex_id, rm_bool preserve_orientation, rm_bool is_oriented_correctly, rm_tristripper_tri** tris_adjacency_lists, rm_tristripper_id_vec* ids_vec)
{
	//Note: "prev_tri" and "tri" are expected to be stripped and the entrances to be part of the strip!
	//This loop always pushes at least one index to the vector!

	while (true)
	{
		//Look for the best neighbour:
		rm_size best_neighbour_index = RM_TRISTRIPPER_NEIGHBOUR_INDEX_NOT_FOUND;
		rm_size best_unstripped_neighbours_count;
		rm_bool is_best_near;

		for (rm_size i = 0; i < 2; i++)
		{
			//Calculate the current neighbour index:
			rm_size curr_neighbour_index = rm_tristripper_tri_remaining_index(index_to_prev, i);

			//Ignore absent or already stripped neighbours:
			rm_tristripper_tri* curr_neighbour = tri->neighbours[curr_neighbour_index];

			if (!curr_neighbour || rm_tristripper_tri_is_stripped(curr_neighbour))
			{
				continue;
			}

			//How many unstripped neighbours does this one have?
			rm_size curr_unstripped_neighbours_count = (rm_size)curr_neighbour->unstripped_neighbours_count;

			//Do we already have another candidate?
			if (best_neighbour_index != RM_TRISTRIPPER_NEIGHBOUR_INDEX_NOT_FOUND)
			{
				//If the other candidate is better, we omit this one:
				if (best_unstripped_neighbours_count < curr_unstripped_neighbours_count)
				{
					break;
				}

				//If we have a tie, prefer the near candidate to avoid a swap:
				if ((best_unstripped_neighbours_count == curr_unstripped_neighbours_count) && is_best_near)
				{
					break;
				}
			}

			//We found a new best candidate :)
			best_neighbour_index = curr_neighbour_index;
			best_unstripped_neighbours_count = curr_unstripped_neighbours_count;
			is_best_near = (tri->vertices[curr_neighbour_index] == entrance_vertex_id) || (tri->vertices[(curr_neighbour_index + 1) % 3] == entrance_vertex_id);
		}

		//Did we find any neighbour?
		if (best_neighbour_index != RM_TRISTRIPPER_NEIGHBOUR_INDEX_NOT_FOUND)
		{
			if (is_best_near)
			{
				//The current entrance becomes the previous one:
				prev_entrance_vertex_id = entrance_vertex_id;

				//As soon as we append the next triangle in the backward-growing loop, the index of the first core triangle
				// changes from odd to even resp. from even to odd.
				is_oriented_correctly = !is_oriented_correctly;
			}
			else
			{
				//Perform the swap by appending the previous entrance again:
				rm_vec_push(ids_vec, prev_entrance_vertex_id);

				//The previous entrance stays the same here because we have inserted it again!
				//We also don't toggle the orientation state here because *two* triangles are appended.
			}
		}

		//Continue with the next entrance and push it.
		//It is located at the opposite vertex of the shared edge.
		entrance_vertex_id = tri->vertices[(index_to_prev + 2) % 3];
		rm_vec_push(ids_vec, entrance_vertex_id);

		//If there is no near and no far neighbour, we cannot grow the strip further :(
		if (best_neighbour_index == RM_TRISTRIPPER_NEIGHBOUR_INDEX_NOT_FOUND)
		{
			//At this point, we should fix the orientation if necessary:
			if (preserve_orientation && !is_oriented_correctly)
			{
				rm_vec_push(ids_vec, entrance_vertex_id);
			}

			return;
		}

		//Move to the next triangle:
		prev_tri = tri;
		tri = tri->neighbours[best_neighbour_index];

		//Update the index "tri -> prev_tri":
		index_to_prev = (rm_size)prev_tri->indices_at_neighbours[best_neighbour_index];

		//Mark the new neighbour as stripped:
		rm_tristripper_tri_set_stripped_and_propagate(tri, tris_adjacency_lists);
	}
}

rm_void rm_tristripper_create_strips_simple(rm_tristripper_tri* tris, rm_size tris_count, rm_bool preserve_orientation, rm_tristripper_strip** strips, rm_size* strips_count)
{
	//Validate the parameters:
	rm_assert(tris, "Passed triangles must be valid.");
	rm_assert(tris_count > 0, "Number of passed triangles must be > 0.");
	rm_assert(strips, "Passed strip outpointer must be valid.");
	rm_assert(strips_count, "Passed strip count outpointer must be valid.");

	//Sort the triangles by their neighbours count and stitch them together:
	rm_tristripper_tri* tris_adjacency_lists[4] = { null };
	rm_tristripper_order_tris(tris, tris_count, tris_adjacency_lists);

	//Collect tristrip pointers in a vector:
	rm_tristripper_strip_vec result_strips_vec;

	//Estimate the number of IDs per strip.
	//TODO: Bench and optimize!
	rm_size strips_count_estimation = tris_count / 4;

	rm_vec_init(&result_strips_vec);
	rm_vec_ensure_capacity(&result_strips_vec, strips_count_estimation);

	//Create a vector to collect the tristrip IDs into.
	//We reuse it for every strip to save some mallocs.
	rm_tristripper_id_vec ids_vec;

	//TODO: Bench and optimize!
	rm_size ids_count_estimation = rm_max(2 + tris_count, (rm_size)32);

	rm_vec_init(&ids_vec);
	rm_vec_ensure_capacity(&ids_vec, ids_count_estimation);

	//Spin through the lists in ascending order (=> prefer triangles with less neighbours) until all of them are empty.
	//Build exactly one tristrip in each iteration.
	rm_tristripper_tri* first_core_tri = null;

	do
	{
		for (rm_size i = 0; i < 4; i++)
		{
			first_core_tri = tris_adjacency_lists[i];

			if (rm_likely(first_core_tri != null))
			{
				//Some basic checks that should always hold true:
				rm_assert(!rm_tristripper_tri_is_stripped(first_core_tri), "The first triangle has already been stripped.");
				rm_assert((rm_size)first_core_tri->unstripped_neighbours_count == i, "Invalid unstripped neighbours count: %zu (found in list %zu)", (rm_size)first_core_tri->unstripped_neighbours_count, i);

				//Make space for a new tristrip and build it:
				rm_tristripper_strip* curr_strip = rm_vec_push_empty(&result_strips_vec);
				rm_tristripper_build_strip(first_core_tri, preserve_orientation, tris_adjacency_lists, &ids_vec, curr_strip);

				break;
			}
		}
	} while (first_core_tri);

	//Dispose the ID vector:
	rm_vec_dispose(&ids_vec);

	//Assign the resulting tristrips:
	*strips_count = result_strips_vec.count;
	*strips = rm_vec_unwrap(&result_strips_vec);
}
