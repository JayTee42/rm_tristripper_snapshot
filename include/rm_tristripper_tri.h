#ifndef __RM_TRISTRIPPER_TRI_H__
#define __RM_TRISTRIPPER_TRI_H__

#include "rm_assert.h"
#include "rm_macro.h"

#include "rm_tristripper_common.h"

//Triangle flags:
typedef rm_uint8 rm_tristripper_tri_flags;

//Indicate if a triangle has already been stripped:
#define RM_TRISTRIPPER_TRI_FLAG_IS_STRIPPED (((rm_tristripper_tri_flags)1) << 0)

//Indicate if a triangle is an end point:
#define RM_TRISTRIPPER_TRI_FLAG_IS_ENDPOINT (((rm_tristripper_tri_flags)1) << 1)

//Indicate if a triangle has been visited:
#define RM_TRISTRIPPER_TRI_FLAG_IS_VISITED (((rm_tristripper_tri_flags)1) << 2)

//Link state denotes the topology of a triangle in relation to the strip it is part of.
//We plug up to three versions of the state into a single struct field to enable a simple backup-restore stack.
typedef rm_uint8 rm_tristripper_tri_link_state;

//Tunnel state denotes the index of the next tunnel triangle in our array of neighbours.
//To allow easy backtracking, we plug up to three potential indices into one byte.
typedef rm_uint8 rm_tristripper_tri_tunnel_state;

//This value acts as a sentinel for tunnel states:
#define RM_TRISTRIPPER_TRI_TUNNEL_STATE_DEPLETED ((rm_tristripper_tri_tunnel_state)3)

//Note: This struct is reordered for minimum size and not for readability :/
typedef struct __rm_tristripper_tri__
{
	/*
		Pointers to the neighbour triangles.
		Order: (01), (12), (20) for vertices 0, 1 and 2:

		       0
		      / \
		   0 /   \ 2
		    /     \
		   1 ----- 2
		       1

		As a consequence, there might be empty spots for missing neighbours in the array.
		Those are guaranteed to be nullpointers.
	*/
	struct __rm_tristripper_tri__* neighbours[3];

	//This doubly-linked list of triangles will serve two distinct purposes in the lifetime of a triangle:
	// 1.) All triangles with the same number of neighbours (0, 1, 2, or 3) form a list.
	//     This mechanism allows us to always select a triangle with minimum neighbour count as starter of a new stirp.
	// 2.) While tunneling, we use this list to link all the endpoint tris together.
	struct __rm_tristripper_tri__* prev_tri;
	struct __rm_tristripper_tri__* next_tri;

	//The vertices 0, 1, and 2:
	rm_tristripper_id vertices[3];

	//The index of the triangle in a potential tunnel (only valid if the "visited" flag has been set):
	rm_uint16 tunnel_index;

	//Entry i in this array represents our index in the neighbours array of our neighbour[i].
	//This helps us to save some instructions as we move from one triangle to the next:
	//Often, we can omit the neighbour at that index because it only points back.
	//Note: These indices stay forever 0 if the corresponding neighbour is not present.
	rm_uint8 indices_at_neighbours[3];

	/*
		The flags:

		-------------------------------------------------
		|  ?  |  ?  |  ?  |  ?  |  ?  | vis | end | str |
		-------------------------------------------------
	*/
	rm_tristripper_tri_flags flags;

	/*
		The link state:

		-------------------------------------------------
		|  ?  |  ?  | bl2 | bl1 | bl0 |  l2 |  l1 |  l0 |
		-------------------------------------------------
	*/
	rm_tristripper_tri_link_state link_state;

	//Unstripped neighbour count and tunnel state are never used together.
	//Let's put them into an anonymous union and save one byte. Yay :)
	union
	{
		/*
			The tunnel state:

			-------------------------------------------------
			|  sentinel |  next_ti2 |  next_ti1 |  next_ti0 |
			-------------------------------------------------
		*/
		rm_tristripper_tri_tunnel_state tunnel_state;

		//The number of unstripped neighbours:
		rm_uint8 unstripped_neighbours_count;
	};
} rm_tristripper_tri;

//Manage the "IS_STRIPPED" flag:
inline rm_bool rm_tristripper_tri_is_stripped(const rm_tristripper_tri* tri);
inline rm_void rm_tristripper_tri_set_stripped(rm_tristripper_tri* tri);
inline rm_void rm_tristripper_tri_set_stripped_and_propagate(rm_tristripper_tri* tri, rm_tristripper_tri** tris_adjacency_lists);

