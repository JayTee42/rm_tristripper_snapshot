#include "rm_tristripper_tri.h"

#include "rm_hashmap.h"
#include "rm_mem.h"

//An edge key is formed by two vertices that form an edge.
//The vertex indices are stuffed in one numeric value twice the size (lower one to the lower bits).
typedef rm_uint64 rm_tristripper_edge_key;

//A triangle pointer in combination with an edge index (0, 1 or 2) that represents a free neighbour position.
typedef struct __rm_tristripper_open_edge__
{
	rm_tristripper_tri* tri;
	rm_uint8 edge_index;
} rm_tristripper_open_edge;

//We need a hashmap monomorphization that maps edge keys to open edges.
//It will be used to stitch triangles to their neighbours.
RM_HASHMAP_DECLARE(tristripper_open_edge, rm_tristripper_edge_key, rm_tristripper_open_edge, SKV)
#define RM_TRISTRIPPER_OPEN_EDGE_HASHMAP_LOAD_FACTOR 0.75

//Hashing and comparing for our hashmap:
static inline rm_hashmap_hash rm_tristripper_open_edge_hashmap_hash(rm_tristripper_edge_key key);
static inline rm_bool rm_tristripper_open_edge_hashmap_compare(rm_tristripper_edge_key key0, rm_tristripper_edge_key key1);

//Build a hashmap key from two tristripper IDs:
static inline rm_tristripper_edge_key rm_tristripper_open_edge_hashmap_make_key(rm_tristripper_id v0, rm_tristripper_id v1);

static inline rm_hashmap_hash rm_tristripper_open_edge_hashmap_hash(rm_tristripper_edge_key key)
{
	//Use the key itself as hash:
	return (rm_hashmap_hash)key;
}

static inline rm_bool rm_tristripper_open_edge_hashmap_compare(rm_tristripper_edge_key key0, rm_tristripper_edge_key key1)
{
	//Simple integer equality:
	return (key0 == key1);
}

static inline rm_tristripper_edge_key rm_tristripper_open_edge_hashmap_make_key(rm_tristripper_id v0, rm_tristripper_id v1)
{
	rm_tristripper_id lower, upper;

	//The indices must be ordered first:
	if (v0 < v1)
	{
		lower = v0;
		upper = v1;
	}
	else
	{
		lower = v1;
		upper = v0;
	}

	return (((rm_tristripper_edge_key)upper) << (sizeof(rm_tristripper_id) * 8)) | ((rm_tristripper_edge_key)lower);
}

//Spawn the implementation of all the hashmap functions:
RM_HASHMAP_DEFINE(tristripper_open_edge, rm_tristripper_open_edge_hashmap_hash, rm_tristripper_open_edge_hashmap_compare, null, null, null, null, null)

