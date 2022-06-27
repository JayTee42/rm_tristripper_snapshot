#include "rm_tristripper_ex.h"

#include "rm_mem.h"

//Logging?
//#define RM_TRISTRIPPER_EX_LOG

#ifdef RM_TRISTRIPPER_EX_LOG
#include "rm_log.h"
#endif

//The result of cementing a tunnel:
typedef enum __rm_tristripper_cement_tunnel_result__
{
	RM_TRISTRIPPER_CEMENT_TUNNEL_RESULT_SUCCESS,
	RM_TRISTRIPPER_CEMENT_TUNNEL_RESULT_FAIL_FOR_GOOD,
	RM_TRISTRIPPER_CEMENT_TUNNEL_RESULT_FAIL_BACKTRACKED,
	RM_TRISTRIPPER_CEMENT_TUNNEL_RESULT_FAIL_SHOULD_OPEN
} rm_tristripper_cement_tunnel_result;

//The dispatch points for our preprocessing algorithms.
//All of them take a collection of triangles, create strips from them and fill the given list of endpoints.
//The number of strips that has been created must be returned.
static rm_size rm_tristripper_create_strips_ex_isolated(rm_tristripper_tri* tris, rm_size tris_count, rm_tristripper_tri** tris_endpoint_list);
static rm_size rm_tristripper_create_strips_ex_pairs(rm_tristripper_tri* tris, rm_size tris_count, rm_tristripper_tri** tris_endpoint_list);
static rm_size rm_tristripper_create_strips_ex_stripify(rm_tristripper_tri* tris, rm_size tris_count, rm_tristripper_tri** tris_endpoint_list);

//Delineate tristrips into the graph.
//Version for "pairs":
static rm_void rm_tristripper_delineate_strip_pairs(rm_tristripper_tri* first_core_tri, rm_tristripper_tri** tris_adjacency_lists, rm_tristripper_tri** tris_endpoint_list);

//Version for "stripify":
static rm_void rm_tristripper_delineate_strip_stripify(rm_tristripper_tri* first_core_tri, rm_tristripper_tri** tris_adjacency_lists, rm_tristripper_tri** tris_endpoint_list);
static rm_tristripper_tri* rm_tristripper_delineate_strip_stripify_loop(rm_tristripper_tri* prev_tri, rm_tristripper_tri* tri, rm_size index_to_prev, rm_tristripper_id entrance_vertex_id, rm_tristripper_tri** tris_adjacency_lists);

//Take a list of endpoints and create strips from them via "rm_tristripper_tunnel_all_the_strips(...)".
//Then, follow all those strips across the graph and write them to the output via "rm_tristripper_collect_strip(...)".
static rm_void rm_tristripper_tri_create_strips_from_endpoints(rm_tristripper_tri** tris_endpoint_list, rm_tristripper_config* config, rm_tristripper_strip** strips, rm_size* strips_count_inout);

//Try to move from one triangle in the graph to the next one of its associated strip.
//Yes, each inner strip triangle has two tunnel neighbours, but "*index_to_prev_inout" denotes in which direction we *don't* want to move.
//If that does not work out because "*tri_inout" points to an endpoint, "false" is returned.
//Otherwise, we retrieve "true" and both "*tri_inout" and "*index_to_prev_inout" are updated.
static rm_bool rm_tristripper_traverse_strip(rm_tristripper_tri** tri_inout, rm_size* index_to_prev_inout);

//Collect a tristrip starting at "first_tri" (which must be an endpoint).
//Write it to the given output pointer and return the second endpoint.
static rm_tristripper_tri* rm_tristripper_collect_strip(const rm_tristripper_tri* first_tri, rm_bool preserve_orientation, rm_tristripper_strip* strip);
static rm_tristripper_tri* rm_tristripper_collect_strip_loop(rm_tristripper_tri* curr_tri, rm_size curr_index_to_prev, rm_tristripper_id prev_entrance_vertex_id, rm_tristripper_id curr_entrance_vertex_id, rm_tristripper_id_vec* strip_ids_vec);

//Apply the tunneling algorithm to all the passed tristrips.
//For each tunnel, up to two endpoints are returned from the endpoint list.
//Return how many strips are left.
static rm_size rm_tristripper_tunnel_all_the_strips(rm_tristripper_tri** tris_endpoint_list, rm_size strips_count, const rm_tristripper_config* config);

//Dig a tunnel starting at "first_endpoint" (which must be an endpoint).
//Use the provided config.
//If "true" is returned, tunnel building has succeeded and the second endpoint of the tunnel is written to "*second_endpoint".
//If no such tunnel can be found, "false" is returned.
static rm_bool rm_tristripper_dig_tunnel(rm_tristripper_tri* first_endpoint, rm_tristripper_tri** tunnel, const rm_tristripper_config* config, rm_tristripper_tri** second_endpoint);

//"Open" a triangle. This translates to "add all valid tunnel states" to it.
//Return if there is at least one valid tunnel state.
static rm_bool rm_tristripper_open_tri(rm_tristripper_tri* tri, rm_size index_to_prev, rm_bool is_tri_red);

//In many cases, we have run into a dead-end and must backtrack a given tunnel stack.
//We pass the current tunnel index via "*tunnel_index_inout".
//The index to start backtracking at is passed in "backtrack_tunnel_index" (<= "*tunnel_index_inout").
//If that does not work out, we try (backtrack_tunnel_index - 1) and so on ...
//If backtracking fails completely, "false" is returned.
//Otherwise, we update "*tunnel_index_inout" to the open node we have found, and return "true".
static rm_bool rm_tristripper_backtrack_tunnel(rm_tristripper_tri** tunnel, rm_size* tunnel_index_inout, rm_size backtrack_tunnel_index);

//Check if the given tunnel has circles.
//If the check succeeds (no circles), "false" is returned. In that case, all "is_visited" flags on tunnel triangles have been cleared.
//Otherwise, "true" is returned and "*last_tunnel_circle_index" receives the tunnel index of the last tunnel triangle that is part of the circle.
//In that case, the "is_visited" state of the tunnel triangles is undefined.
static rm_bool rm_tristripper_check_tunnel_for_circles(rm_tristripper_tri** tunnel, rm_size tunnel_index, rm_size* last_tunnel_circle_index);