//Manage the "IS_ENDPOINT" flag:
inline rm_bool rm_tristripper_tri_is_endpoint(const rm_tristripper_tri* tri);
inline rm_void rm_tristripper_tri_set_endpoint(rm_tristripper_tri* tri);
inline rm_void rm_tristripper_tri_set_non_endpoint(rm_tristripper_tri* tri);

//Manage the "IS_VISIBLE" flag:
inline rm_bool rm_tristripper_tri_is_visited(const rm_tristripper_tri* tri);
inline rm_void rm_tristripper_tri_set_visited(rm_tristripper_tri* tri, rm_size tunnel_index);
inline rm_void rm_tristripper_tri_set_unvisited(rm_tristripper_tri* tri);

//Manage the link state:
inline rm_bool rm_tristripper_tri_is_linked_to_neighbour(const rm_tristripper_tri* tri, rm_size neighbour_index);
inline rm_bool rm_tristripper_tri_is_isolated(const rm_tristripper_tri* tri);
inline rm_void rm_tristripper_tri_link_to_neighbour(rm_tristripper_tri* tri, rm_size neighbour_index);
inline rm_void rm_tristripper_tri_unlink_from_neighbour(rm_tristripper_tri* tri, rm_size neighbour_index);
inline rm_void rm_tristripper_tri_save_link_state(rm_tristripper_tri* tri);
inline rm_void rm_tristripper_tri_restore_link_state(rm_tristripper_tri* tri);

//Manage the tunnel state:
inline rm_void rm_tristripper_tri_init_tunnel_state(rm_tristripper_tri* tri);
inline rm_bool rm_tristripper_tri_is_tunnel_state_depleted(const rm_tristripper_tri* tri);
inline rm_void rm_tristripper_tri_add_tunnel_state(rm_tristripper_tri* tri, rm_size neighbour_index);
inline rm_bool rm_tristripper_tri_select_next_tunnel_state(rm_tristripper_tri* tri);
inline rm_size rm_tristripper_tri_get_tunnel_successor_index(rm_tristripper_tri* tri);

//Manage the doubly-linked list membership:
inline rm_void rm_tristripper_tri_prepend_to_list(rm_tristripper_tri* tri, rm_tristripper_tri** head);
inline rm_void rm_tristripper_tri_remove_from_list(const rm_tristripper_tri* tri, rm_tristripper_tri** head);

//Sort the given triangles into the adjacency lists, depending on their neighbour count (0, 1, 2, or 3 neighbours):
inline rm_void rm_tristripper_order_tris(rm_tristripper_tri* tris, rm_size tris_count, rm_tristripper_tri** tris_adjacency_lists);

//Look at the two edges between the three core triangles.
//Derive the entrance vertex IDs for all of them.
inline rm_void rm_tristripper_determine_core_entrance_vertex_ids(const rm_tristripper_id* first_shared_edge, const rm_tristripper_id* second_shared_edge, rm_tristripper_id* core_entrance_vertix_ids);

//Take the given IDs, build triangles from them and assign their neighbour pointers.
//Preserve the winding order for all triangles.
rm_void rm_tristripper_build_tris(const rm_tristripper_id* ids, rm_size ids_count, rm_tristripper_tri** tris, rm_size* tris_count);

//This function is used to select the second and third core triangles.
//Also return the shared edge and the index of the new triangle as seen from "tri".
rm_tristripper_tri* rm_tristripper_select_next_core_tri(rm_tristripper_tri* tri, rm_tristripper_tri** tris_adjacency_lists, rm_tristripper_id* shared_edge, rm_size* index_from_tri);

inline rm_bool rm_tristripper_tri_is_stripped(const rm_tristripper_tri* tri)
{
	return (tri->flags & RM_TRISTRIPPER_TRI_FLAG_IS_STRIPPED);
}

inline rm_void rm_tristripper_tri_set_stripped(rm_tristripper_tri* tri)
{
	tri->flags |= RM_TRISTRIPPER_TRI_FLAG_IS_STRIPPED;
}