//Emit non-inline versions:
extern rm_bool rm_tristripper_tri_is_stripped(const rm_tristripper_tri* tri);
extern rm_void rm_tristripper_tri_set_stripped(rm_tristripper_tri* tri);
extern rm_void rm_tristripper_tri_set_stripped_and_propagate(rm_tristripper_tri* tri, rm_tristripper_tri** tris_adjacency_lists);
extern rm_bool rm_tristripper_tri_is_endpoint(const rm_tristripper_tri* tri);
extern rm_void rm_tristripper_tri_set_endpoint(rm_tristripper_tri* tri);
extern rm_void rm_tristripper_tri_set_non_endpoint(rm_tristripper_tri* tri);
extern rm_bool rm_tristripper_tri_is_visited(const rm_tristripper_tri* tri);
extern rm_void rm_tristripper_tri_set_visited(rm_tristripper_tri* tri, rm_size tunnel_index);
extern rm_void rm_tristripper_tri_set_unvisited(rm_tristripper_tri* tri);
extern rm_bool rm_tristripper_tri_is_linked_to_neighbour(const rm_tristripper_tri* tri, rm_size neighbour_index);
extern rm_bool rm_tristripper_tri_is_isolated(const rm_tristripper_tri* tri);
extern rm_void rm_tristripper_tri_link_to_neighbour(rm_tristripper_tri* tri, rm_size neighbour_index);
extern rm_void rm_tristripper_tri_unlink_from_neighbour(rm_tristripper_tri* tri, rm_size neighbour_index);
extern rm_void rm_tristripper_tri_save_link_state(rm_tristripper_tri* tri);
extern rm_void rm_tristripper_tri_restore_link_state(rm_tristripper_tri* tri);
extern rm_void rm_tristripper_tri_init_tunnel_state(rm_tristripper_tri* tri);
extern rm_bool rm_tristripper_tri_is_tunnel_state_depleted(const rm_tristripper_tri* tri);
extern rm_void rm_tristripper_tri_add_tunnel_state(rm_tristripper_tri* tri, rm_size neighbour_index);
extern rm_bool rm_tristripper_tri_select_next_tunnel_state(rm_tristripper_tri* tri);
extern rm_size rm_tristripper_tri_get_tunnel_successor_index(rm_tristripper_tri* tri);
extern rm_void rm_tristripper_tri_prepend_to_list(rm_tristripper_tri* tri, rm_tristripper_tri** head);
extern rm_void rm_tristripper_tri_remove_from_list(const rm_tristripper_tri* tri, rm_tristripper_tri** head);
extern rm_void rm_tristripper_order_tris(rm_tristripper_tri* tris, rm_size tris_count, rm_tristripper_tri** tris_adjacency_lists);
extern rm_void rm_tristripper_determine_core_entrance_vertex_ids(const rm_tristripper_id* first_shared_edge, const rm_tristripper_id* second_shared_edge, rm_tristripper_id* core_entrance_vertix_ids);;