//Try to "cement" the provided tunnel. There are four possible outcomes:
// - RM_TRISTRIPPER_CEMENT_TUNNEL_RESULT_SUCCESS: The tunnel has been cemented.
// - RM_TRISTRIPPER_CEMENT_TUNNEL_RESULT_FAIL_FOR_GOOD: The tunnel could not be cemented and backtracking failed for good. Abort mission!
// - RM_TRISTRIPPER_CEMENT_TUNNEL_RESULT_FAIL_BACKTRACKED: We failed to cement the tunnel and had to backtrack, but succeeded. "*last_tunnel_circle_index" hs been updated.
// - RM_TRISTRIPPER_CEMENT_TUNNEL_RESULT_FAIL_SHOULD_OPEN: Also a failure, but no backtracking occurred. Instead, we should try to open the current endpoint of the tunnel.
static rm_tristripper_cement_tunnel_result rm_tristripper_cement_tunnel(rm_tristripper_tri** tunnel, rm_size* tunnel_index_inout);

static rm_size rm_tristripper_create_strips_ex_isolated(rm_tristripper_tri* tris, rm_size tris_count, rm_tristripper_tri** tris_endpoint_list)
{
	//Iterate over all triangles and make them isolated endpoints:
	for (rm_size i = 0; i < tris_count; i++)
	{
		rm_tristripper_tri* curr_tri = &tris[i];

		rm_tristripper_tri_set_endpoint(curr_tri);
		rm_tristripper_tri_prepend_to_list(curr_tri, tris_endpoint_list);
	}

	return tris_count;
}

static rm_size rm_tristripper_create_strips_ex_pairs(rm_tristripper_tri* tris, rm_size tris_count, rm_tristripper_tri** tris_endpoint_list)
{
	//Sort the triangles by their neighbours count and stitch them together:
	rm_tristripper_tri* tris_adjacency_lists[4] = { null };
	rm_tristripper_order_tris(tris, tris_count, tris_adjacency_lists);

	//Spin through the lists in ascending order (=> prefer triangles with less neighbours) until all of them are empty.
	//Delineate exactly one tristrip in each iteration.
	rm_tristripper_tri* first_core_tri = null;
	rm_size strips_count = 0;

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

				//Delineate a tristrip into the graph and increment the count:
				rm_tristripper_delineate_strip_pairs(first_core_tri, tris_adjacency_lists, tris_endpoint_list);
				strips_count++;

				break;
			}
		}
	} while (first_core_tri);

	return strips_count;
}

static rm_size rm_tristripper_create_strips_ex_stripify(rm_tristripper_tri* tris, rm_size tris_count, rm_tristripper_tri** tris_endpoint_list)
{
	//Sort the triangles by their neighbours count and stitch them together:
	rm_tristripper_tri* tris_adjacency_lists[4] = { null };
	rm_tristripper_order_tris(tris, tris_count, tris_adjacency_lists);

	//Spin through the lists in ascending order (=> prefer triangles with less neighbours) until all of them are empty.
	//Delineate exactly one tristrip in each iteration.
	rm_tristripper_tri* first_core_tri = null;
	rm_size strips_count = 0;

	do
	{
		for (rm_size i = 0; i < 4; i++)
		{
			first_core_tri = tris_adjacency_lists[i];

			if (first_core_tri)
			{
				//Some basic checks that should always hold true:
				rm_assert(!rm_tristripper_tri_is_stripped(first_core_tri), "The first triangle has already been stripped.");
				rm_assert((rm_size)first_core_tri->unstripped_neighbours_count == i, "Invalid unstripped neighbours count: %zu (found in list %zu)", (rm_size)first_core_tri->unstripped_neighbours_count, i);

				//Delineate a tristrip into the graph and increment the count:
				rm_tristripper_delineate_strip_stripify(first_core_tri, tris_adjacency_lists, tris_endpoint_list);
				strips_count++;

				break;
			}
		}
	} while (first_core_tri);

	return strips_count;
}

static rm_void rm_tristripper_delineate_strip_pairs(rm_tristripper_tri* first_core_tri, rm_tristripper_tri** tris_adjacency_lists, rm_tristripper_tri** tris_endpoint_list)
{
	//Mark the start triangle as stripped and as an endpoint:
	rm_tristripper_tri_set_stripped_and_propagate(first_core_tri, tris_adjacency_lists);
	rm_tristripper_tri_set_endpoint(first_core_tri);
	rm_tristripper_tri_prepend_to_list(first_core_tri, tris_endpoint_list);

	//Get any neighbour of the first core triangle that has not been stripped yet.
	//We also need the index "first -> second".
	rm_tristripper_id first_shared_edge_unused[2];
	rm_size index_first_to_second;

	rm_tristripper_tri* second_core_tri = rm_tristripper_select_next_core_tri(first_core_tri, tris_adjacency_lists, first_shared_edge_unused, &index_first_to_second);

	//If there is no such neighbour, we are done.
	if (!second_core_tri)
	{
		return;
	}

	//Mark the second triangle as endpoint, too:
	rm_tristripper_tri_set_endpoint(second_core_tri);
	rm_tristripper_tri_prepend_to_list(second_core_tri, tris_endpoint_list);

	//Link first <-> second:
	rm_size index_second_to_first = (rm_size)first_core_tri->indices_at_neighbours[index_first_to_second];

	rm_tristripper_tri_link_to_neighbour(first_core_tri, index_first_to_second);
	rm_tristripper_tri_link_to_neighbour(second_core_tri, index_second_to_first);
}