inline rm_void rm_tristripper_tri_set_stripped_and_propagate(rm_tristripper_tri* tri, rm_tristripper_tri** tris_adjacency_lists)
{
	//Set the flag and remove the triangle from its current doubly-linked list, we don't have to visit it again:
	rm_tristripper_tri_set_stripped(tri);
	rm_tristripper_tri_remove_from_list(tri, &tris_adjacency_lists[(rm_size)tri->unstripped_neighbours_count]);

	//Visit those neighbours that are not yet stripped:
	for (rm_size i = 0; i < rm_array_count(tri->neighbours); i++)
	{
		rm_tristripper_tri* neighbour = tri->neighbours[i];

		if (!neighbour || rm_tristripper_tri_is_stripped(neighbour))
		{
			continue;
		}

		//Remove it from its current doubly-linked list:
		rm_tristripper_tri_remove_from_list(neighbour, &tris_adjacency_lists[(rm_size)neighbour->unstripped_neighbours_count]);

		//Decrement the neighbours count of the neighbour and prepend it to the correct list:
		neighbour->unstripped_neighbours_count--;
		rm_tristripper_tri_prepend_to_list(neighbour, &tris_adjacency_lists[(rm_size)neighbour->unstripped_neighbours_count]);
	}
}

inline rm_bool rm_tristripper_tri_is_endpoint(const rm_tristripper_tri* tri)
{
	return (tri->flags & RM_TRISTRIPPER_TRI_FLAG_IS_ENDPOINT);
}

inline rm_void rm_tristripper_tri_set_endpoint(rm_tristripper_tri* tri)
{
	tri->flags |= RM_TRISTRIPPER_TRI_FLAG_IS_ENDPOINT;
}

inline rm_void rm_tristripper_tri_set_non_endpoint(rm_tristripper_tri* tri)
{
	tri->flags &= (rm_tristripper_tri_flags)~RM_TRISTRIPPER_TRI_FLAG_IS_ENDPOINT;
}

inline rm_bool rm_tristripper_tri_is_visited(const rm_tristripper_tri* tri)
{
	return (tri->flags & RM_TRISTRIPPER_TRI_FLAG_IS_VISITED);
}

inline rm_void rm_tristripper_tri_set_visited(rm_tristripper_tri* tri, rm_size tunnel_index)
{
	rm_assert(tunnel_index <= (rm_size)UINT16_MAX, "Tunnel index must be below 2^16.");

	tri->flags |= RM_TRISTRIPPER_TRI_FLAG_IS_VISITED;
	tri->tunnel_index = (rm_uint16)tunnel_index;
}

inline rm_void rm_tristripper_tri_set_unvisited(rm_tristripper_tri* tri)
{
	tri->flags &= (rm_tristripper_tri_flags)~RM_TRISTRIPPER_TRI_FLAG_IS_VISITED;
}

inline rm_bool rm_tristripper_tri_is_linked_to_neighbour(const rm_tristripper_tri* tri, rm_size neighbour_index)
{
	rm_assert(neighbour_index < 3, "Neighbour index is limited to 0...2.");
	return (tri->link_state & (((rm_tristripper_tri_link_state)1) << neighbour_index));
}

inline rm_bool rm_tristripper_tri_is_isolated(const rm_tristripper_tri* tri)
{
	//An isolated triangle is incident to three "weak" edges.
	//It forms a tristrip on its own and is, by definition, an endpoint.
	//That's special because it will still be an endpoint after being tunneled once.
	rm_assert(rm_tristripper_tri_is_endpoint(tri), "Only endpoints can be isolated.");
	return (tri->link_state & 7) == 0;
}

inline rm_void rm_tristripper_tri_link_to_neighbour(rm_tristripper_tri* tri, rm_size neighbour_index)
{
	rm_assert(neighbour_index < 3, "Neighbour index is limited to 0...2.");
	tri->link_state |= (rm_tristripper_tri_link_state)(((rm_tristripper_tri_link_state)1) << neighbour_index);
}

inline rm_void rm_tristripper_tri_unlink_from_neighbour(rm_tristripper_tri* tri, rm_size neighbour_index)
{
	rm_assert(neighbour_index < 3, "Neighbour index is limited to 0...2.");
	tri->link_state &= (rm_tristripper_tri_link_state)~(((rm_tristripper_tri_link_state)1) << neighbour_index);
}

inline rm_void rm_tristripper_tri_save_link_state(rm_tristripper_tri* tri)
{
	tri->link_state = (rm_tristripper_tri_link_state)((tri->link_state << 3) | (tri->link_state & 7));
}

inline rm_void rm_tristripper_tri_restore_link_state(rm_tristripper_tri* tri)
{
	tri->link_state >>= 3;
}

inline rm_void rm_tristripper_tri_init_tunnel_state(rm_tristripper_tri* tri)
{
	//Init the tunnel state with the sentinel:
	tri->tunnel_state = RM_TRISTRIPPER_TRI_TUNNEL_STATE_DEPLETED;
}