rm_void rm_tristripper_build_tris(const rm_tristripper_id* ids, rm_size ids_count, rm_tristripper_tri** tris, rm_size* tris_count)
{
	//Make sure we don't get rubbish as input:
	rm_precond((ids_count % 3) == 0, "Number of vertex IDs must be divisible by 3.");

	//Create a hashmap for the open edges:
	rm_tristripper_open_edge_hashmap open_edges;

	//Initialize it with a (hopefully) sufficient bucket count:
    rm_size bucket_count = rm_hashmap_get_sufficient_bucket_count(ids_count, RM_TRISTRIPPER_OPEN_EDGE_HASHMAP_LOAD_FACTOR);
    rm_tristripper_open_edge_hashmap_init_ex(&open_edges, bucket_count, RM_TRISTRIPPER_OPEN_EDGE_HASHMAP_LOAD_FACTOR);

	//Allocate memory for the triangles.
	//We allocate the maximal amount and expect no triangles to be degenerated.
	//If there are actually some of them, there will be unused, "overhanging" memory.
	rm_size expected_tris_count = ids_count / 3;
    rm_size result_tris_count = 0;

	rm_tristripper_tri* result_tris = rm_malloc(expected_tris_count * sizeof(rm_tristripper_tri));

	//Iterate over all of them:
	for (rm_size i = 0; i < expected_tris_count; i++)
	{
		//Get the current triangle:
		rm_tristripper_tri* tri = &result_tris[result_tris_count];

		//Start without any neighbours:
		tri->neighbours[0] = null;
		tri->neighbours[1] = null;
		tri->neighbours[2] = null;

		tri->unstripped_neighbours_count = 0;

		//Null all the flags (but only the necessary stuff):
		tri->flags = 0;
		tri->link_state = 0;

		//Insert the vertices (keep the winding order):
		tri->vertices[0] = ids[3 * i];
		tri->vertices[1] = ids[(3 * i) + 1];
		tri->vertices[2] = ids[(3 * i) + 2];

		//Ignore degenerated triangles:
		if (rm_unlikely((tri->vertices[0] == tri->vertices[1]) || (tri->vertices[1] == tri->vertices[2]) || (tri->vertices[2] == tri->vertices[0])))
		{
			continue;
		}

		//Increment the actual triangle count:
		result_tris_count++;

		//Build the three edge keys:
		rm_tristripper_edge_key edge_keys[3] =
		{
			rm_tristripper_open_edge_hashmap_make_key(tri->vertices[0], tri->vertices[1]),
			rm_tristripper_open_edge_hashmap_make_key(tri->vertices[1], tri->vertices[2]),
			rm_tristripper_open_edge_hashmap_make_key(tri->vertices[2], tri->vertices[0])
		};

		//Iterate through the three edges:
		for (rm_size j = 0; j < rm_array_count(edge_keys); j++)
		{
			rm_tristripper_edge_key curr_edge_key = edge_keys[j];

			//Create an open edge struct:
			rm_tristripper_open_edge new_open_edge =
			{
				.tri = tri,
				.edge_index = (rm_uint8)j
			};

			//Try to insert the open edge into the hashmap.
			//If the spot is already occupied, we *instead* retrieve the existing open edge.
			rm_tristripper_open_edge old_open_edge;

			if (rm_tristripper_open_edge_hashmap_update(&open_edges, curr_edge_key, new_open_edge, RM_HASHMAP_UPDATE_MODE_INSERT, &old_open_edge))
			{
				//Get our neighbour triangle:
				rm_tristripper_tri* neighbour = old_open_edge.tri;

				//Stich the two triangles together:
				tri->neighbours[j] = neighbour;
				tri->indices_at_neighbours[j] = old_open_edge.edge_index;
				tri->unstripped_neighbours_count++;

				neighbour->neighbours[(rm_size)old_open_edge.edge_index] = tri;
				neighbour->indices_at_neighbours[(rm_size)old_open_edge.edge_index] = (rm_uint8)j;
				neighbour->unstripped_neighbours_count++;

				//Remove the open edge from the hashmap.
				//This allows more than two triangles to share an edge.
				//Example: If triangles A, B, C, and D share an edge and are inserted in that order,
				// (A, B) and (C, D) will become neighbours without interference.
				//Of course, this should only be a three-dimensional issue ...
				rm_tristripper_open_edge_hashmap_remove(&open_edges, curr_edge_key);
			}
		}
	}

	//Dispose the hashmap:
	rm_tristripper_open_edge_hashmap_dispose(&open_edges);

	//Assign the result:
	*tris = result_tris;
	*tris_count = result_tris_count;
}

rm_tristripper_tri* rm_tristripper_select_next_core_tri(rm_tristripper_tri* tri, rm_tristripper_tri** tris_adjacency_lists, rm_tristripper_id* shared_edge, rm_size* index_from_tri)
{
	//Start with "not found".
	rm_tristripper_tri* best_neighbour = null;
	rm_size best_neighbour_index;

	for (rm_size i = 0; i < rm_array_count(tri->neighbours); i++)
	{
		rm_tristripper_tri* curr_neighbour = tri->neighbours[i];

		//The neighbour must exist and we must not have stripped it yet.
		//Also make sure it is better than our most promising candidate.
		if (!curr_neighbour || rm_tristripper_tri_is_stripped(curr_neighbour))
		{
			continue;
		}

		//Check if we already have a candidate with a better (= lower) unstripped neighbours count:
		if (!best_neighbour || (curr_neighbour->unstripped_neighbours_count < best_neighbour->unstripped_neighbours_count))
		{
			//We have found no such neighbour, this is the new best one :)
			best_neighbour = curr_neighbour;
			best_neighbour_index = i;
		}
	}

	if (best_neighbour)
	{
		//Store the output parameters:
		shared_edge[0] = tri->vertices[best_neighbour_index];
		shared_edge[1] = tri->vertices[(best_neighbour_index + 1) % 3];

		*index_from_tri = best_neighbour_index;

		//Mark the new neighbour as stripped:
		rm_tristripper_tri_set_stripped_and_propagate(best_neighbour, tris_adjacency_lists);
	}

	return best_neighbour;
}