static rm_void rm_tristripper_delineate_strip_stripify(rm_tristripper_tri* first_core_tri, rm_tristripper_tri** tris_adjacency_lists, rm_tristripper_tri** tris_endpoint_list)
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
		//"first_core_tri" is a full tristrip and also an endpoint (with a multiplicity of 2, but we only reference it one time).
		rm_tristripper_tri_set_endpoint(first_core_tri);
		rm_tristripper_tri_prepend_to_list(first_core_tri, tris_endpoint_list);

		return;
	}

	//Link first <-> second:
	rm_size index_second_to_first = (rm_size)first_core_tri->indices_at_neighbours[index_first_to_second];

	rm_tristripper_tri_link_to_neighbour(first_core_tri, index_first_to_second);
	rm_tristripper_tri_link_to_neighbour(second_core_tri, index_second_to_first);

	//Look at the two neighbours of the second core triangle.
	//Select one of them that has not been stripped yet as the third one, along with the edge shared with the second and the index.
	rm_tristripper_id second_shared_edge[2];
	rm_size index_second_to_third;

	rm_tristripper_tri* third_core_tri = rm_tristripper_select_next_core_tri(second_core_tri, tris_adjacency_lists, second_shared_edge, &index_second_to_third);

	//If there is no such neighbour, we are also done.
	if (!third_core_tri)
	{
		//"first_core_tri" and "second_core_tri" form a complete strip.
		rm_tristripper_tri_set_endpoint(first_core_tri);
		rm_tristripper_tri_prepend_to_list(first_core_tri, tris_endpoint_list);

		rm_tristripper_tri_set_endpoint(second_core_tri);
		rm_tristripper_tri_prepend_to_list(second_core_tri, tris_endpoint_list);

		return;
	}

	//Link second <-> third:
	rm_size index_third_to_second = (rm_size)second_core_tri->indices_at_neighbours[index_second_to_third];

	rm_tristripper_tri_link_to_neighbour(second_core_tri, index_second_to_third);
	rm_tristripper_tri_link_to_neighbour(third_core_tri, index_third_to_second);

	//Determine the entrance vertices of the first and third core triangle, as viewed from the second one.
	//Note: We don't need the second entry, but it is used in "rm_tristripper_collect_strip(...)".
	rm_tristripper_id core_entrance_vertex_ids[3];
	rm_tristripper_determine_core_entrance_vertex_ids(first_shared_edge, second_shared_edge, core_entrance_vertex_ids);

	//Grow the strip from the second core triangle in both directions:
	rm_tristripper_tri* first_end_tri = rm_tristripper_delineate_strip_stripify_loop(second_core_tri, first_core_tri, index_first_to_second, core_entrance_vertex_ids[0], tris_adjacency_lists);
	rm_tristripper_tri* second_end_tri = rm_tristripper_delineate_strip_stripify_loop(second_core_tri, third_core_tri, index_third_to_second, core_entrance_vertex_ids[2], tris_adjacency_lists);

	//Mark both of them as endpoints and prepend them to the list:
	rm_tristripper_tri_set_endpoint(first_end_tri);
	rm_tristripper_tri_prepend_to_list(first_end_tri, tris_endpoint_list);

	rm_tristripper_tri_set_endpoint(second_end_tri);
	rm_tristripper_tri_prepend_to_list(second_end_tri, tris_endpoint_list);
}

static rm_tristripper_tri* rm_tristripper_delineate_strip_stripify_loop(rm_tristripper_tri* prev_tri, rm_tristripper_tri* tri, rm_size index_to_prev, rm_tristripper_id entrance_vertex_id, rm_tristripper_tri** tris_adjacency_lists)
{
	//Note: "prev_tri" and "tri" are expected to be stripped and linked together in both directions.

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

		//If there is no near and no far neighbour, we cannot grow the strip further :(
		if (best_neighbour_index == RM_TRISTRIPPER_NEIGHBOUR_INDEX_NOT_FOUND)
		{
			//"tri" is an end triangle.
			return tri;
		}

		//Retrieve the next entrance vertex ID.
		//It is located at the opposite vertex of the shared edge.
		entrance_vertex_id = tri->vertices[(index_to_prev + 2) % 3];

		//Move to the next triangle:
		prev_tri = tri;
		tri = tri->neighbours[best_neighbour_index];

		//Update the index "tri -> prev_tri":
		index_to_prev = (rm_size)prev_tri->indices_at_neighbours[best_neighbour_index];

		//Establish the links between the two triangles:
		rm_tristripper_tri_link_to_neighbour(prev_tri, best_neighbour_index);
		rm_tristripper_tri_link_to_neighbour(tri, index_to_prev);

		//Mark the new neighbour as stripped:
		rm_tristripper_tri_set_stripped_and_propagate(tri, tris_adjacency_lists);
	}
}

static rm_void rm_tristripper_tri_create_strips_from_endpoints(rm_tristripper_tri** tris_endpoint_list, rm_tristripper_config* config, rm_tristripper_strip** strips, rm_size* strips_count_inout)
{
	//Get the current number of strips:
	rm_size result_strips_count = *strips_count_inout;

	//Perform tunneling:
	if (config->incremental)
	{
		//First, try all tunnels of length 2, then all of length 4, ...
		rm_size max_count = config->max_count;

		for (rm_size i = 2; i <= max_count; i += 2)
		{
			config->max_count = i;
			result_strips_count = rm_tristripper_tunnel_all_the_strips(tris_endpoint_list, result_strips_count, config);
		}
	}
	else
	{
		//Try the maximum length immediately:
		result_strips_count = rm_tristripper_tunnel_all_the_strips(tris_endpoint_list, result_strips_count, config);
	}

	//Allocate the result array:
	rm_tristripper_strip* result_strips = rm_malloc(result_strips_count * sizeof(rm_tristripper_strip));

	//Spin through the list of endpoints. Create one strip from each endpoint we encounter.
	rm_tristripper_tri* first_endpoint = *tris_endpoint_list;

	for (rm_size i = 0; i < result_strips_count; i++)
	{
		//Collect the strip that starts at "first_endpoint" and prepare it for output.
		//The return value of this call is the other endpoint of the strip if there is one (it could also be isolated).
		//We should remove it from the linked list.
		//Otherwise, we would build the same strip a second time in the other direction as soon as we encouter it.
		rm_tristripper_tri* second_endpoint = rm_tristripper_collect_strip(first_endpoint, config->preserve_orientation, &result_strips[i]);

		if (second_endpoint)
		{
			rm_tristripper_tri_remove_from_list(second_endpoint, tris_endpoint_list);
		}

		//Get the next first endpoint:
		first_endpoint = first_endpoint->next_tri;
	}

	//By definition, we must have reached the end of the list now:
	rm_assert(!first_endpoint, "Created all %zu tristrips, but there are still endpoints left.", result_strips_count);

	//Assign the resulting tristrips:
	*strips_count_inout = result_strips_count;
	*strips = result_strips;
}