inline rm_void rm_tristripper_tri_add_tunnel_state(rm_tristripper_tri* tri, rm_size neighbour_index)
{
	rm_assert(neighbour_index < 3, "Neighbour index is limited to 0...2.");

	//Append the state:
	tri->tunnel_state = (rm_tristripper_tri_tunnel_state)((tri->tunnel_state << 2) | ((rm_tristripper_tri_tunnel_state)neighbour_index));
}

inline rm_bool rm_tristripper_tri_is_tunnel_state_depleted(const rm_tristripper_tri* tri)
{
	return (tri->tunnel_state == RM_TRISTRIPPER_TRI_TUNNEL_STATE_DEPLETED);
}

inline rm_bool rm_tristripper_tri_select_next_tunnel_state(rm_tristripper_tri* tri)
{
	//Move to the next tunnel state:
	tri->tunnel_state >>= 2;

	//Are there no more tunnel states?
	if (rm_tristripper_tri_is_tunnel_state_depleted(tri))
	{
		return false;
	}

	return true;
}

inline rm_size rm_tristripper_tri_get_tunnel_successor_index(rm_tristripper_tri* tri)
{
	return (rm_size)(tri->tunnel_state & 3);
}

inline rm_void rm_tristripper_tri_prepend_to_list(rm_tristripper_tri* tri, rm_tristripper_tri** head)
{
	tri->prev_tri = null;
	tri->next_tri = *head;

	if (rm_likely(tri->next_tri != null))
	{
		tri->next_tri->prev_tri = tri;
	}

	//Fix the list head:
	*head = tri;
}

inline rm_void rm_tristripper_tri_remove_from_list(const rm_tristripper_tri* tri, rm_tristripper_tri** head)
{
	if (rm_likely(tri->prev_tri != null))
	{
		//Adjust the pointer in the previous triangle:
		tri->prev_tri->next_tri = tri->next_tri;
	}
	else
	{
		//The triangle was the first one in its list.
		//Make sure to fix the list head!
		*head = tri->next_tri;
	}

	if (rm_likely(tri->next_tri != null))
	{
		//Adjust the pointer in the next triangle:
		tri->next_tri->prev_tri = tri->prev_tri;
	}
}

inline rm_void rm_tristripper_order_tris(rm_tristripper_tri* tris, rm_size tris_count, rm_tristripper_tri** tris_adjacency_lists)
{
	//Iterate over the triangles:
	for (rm_size i = 0; i < tris_count; i++)
	{
		//Get the current triangle:
		rm_tristripper_tri* tri = &tris[i];

		//Prepend the triangle to the corresponding list:
		rm_tristripper_tri_prepend_to_list(tri, &tris_adjacency_lists[(rm_size)tri->unstripped_neighbours_count]);
	}
}

inline rm_void rm_tristripper_determine_core_entrance_vertex_ids(const rm_tristripper_id* first_shared_edge, const rm_tristripper_id* second_shared_edge, rm_tristripper_id* core_entrance_vertix_ids)
{
	//The shared vertex is the entrance vertex for the second triangle.
	//The non-shared vertex in "first_shared_edge" is the entrance vertex for the first triangle.
	//The non-shared vertex in "second_shared_edge" is the entrance vertex for the third triangle.

	if (first_shared_edge[0] == second_shared_edge[0])
	{
		core_entrance_vertix_ids[0] = first_shared_edge[1];
		core_entrance_vertix_ids[1] = first_shared_edge[0];
		core_entrance_vertix_ids[2] = second_shared_edge[1];
	}
	else if (first_shared_edge[0] == second_shared_edge[1])
	{
		core_entrance_vertix_ids[0] = first_shared_edge[1];
		core_entrance_vertix_ids[1] = first_shared_edge[0];
		core_entrance_vertix_ids[2] = second_shared_edge[0];
	}
	else if (first_shared_edge[1] == second_shared_edge[0])
	{
		core_entrance_vertix_ids[0] = first_shared_edge[0];
		core_entrance_vertix_ids[1] = first_shared_edge[1];
		core_entrance_vertix_ids[2] = second_shared_edge[1];
	}
	else
	{
		rm_assert(first_shared_edge[1] == second_shared_edge[1], "First and second shared edge must have a common vertex.");

		core_entrance_vertix_ids[0] = first_shared_edge[0];
		core_entrance_vertix_ids[1] = first_shared_edge[1];
		core_entrance_vertix_ids[2] = second_shared_edge[0];
	}
}

#endif
