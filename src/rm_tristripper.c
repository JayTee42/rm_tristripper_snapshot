#include "rm_tristripper.h"

#include "rm_tristripper_tri.h"
#include "rm_tristripper_simple.h"
#include "rm_tristripper_ex.h"

rm_void rm_tristripper_create_strips(const rm_tristripper_id* ids, rm_size ids_count, rm_tristripper_config* config, rm_tristripper_strip** strips, rm_size* strips_count)
{
	//Validate the output parameters:
	rm_assert(strips, "Passed strip outpointer must be valid.");
	rm_assert(strips_count, "Passed strip count outpointer must be valid.");

	//Catch the trivial cases:
	if (ids_count < 3)
	{
		*strips = null;
		*strips_count = 0;

		return;
	}

	//Validate the input parameters:
	rm_precond(ids, "Passed IDs must be valid.");
	rm_precond(config, "Passed config must be valid.");

	//Build triangles from the given ids:
	rm_tristripper_tri* tris;
	rm_size tris_count;

	rm_tristripper_build_tris(ids, ids_count, &tris, &tris_count);

	//Are there triangles at all?
	if (tris_count > 0)
	{
		//Tunneling or stripify-only?
		if (config->use_tunneling)
		{
			//We want to perform tunneling.
			//The maximum size of a tunnel should be limited to the maximum number of triangles (except that is below 2 or above UINT16_MAX).
			//It is also a stupid idea to use an odd value because all tunnels need an even count.
			//So let's perform truncation here.
			config->max_count = rm_min(rm_min(config->max_count, tris_count), (rm_size)UINT16_MAX);
			config->max_count = rm_max((config->max_count / 2) * 2, (rm_size)2);

			//Apply the extended "tunneling" algorithm:
			rm_tristripper_create_strips_ex(tris, tris_count, config, strips, strips_count);
		}
		else
		{
			//Apply the simple "stripify" algorithm:
			rm_tristripper_create_strips_simple(tris, tris_count, config->preserve_orientation, strips, strips_count);
		}
	}
	else
	{
		*strips = null;
		*strips_count = 0;
	}

	//Free the triangles:
	rm_free(tris);
}

rm_void rm_tristripper_dispose_strips(const rm_tristripper_strip* strips, rm_size strips_count)
{
	//Free each single strip's buffer:
	for (rm_size i = 0; i < strips_count; i++)
	{
		rm_free(strips[i].ids);
	}

	//Free the array itself:
	rm_free(strips);
}