static rm_bool rm_tristripper_traverse_strip(rm_tristripper_tri** tri_inout, rm_size* index_to_prev_inout)
{
	//Get the inout triangle:
	const rm_tristripper_tri* curr_tri = *tri_inout;

	//First, we have to test if "curr_tri" is an endpoint.
	//In that case, we are instantly done and the whole strip has been traversed.
	if (rm_tristripper_tri_is_endpoint(curr_tri))
	{
		return false;
	}

	//Get the inout index:
	rm_size curr_index_to_prev = *index_to_prev_inout;

	//Iterate over the neighbours of the triangle:
	for (rm_size i = 0; i < 2; i++)
	{
		//Because we already know the index of the previous triangle, we don't have to check that one.
		//Example: If the previous one is at index 1, we only have to check the indices 0 and 2.
		rm_size curr_neighbour_index = rm_tristripper_tri_remaining_index(curr_index_to_prev, i);

		//Test if the corresponding neighbour is linked:
		if (!rm_tristripper_tri_is_linked_to_neighbour(curr_tri, curr_neighbour_index))
		{
			continue;
		}

		//Move to the next triangle:
		*tri_inout = curr_tri->neighbours[curr_neighbour_index];

		//Index at the next triangle that points back to "curr_tri" and can therefore be ignored:
		*index_to_prev_inout = (rm_size)curr_tri->indices_at_neighbours[curr_neighbour_index];

		return true;
	}

	//If we end up here, the point is not marked as endpoint, but is not linked to a neighbour apart from the previous one.
	//That should never happen and is an error case!
	rm_exit("Stranded at a non-endpoint triangle without linked neighbours.");
}

static rm_tristripper_tri* rm_tristripper_collect_strip(const rm_tristripper_tri* first_tri, rm_bool preserve_orientation, rm_tristripper_strip* strip)
{
	//At this point, the first triangle must always be an endpoint:
	rm_assert(rm_tristripper_tri_is_endpoint(first_tri), "rm_tristripper_collect_strip(...) must be called with an endpoint.");

	//Get the index of the second triangle of the strip as seen from the first one, the back-linking index and the shared edge between them.
	rm_size index_first_to_second = RM_TRISTRIPPER_NEIGHBOUR_INDEX_NOT_FOUND;
	rm_size curr_index_to_prev;
	rm_tristripper_id first_shared_edge[2];

	for (rm_size i = 0; i < rm_array_count(first_tri->neighbours); i++)
	{
		//Test if the corresponding neighbour is linked:
		if (!rm_tristripper_tri_is_linked_to_neighbour(first_tri, i))
		{
			continue;
		}

		index_first_to_second = i;
		curr_index_to_prev = (rm_size)first_tri->indices_at_neighbours[i];

		first_shared_edge[0] = first_tri->vertices[i];
		first_shared_edge[1] = first_tri->vertices[(i + 1) % 3];

		break;
	}

	//If there is no second triangle, "first_tri" has a multiplicity of 2 as an endpoint.
	//As a result, we have to create a tristrip with a single triangle in it :(
	if (index_first_to_second == RM_TRISTRIPPER_NEIGHBOUR_INDEX_NOT_FOUND)
	{
		strip->ids_count = 3;
		strip->ids = rm_mem_dup(first_tri->vertices, 3 * sizeof(rm_tristripper_id));

		return null;
	}

	//The is a second triangle. Get it.
	rm_tristripper_tri* second_tri = first_tri->neighbours[index_first_to_second];

	//Now we can derive the first vertex ID of the strip:
	rm_tristripper_id first_vertex_id = first_tri->vertices[(index_first_to_second + 2) % 3];

	//Test if the second triangle is an endpoint.
	//In that case, we can create a tristrip from the two triangles and are done.
	//Still not that great :(
	if (rm_tristripper_tri_is_endpoint(second_tri))
	{
		strip->ids_count = 4;
		strip->ids = rm_malloc(4 * sizeof(rm_tristripper_id));

		//Append the vertices of the first triangle in correct orientation:
		strip->ids[0] = first_vertex_id;
		strip->ids[1] = first_tri->vertices[index_first_to_second];
		strip->ids[2] = first_tri->vertices[(index_first_to_second + 1) % 3];

		//Append the last vertex (from the second triangle):
		strip->ids[3] = second_tri->vertices[(curr_index_to_prev + 2) % 3];

		return second_tri;
	}

	//Get the third triangle from the second one.
	//Update "curr_index_to_prev" and also find the shared edge.
	rm_tristripper_tri* third_tri = null;
	rm_tristripper_id second_shared_edge[2] = { 0 };

	for (rm_size i = 0; i < 2; i++)
	{
		//Calculate the current neighbour index:
		rm_size curr_neighbour_index = rm_tristripper_tri_remaining_index(curr_index_to_prev, i);

		//Test if the corresponding neighbour is linked:
		if (!rm_tristripper_tri_is_linked_to_neighbour(second_tri, curr_neighbour_index))
		{
			continue;
		}

		third_tri = second_tri->neighbours[curr_neighbour_index];
		curr_index_to_prev = (rm_size)second_tri->indices_at_neighbours[curr_neighbour_index];

		second_shared_edge[0] = second_tri->vertices[curr_neighbour_index];
		second_shared_edge[1] = second_tri->vertices[(curr_neighbour_index + 1) % 3];

		break;
	}

	rm_assert(third_tri, "Stranded at the third triangle (not an endpoint) without linked neighbours.");

	//Determine the entrance vertices of all three triangles:
	rm_tristripper_id core_entrance_vertex_ids[3];
	rm_tristripper_determine_core_entrance_vertex_ids(first_shared_edge, second_shared_edge, core_entrance_vertex_ids);

	//Initialize a vector of vertex IDs for the result:
	rm_tristripper_id_vec strip_ids_vec;
	rm_vec_init(&strip_ids_vec);

	//TODO: Bench and optimize!
	rm_vec_ensure_capacity(&strip_ids_vec, 32);

	//Push the first vertex ID:
	rm_vec_push(&strip_ids_vec, first_vertex_id);

	//If we have to fix the orientation, the first vertex ID must be repeated:
	if (preserve_orientation && (first_tri->vertices[index_first_to_second] != core_entrance_vertex_ids[0]))
	{
		rm_vec_push(&strip_ids_vec, first_vertex_id);
	}

	//Push the entrance vertex IDs of the core:
	for (rm_size i = 0; i < rm_array_count(core_entrance_vertex_ids); i++)
	{
		rm_vec_push(&strip_ids_vec, core_entrance_vertex_ids[i]);
	}

	//Keep traversing until we reach an endpoint:
	rm_tristripper_tri* last_tri = rm_tristripper_collect_strip_loop(third_tri, curr_index_to_prev, core_entrance_vertex_ids[1], core_entrance_vertex_ids[2], &strip_ids_vec);

	//Build the strip by unwrapping the vector:
	strip->ids_count = strip_ids_vec.count;
	strip->ids = rm_vec_unwrap(&strip_ids_vec);

	//Return the second endpoint of the strip:
	return last_tri;
}

static rm_tristripper_tri* rm_tristripper_collect_strip_loop(rm_tristripper_tri* curr_tri, rm_size curr_index_to_prev, rm_tristripper_id prev_entrance_vertex_id, rm_tristripper_id curr_entrance_vertex_id, rm_tristripper_id_vec* strip_ids_vec)
{
	rm_bool are_tris_left;

	do
	{
		//Determine the next entrance vertex ID.
		//We find it at the opposite end of the previous triangle.
		//This vertex ID must *always* (also in the case of endpoints) be pushed to the end of the strip to complete "curr_tri".
		rm_tristripper_id next_entrance_vertex_id = curr_tri->vertices[(curr_index_to_prev + 2) % 3];

		//Try to move to the next triangle.
		//If we have found an endpoint, the traversal function returns "false".
		are_tris_left = rm_tristripper_traverse_strip(&curr_tri, &curr_index_to_prev);

		if (are_tris_left)
		{
			//Do we have to perform a swap?
			//This can be determined by looking at the edge between the (now) current and (now) previous neighbour.
			//If this edge contains the current entrance vertex, we have near neighbours. Otherwise, both triangles are far neighbours.
			//Only in the latter case we need a swap.

			if (rm_likely((curr_tri->vertices[curr_index_to_prev] == curr_entrance_vertex_id) || (curr_tri->vertices[(curr_index_to_prev + 1) % 3] == curr_entrance_vertex_id)))
			{
				//No swap!
				//Update the previous entrance vertex:
				prev_entrance_vertex_id = curr_entrance_vertex_id;
			}
			else
			{
				//Swap!
				//Push the previous entrance vertex again:
				rm_vec_push(strip_ids_vec, prev_entrance_vertex_id);
			}
		}

		//Make the next entrance vertex the current one and push it:
		curr_entrance_vertex_id = next_entrance_vertex_id;
		rm_vec_push(strip_ids_vec, curr_entrance_vertex_id);
	} while (are_tris_left);

	//Return the last triangle we have visited as it is the second endpoint of the strip:
	return curr_tri;
}

static rm_size rm_tristripper_tunnel_all_the_strips(rm_tristripper_tri** tris_endpoint_list, rm_size strips_count, const rm_tristripper_config* config)
{
	//Allocate the stack for the tunnel DFS:
	rm_tristripper_tri** tunnel = rm_malloc(config->max_count * sizeof(rm_tristripper_tri*));

	//Iterate through the remaining endpoints until only one strip is left or we have found no new tunnel:
	rm_bool has_found_tunnel;

	do
	{
		has_found_tunnel = false;

		//Iterate through the endpoints:
		rm_tristripper_tri* first_endpoint = *tris_endpoint_list;

		//At this point, "first_endpoint" is never null because there are strips left.
		do
		{
			//Is the destination count already reached?
			if ((config->dest_count != RM_TRISTRIPPER_NO_DEST_COUNT) && (strips_count <= config->dest_count))
			{
				//We have reached the destination count.
				//Get out of here!
				has_found_tunnel = false;
				break;
			}

			//Try to dig a tunnel originating from that endpoint.
			//If that works out, we receive a pointer to the other endpoint we have tunneled to.
			rm_tristripper_tri* second_endpoint = null;

			if (!rm_tristripper_dig_tunnel(first_endpoint, tunnel, config, &second_endpoint))
			{
				//Just move to the next endpoint:
				first_endpoint = first_endpoint->next_tri;
				continue;
			}

			//We got a tunnel :)
			//One strip has been eliminated.
			has_found_tunnel = true;
			strips_count--;
#ifdef RM_TRISTRIPPER_EX_LOG
			rm_log(RM_LOG_TYPE_DEBUG, "Tunnel found, %zu strips remaining ...", strips_count);
#endif
			//Remove the endpoints from the list if they are no endpoints anymore:
			if (!rm_tristripper_tri_is_endpoint(first_endpoint))
			{
				//Get a next endpoint to continue with.
				//A good candidate is the successor of "first_endpoint" - *except* that one is "second_endpoint"!
				rm_tristripper_tri* next_endpoint = ((first_endpoint->next_tri != second_endpoint) ? first_endpoint : second_endpoint)->next_tri;
				rm_tristripper_tri_remove_from_list(first_endpoint, tris_endpoint_list);

				//Move to the next endpoint:
				first_endpoint = next_endpoint;
			}

			//If "first_endpoint" is still an endpoint, we continue with it.

			if (!rm_tristripper_tri_is_endpoint(second_endpoint))
			{
				rm_tristripper_tri_remove_from_list(second_endpoint, tris_endpoint_list);
			}
		} while (first_endpoint && (strips_count > 1));
	} while (has_found_tunnel && (strips_count > 1));

	//Free the tunnel stack:
	rm_free(tunnel);

	//Return the (hopefully) reduced number of strips:
	return strips_count;
}

static rm_bool rm_tristripper_dig_tunnel(rm_tristripper_tri* first_endpoint, rm_tristripper_tri** tunnel, const rm_tristripper_config* config, rm_tristripper_tri** second_endpoint)
{
	rm_assert(rm_tristripper_tri_is_endpoint(first_endpoint), "rm_tristripper_tunnel_strip(...) expects an endpoint.");

	//Try to open the first endpoint manually (it's always red):
	rm_tristripper_tri_init_tunnel_state(first_endpoint);

	for (rm_size i = 0; i < rm_array_count(first_endpoint->neighbours); i++)
	{
		//Only valid neighbours from other strips:
		if (!first_endpoint->neighbours[i] || rm_tristripper_tri_is_linked_to_neighbour(first_endpoint, i))
		{
			continue;
		}

		//Add the tunnel state:
		rm_tristripper_tri_add_tunnel_state(first_endpoint, i);
	}

	//If an endpoint has no non-linked neighbours, we cannot continue.
	if (rm_tristripper_tri_is_tunnel_state_depleted(first_endpoint))
	{
		return false;
	}

	//Start a new tunnel stack:
	tunnel[0] = first_endpoint;
	rm_size tunnel_index = 0;

	rm_tristripper_tri_set_visited(first_endpoint, 0);

	//Track the number of loop iterations:
	rm_size tunnel_loop_count = 0;

	while (true)
	{
		//Do we have to limit the number of loop iterations?
		//Increment the count. Shall we backtrack?
		if ((config->loop_limit != RM_TRISTRIPPER_NO_LOOP_LIMIT) && (tunnel_loop_count++ >= config->loop_limit))
		{
			//If no backtracking is desired, we unvisit all tunnel members and return.
			if (!config->backtrack_after_loop_limit)
			{
#ifdef RM_TRISTRIPPER_EX_LOG
				rm_log(RM_LOG_TYPE_DEBUG, "Tunnel limit reached, cancelling ...");
#endif
				//Just mark everything as non-visited and return.
				for (rm_size i = 0; i <= tunnel_index; i++)
				{
					rm_tristripper_tri_set_unvisited(tunnel[tunnel_index]);
				}

				return false;
			}

			//Try to backtrack down to zero.
			//If that fails, we have to return.
#ifdef RM_TRISTRIPPER_EX_LOG
			rm_log(RM_LOG_TYPE_DEBUG, "Tunnel limit reached, backtracking to 0 ...");
#endif
			if (!rm_tristripper_backtrack_tunnel(tunnel, &tunnel_index, 0))
			{
				return false;
			}

			//Backtracking has worked!
			//Reset the loop count and carry on.
			tunnel_loop_count = 0;
		}

		//Our state at this point:
		//At "tunnel_index", we have stored a triangle with at least one valid tunnel state that is currently selected.
		//In addition, we must not end up here if the stack is completely filled.
		rm_assert(tunnel_index < (config->max_count - 1), "Tried to enter tunnel loop with full stack.");

		//Get the last triangle:
		rm_tristripper_tri* last_tri = tunnel[tunnel_index];

		//Get the successor of the last triangle:
		rm_size curr_tri_index_from_last_tri = rm_tristripper_tri_get_tunnel_successor_index(last_tri);
		rm_tristripper_tri* curr_tri = last_tri->neighbours[curr_tri_index_from_last_tri];

		//Our opening function only adds tunnel state for non-visited neighbours:
		rm_assert(!rm_tristripper_tri_is_visited(curr_tri), "Encountered already visited triangle via valid tunnel state.");

		//Push the triangle on the stack and mark it as visited:
		tunnel[++tunnel_index] = curr_tri;
		rm_tristripper_tri_set_visited(curr_tri, tunnel_index);

		//Determine if the current triangle is red or black:
		rm_bool is_curr_tri_red = ((tunnel_index % 2) == 0);

		//If the current triangle is black and an endpoint, we should try to cement the tunnel with it:
		if (!is_curr_tri_red && rm_tristripper_tri_is_endpoint(curr_tri))
		{
			/*
				See if we can cement the tunnel with the new endpoint.
				Potential outcomes:
				 - Success -> we are done.
				 - Failure because of a circle ->
			       - "curr_tri" is part of the circle -> open it.
			       - "curr_tri" is not part of the circle -> backtrack starting at the last triangle
			                                                 that is part of both the circle and the tunnel.
			                                                 If that fails, we are done for good.
			*/

			switch (rm_tristripper_cement_tunnel(tunnel, &tunnel_index))
			{
			case RM_TRISTRIPPER_CEMENT_TUNNEL_RESULT_SUCCESS:

				//Nice :) assign the second endpoint and return.
				*second_endpoint = curr_tri;
				return true;

			case RM_TRISTRIPPER_CEMENT_TUNNEL_RESULT_FAIL_FOR_GOOD:

				//We had to backtrack and that failed. No chance to recover anymore.
				return false;

			case RM_TRISTRIPPER_CEMENT_TUNNEL_RESULT_FAIL_BACKTRACKED:

				//We had to backtrack, but that succeeded.
				//Continue with the next triangle.
				continue;

			case RM_TRISTRIPPER_CEMENT_TUNNEL_RESULT_FAIL_SHOULD_OPEN:

				//We couldn't close the tunnel, but it is safe to continue by opening "curr_tri".
				break;
			}
		}

		//We have tried to close the tunnel, but failed.
		//The tunnel index has not changed, we still point at "curr_tri".
		rm_assert(tunnel[tunnel_index] == curr_tri, "Tunnel index has changed, we lost the current triangle.");

		//If the tunnel stack is full now, we cannot look for further expansion here ("open" it).
		//Instead, we have to backtrack until a valid tunnel state has been found.
		//Taking one step back might already be enough.
		if (tunnel_index == (config->max_count - 1))
		{
			if (!rm_tristripper_backtrack_tunnel(tunnel, &tunnel_index, tunnel_index - 1))
			{
				//Backtracking failed, this tunnel cannot be completed.
				return false;
			}

			//Continue with backtracked candidate:
			continue;
		}

		//There is still space left on the stack.
		//Let's append tunnel states for "curr_tri" (that's what "opening" it means).
		//If the current triangle has no valid tunnel states, we have to backtrack one step, too.
		rm_size last_tri_index_from_curr_tri = (rm_size)last_tri->indices_at_neighbours[curr_tri_index_from_last_tri];

		if (!rm_tristripper_open_tri(curr_tri, last_tri_index_from_curr_tri, is_curr_tri_red))
		{
			if (!rm_tristripper_backtrack_tunnel(tunnel, &tunnel_index, tunnel_index - 1))
			{
				//Backtracking failed, this tunnel cannot be completed.
				return false;
			}
		}

		//Continue with next or backtracked candidate.
	}
}

static rm_bool rm_tristripper_open_tri(rm_tristripper_tri* tri, rm_size index_to_prev, rm_bool is_tri_red)
{
	//Look at the remaining neighbours:
	rm_tristripper_tri_init_tunnel_state(tri);

	for (rm_size i = 0; i < 2; i++)
	{
		rm_size curr_neighbour_index = rm_tristripper_tri_remaining_index(index_to_prev, i);
		rm_tristripper_tri* curr_neighbour = tri->neighbours[curr_neighbour_index];

		//Valid, from same / other strip and unvisited?
		if (!curr_neighbour || (is_tri_red == rm_tristripper_tri_is_linked_to_neighbour(tri, curr_neighbour_index)) || rm_tristripper_tri_is_visited(curr_neighbour))
		{
			continue;
		}

		//Add tunnel state:
		rm_tristripper_tri_add_tunnel_state(tri, curr_neighbour_index);
	}

	//Return if there are valid tunnel states now:
	return !rm_tristripper_tri_is_tunnel_state_depleted(tri);
}

static rm_bool rm_tristripper_backtrack_tunnel(rm_tristripper_tri** tunnel, rm_size* tunnel_index_inout, rm_size backtrack_tunnel_index)
{
	//Get the current tunnel index from the inout parameter:
	rm_size tunnel_index = *tunnel_index_inout;

	//Backtracking means ... well ... going back ...
	rm_assert(backtrack_tunnel_index <= tunnel_index, "Tried to backtrack forward from %zu to %zu.", tunnel_index, backtrack_tunnel_index);

	//First, unvisit all those triangles strictly above the backtrack index.
	//They have lost the privilege to belong to the tunnel!
	for (; tunnel_index > backtrack_tunnel_index; tunnel_index--)
	{
		rm_tristripper_tri_set_unvisited(tunnel[tunnel_index]);
	}

	//Now try to backtrack the current triangle.
	//If it has no valid tunnel state left, we unvisit it and go back one step further etc.
	//The start of the tunnel is critical (and "tunnel_index" is unsigned, therefore no ">= here)!
	for (; tunnel_index > 0; tunnel_index--)
	{
		rm_tristripper_tri* curr_tri = tunnel[tunnel_index];

		if (rm_tristripper_tri_select_next_tunnel_state(curr_tri))
		{
			//Success, backtracking has worked!
			*tunnel_index_inout = tunnel_index;

			return true;
		}

		//The current triangle is out of tunnel states, so we abandon it, too.
		rm_tristripper_tri_set_unvisited(curr_tri);
	}

	//If we fail to backtrack the first endpoint, digging this tunnel has failed for good :(
	rm_tristripper_tri* first_endpoint = tunnel[0];

	if (!rm_tristripper_tri_select_next_tunnel_state(first_endpoint))
	{
		rm_tristripper_tri_set_unvisited(first_endpoint);
		return false;
	}

	//Phew, we were able to backtrack the first endpoint of the tunnel.
	//Let's see where we get from here!
	*tunnel_index_inout = 0;
	return true;
}

static rm_bool rm_tristripper_check_tunnel_for_circles(rm_tristripper_tri** tunnel, rm_size tunnel_index, rm_size* last_tunnel_circle_index)
{
	//Iterate over the now black triangles of the tunnel.
	//They are located at the even indices.

	for (rm_size i = 0; i < tunnel_index; i += 2)
	{
		//Get the current triangle:
		rm_tristripper_tri* starter_tri = tunnel[i];

		//If this triangle is not visited anymore, we have already been here in an earlier iteration.
		//Because we have not found a circle there, we won't find one over here and can continue.
		if (!rm_tristripper_tri_is_visited(starter_tri))
		{
			continue;
		}

		//Don't unvisit it at this point, we do that in the end!

		//Move to its successor in the tunnel. Both of them form a "strong" edge (= belong to the same tristrip now).
		rm_size next_tri_index_from_starter_tri = rm_tristripper_tri_get_tunnel_successor_index(starter_tri);

		//Remember its index in the tunnel:
		rm_tristripper_tri* next_tri = starter_tri->neighbours[next_tri_index_from_starter_tri];
		rm_assert(next_tri == tunnel[i + 1], "Tunnel successor is not equal.");
		rm_size max_tunnel_index = next_tri->tunnel_index;

		//This one can already be unvisited:
		rm_tristripper_tri_set_unvisited(next_tri);

		//Now traverse the whole tristrip in this direction.
		//Remember tunnel indices if we meet other tunnel members.
		rm_size curr_index_to_prev = (rm_size)starter_tri->indices_at_neighbours[next_tri_index_from_starter_tri];

		while (rm_tristripper_traverse_strip(&next_tri, &curr_index_to_prev))
		{
			//Is this a tunnel member?
			//Otherwise, it is not interesting.
			if (rm_likely(!rm_tristripper_tri_is_visited(next_tri)))
			{
				continue;
			}

			//Is this the triangle where we have started the strip exploration?
			//In that case, we have found a circle :(
			if (next_tri == starter_tri)
			{
				*last_tunnel_circle_index = max_tunnel_index;
				return true;
			}

			//Okay, just some other random tunnel member.
			//Update the maximum index if necessary.
			max_tunnel_index = rm_max(max_tunnel_index, (rm_size)next_tri->tunnel_index);

			//Unvisit it so we don't check it a second time:
			rm_tristripper_tri_set_unvisited(next_tri);
		}

		//We have reached an endpoint without meeting "starter_tri" again -> no tunnel :)
		//Mark our starter as non-visited (we haven't done that before to simplify the loop branch).
		rm_tristripper_tri_set_unvisited(starter_tri);
	}

	//Great, no tunnel has been found :)
	//By now, no triangle is marked as visited anymore.
	return false;
}

static rm_tristripper_cement_tunnel_result rm_tristripper_cement_tunnel(rm_tristripper_tri** tunnel, rm_size* tunnel_index_inout)
{
	//Get the current tunnel index from the inout variable:
	rm_size tunnel_index = *tunnel_index_inout;

	//Get the endpoints:
	rm_tristripper_tri* first_endpoint = tunnel[0];
	rm_tristripper_tri* second_endpoint = tunnel[tunnel_index];

	//Basic checks for tunnel endpoints:
	rm_assert(rm_tristripper_tri_is_endpoint(first_endpoint), "Tunnels must start with a red endpoint.");
	rm_assert(((tunnel_index % 2) != 0) && rm_tristripper_tri_is_endpoint(second_endpoint), "Tunnels must be cemented with a black, second endpoint.");

	//Remember if the endpoints are isolated endpoints before being tunneled:
	rm_bool first_endpoint_is_isolated = rm_tristripper_tri_is_isolated(first_endpoint);
	rm_bool second_endpoint_is_isolated = rm_tristripper_tri_is_isolated(second_endpoint);

	//Move along the tunnel to save all link states and flip the edges:
	rm_tristripper_tri_save_link_state(first_endpoint);

	for (rm_size i = 0; i < tunnel_index; i++)
	{
		//Get the current triangle and its color:
		rm_tristripper_tri* curr_tri = tunnel[i];
		rm_bool is_curr_tri_red = ((i % 2) == 0);

		//Get the indices in both directions:
		rm_size next_tri_index_from_curr_tri = rm_tristripper_tri_get_tunnel_successor_index(curr_tri);
		rm_size curr_tri_index_from_next_tri = (rm_size)curr_tri->indices_at_neighbours[next_tri_index_from_curr_tri];

		//Get the successor:
		rm_tristripper_tri* next_tri = tunnel[i + 1];
		rm_assert(next_tri == curr_tri->neighbours[next_tri_index_from_curr_tri], "Tunnel successor is not equal.");

		//Save the link state of the successor (ours is already safe):
		rm_tristripper_tri_save_link_state(next_tri);

		//Flip the edge from both directions:
		if (is_curr_tri_red)
		{
			rm_tristripper_tri_link_to_neighbour(curr_tri, next_tri_index_from_curr_tri);
			rm_tristripper_tri_link_to_neighbour(next_tri, curr_tri_index_from_next_tri);
		}
		else
		{
			rm_tristripper_tri_unlink_from_neighbour(curr_tri, next_tri_index_from_curr_tri);
			rm_tristripper_tri_unlink_from_neighbour(next_tri, curr_tri_index_from_next_tri);
		}
	}

	//Remove the endpoint markers from start and end.
	//That's important because otherwise the circle checker might back off too early.
	//But make sure they are not isloated!
	if (!first_endpoint_is_isolated)
	{
		rm_tristripper_tri_set_non_endpoint(first_endpoint);
	}

	if (!second_endpoint_is_isolated)
	{
		rm_tristripper_tri_set_non_endpoint(second_endpoint);
	}

	//Okay, the tunnel has been cemented - but we still have to look for circles.
	//If we find one, the cementation has to be undone :/
	rm_size last_tunnel_circle_index = 0;

	if (!rm_tristripper_check_tunnel_for_circles(tunnel, tunnel_index, &last_tunnel_circle_index))
	{
		//Nice, there are no circles :)
		//The check has also cleaned all of the "visited" flags, so we are completely done here.
		return RM_TRISTRIPPER_CEMENT_TUNNEL_RESULT_SUCCESS;
	}

	//Oh no, we have found a circle. Undo all the edge flipping and mark everything as visited again.
	for (rm_size i = 0; i <= tunnel_index; i++)
	{
		rm_tristripper_tri* curr_tri = tunnel[i];

		rm_tristripper_tri_set_visited(curr_tri, i);
		rm_tristripper_tri_restore_link_state(curr_tri);
	}

	//Restore the endpoint flags:
	rm_tristripper_tri_set_endpoint(first_endpoint);
	rm_tristripper_tri_set_endpoint(second_endpoint);

	//We know the index of the last triangle that is part of circle and tunnel.
	//If that is also the last triangle of the tunnel (our second endpoint), we don't have to backtrack.
	//Just try to open that endpoint and continue with it.
	if (last_tunnel_circle_index == tunnel_index)
	{
		return RM_TRISTRIPPER_CEMENT_TUNNEL_RESULT_FAIL_SHOULD_OPEN;
	}

	//The fault is located before the end. Try to backtrack down there:
	if (rm_tristripper_backtrack_tunnel(tunnel, tunnel_index_inout, last_tunnel_circle_index))
	{
		return RM_TRISTRIPPER_CEMENT_TUNNEL_RESULT_FAIL_BACKTRACKED;
	}
	else
	{
		return RM_TRISTRIPPER_CEMENT_TUNNEL_RESULT_FAIL_FOR_GOOD;
	}
}

rm_void rm_tristripper_create_strips_ex(rm_tristripper_tri* tris, rm_size tris_count, rm_tristripper_config* config, rm_tristripper_strip** strips, rm_size* strips_count)
{
	//Validate the parameters:
	rm_assert(tris, "Passed triangles must be valid.");
	rm_assert(tris_count > 0, "Number of passed triangles must be > 0.");
	rm_assert(config, "Passed config must be valid.");
	rm_assert(strips, "Passed strip outpointer must be valid.");
	rm_assert(strips_count, "Passed strip count outpointer must be valid.");

	//Collect endpoints into a linked list:
	rm_tristripper_tri* tris_endpoint_list = null;

	//Dispatch to the correct preprocessing algorithm:
	switch (config->preproc_algorithm)
	{
	case RM_TRISTRIPPER_PREPROC_ALGORITHM_ISOLATED:

		*strips_count = rm_tristripper_create_strips_ex_isolated(tris, tris_count, &tris_endpoint_list);
		break;

	case RM_TRISTRIPPER_PREPROC_ALGORITHM_PAIRS:

		*strips_count = rm_tristripper_create_strips_ex_pairs(tris, tris_count, &tris_endpoint_list);
		break;

	case RM_TRISTRIPPER_PREPROC_ALGORITHM_STRIPIFY:

		*strips_count = rm_tristripper_create_strips_ex_stripify(tris, tris_count, &tris_endpoint_list);
		break;

	default:

		rm_exit("Invalid preprocessing algorithm for tunneling.");
	}

	//Pass the endpoints down and create strips.
	//Note: "strips_count" is an inout parameter!
	//We have initialized it with the number of strips the preprocessing algorithm has created.
	//Tunneling might (and hopefully will) reduce that number.
	rm_tristripper_tri_create_strips_from_endpoints(&tris_endpoint_list, config, strips, strips_count);
}
