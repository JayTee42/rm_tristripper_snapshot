#include "rm_tristripper_verifier.h"

#include "rm_assert.h"
#include "rm_log.h"
#include "rm_macro.h"
#include "rm_mem.h"

//The load factor for our hashmap:
#define RM_TRISTRIPPER_TRI_OCCURRENCE_HASHMAP_LOAD_FACTOR 0.75

//Hashing, comparing and merging for our hashmap:
static inline rm_hashmap_hash rm_tristripper_tri_occurrence_hashmap_hash(rm_tristripper_tri_key key);
static inline rm_bool rm_tristripper_tri_occurrence_hashmap_compare(rm_tristripper_tri_key key1, rm_tristripper_tri_key key2);
static inline rm_tristripper_tri_occurrence rm_tristripper_tri_occurrence_hashmap_merge(rm_tristripper_tri_key key, rm_tristripper_tri_occurrence old_value, rm_tristripper_tri_occurrence new_value);

//Build a hashmap key from three tristripper IDs:
static inline rm_tristripper_tri_key rm_tristripper_tri_occurrence_hashmap_make_key(rm_tristripper_id v0, rm_tristripper_id v1, rm_tristripper_id v2);

//This is a helper function to log all the triangles that are missing from a strip:
static rm_bool rm_tristripper_verifier_log_missing_tris(const rm_tristripper_tri_occurrence_hashmap* occurrences, rm_tristripper_tri_key key, rm_tristripper_tri_occurrence occurrence, rm_void* context);

static inline rm_hashmap_hash rm_tristripper_tri_occurrence_hashmap_hash(rm_tristripper_tri_key key)
{
	//XOR them all together:
	return (rm_hashmap_hash)(key.vertex_ids[0] ^ key.vertex_ids[1] ^ key.vertex_ids[2]);
}

static inline rm_bool rm_tristripper_tri_occurrence_hashmap_compare(rm_tristripper_tri_key key0, rm_tristripper_tri_key key1)
{
	//Check all three components:
	return ((key0.vertex_ids[0] == key1.vertex_ids[0]) && (key0.vertex_ids[1] == key1.vertex_ids[1]) && (key0.vertex_ids[2] == key1.vertex_ids[2]));
}

static inline rm_tristripper_tri_occurrence rm_tristripper_tri_occurrence_hashmap_merge(rm_tristripper_tri_key key, rm_tristripper_tri_occurrence old_value, rm_tristripper_tri_occurrence new_value)
{
	rm_unused(key);
	rm_unused(new_value);

	//Just increment the multiplicity on the old value by one:
	old_value.multiplicity++;
	return old_value;
}

static inline rm_tristripper_tri_key rm_tristripper_tri_occurrence_hashmap_make_key(rm_tristripper_id v0, rm_tristripper_id v1, rm_tristripper_id v2)
{
	rm_tristripper_id first, second, third;

	//The indices must be ordered first:
	if (v0 < v1)
	{
		if (v0 < v2)
		{
			first = v0;

			if (v1 < v2)
			{
				second = v1; third = v2;
			}
			else
			{
				second = v2; third = v1;
			}
		}
		else
		{
			first = v2; second = v0; third = v1;
		}
	}
	else if (v1 < v2)
	{
		first = v1;

		if (v0 < v2)
		{
			second = v0; third = v2;
		}
		else
		{
			second = v2; third = v0;
		}
	}
	else
	{
		first = v2; second = v1; third = v0;
	}

	//Build the key from them:
	return (rm_tristripper_tri_key)
	{
		.vertex_ids = { first, second, third }
	};
}

static rm_bool rm_tristripper_verifier_log_missing_tris(const rm_tristripper_tri_occurrence_hashmap* occurrences, rm_tristripper_tri_key key, rm_tristripper_tri_occurrence occurrence, rm_void* context)
{
	rm_unused(occurrences);

	//Cast the context to our multiplicity list:
	rm_size* distinct_valid_tris_multiplicities = (rm_size*)context;

	//Get the multiplicity from the strip:
	rm_size strip_multiplicity = distinct_valid_tris_multiplicities[occurrence.index];

	//Compare it to the correct one:
	if (strip_multiplicity < occurrence.multiplicity)
	{
		if (strip_multiplicity == 0)
		{
			rm_log(RM_LOG_TYPE_WARNING, "Missing triangle: (%zu, %zu, %zu) should appear %zu time(s), but we did not encounter it at all.", (rm_size)key.vertex_ids[0], (rm_size)key.vertex_ids[1], (rm_size)key.vertex_ids[2], occurrence.multiplicity);
		}
		else
		{
			rm_log(RM_LOG_TYPE_WARNING, "Missing triangle: (%zu, %zu, %zu) should appear %zu time(s), but we only encountered it %zu time(s).", (rm_size)key.vertex_ids[0], (rm_size)key.vertex_ids[1], (rm_size)key.vertex_ids[2], occurrence.multiplicity, strip_multiplicity);
		}
	}

	return true;
}

//Spawn the implementation of all the hashmap functions:
RM_HASHMAP_DEFINE(tristripper_tri_occurrence, rm_tristripper_tri_occurrence_hashmap_hash, rm_tristripper_tri_occurrence_hashmap_compare, null, null, null, null, rm_tristripper_tri_occurrence_hashmap_merge)

rm_void rm_tristripper_init_verifier(rm_tristripper_verifier* verifier, rm_tristripper_id* ids, rm_size ids_count)
{
	//Make sure we don't get rubbish as input:
	rm_precond((ids_count % 3) == 0, "Number of vertex IDs must be divisible by 3.");

	//Initialize the hashmap with a (hopefully) sufficient bucket count:
    rm_size bucket_count = rm_hashmap_get_sufficient_bucket_count(ids_count, RM_TRISTRIPPER_TRI_OCCURRENCE_HASHMAP_LOAD_FACTOR);
    rm_tristripper_tri_occurrence_hashmap_init_ex(&verifier->occurrences, bucket_count, RM_TRISTRIPPER_TRI_OCCURRENCE_HASHMAP_LOAD_FACTOR);

    //Iterate over the triangles that are formed by the IDs:
    verifier->valid_tris_count = 0;
	verifier->distinct_valid_tris_count = 0;

	for (rm_size i = 0; i < ids_count; i += 3)
	{
		//Get the IDs for the current triangle:
		rm_tristripper_id curr_ids[] = { ids[i], ids[i + 1], ids[i + 2] };

		//Ignore degenerated triangles:
		if (rm_unlikely((curr_ids[0] == curr_ids[1]) || (curr_ids[1] == curr_ids[2]) || (curr_ids[2] == curr_ids[0])))
		{
			continue;
		}

		//Build a triangle key:
		rm_tristripper_tri_key curr_tri_key = rm_tristripper_tri_occurrence_hashmap_make_key(curr_ids[0], curr_ids[1], curr_ids[2]);

		//Build an initial value struct:
		rm_tristripper_tri_occurrence new_occurrence =
		{
			.multiplicity = 1,
			.index = verifier->distinct_valid_tris_count
		};

		//Insert or merge the triangle into the hashmap.
		//If the key-value pair already exists, the value's multiplicity will be increased by 1.
		rm_tristripper_tri_occurrence dummy_occurrence;

		if (!rm_tristripper_tri_occurrence_hashmap_update(&verifier->occurrences, curr_tri_key, new_occurrence, RM_HASHMAP_UPDATE_MODE_INSERT_OR_MERGE, &dummy_occurrence))
		{
			//This is the first occurrence of the current triangle!
			//Increase the number of distinct valid triangles.
			verifier->distinct_valid_tris_count++;
		}

		//We have added a new valid triangle:
		verifier->valid_tris_count++;
	}
}

rm_void rm_tristripper_dispose_verifier(rm_tristripper_verifier* verifier)
{
	//Dispose the hashmap:
	rm_tristripper_tri_occurrence_hashmap_dispose(&verifier->occurrences);
}

rm_bool rm_tristripper_verify(const rm_tristripper_verifier* verifier, const rm_tristripper_strip* strips, rm_size strips_count, rm_bool log_errors)
{
	//Allocate an array of multiplicities for the valid, distinct triangles.
	//Initialize all of them with 0.
	rm_size* distinct_valid_tris_multiplicities = (verifier->distinct_valid_tris_count > 0) ? rm_malloc_zero(verifier->distinct_valid_tris_count * sizeof(rm_size)) : null;

	//Iterate over the strips.
	//Count the total number of valid triangles we have matched.
	rm_size valid_tris_count = 0;
	rm_bool has_found_error = false;

	for (rm_size i = 0; i < strips_count; i++)
	{
		//Get the current strip:
		const rm_tristripper_strip* curr_strip = &strips[i];

		//Iterate over the IDs:
		for (rm_size j = 0; j < (curr_strip->ids_count - 2); j++)
		{
			//Get the IDs for the current triangle:
			rm_tristripper_id curr_ids[] = { curr_strip->ids[j], curr_strip->ids[j + 1], curr_strip->ids[j + 2] };

			//Ignore degenerated triangles:
			if (rm_unlikely((curr_ids[0] == curr_ids[1]) || (curr_ids[1] == curr_ids[2]) || (curr_ids[2] == curr_ids[0])))
			{
				continue;
			}

			//Build a triangle key:
			rm_tristripper_tri_key curr_tri_key = rm_tristripper_tri_occurrence_hashmap_make_key(curr_ids[0], curr_ids[1], curr_ids[2]);

			//Try to query the corresponding triangle.
			//If that fails, we have found an error.
			rm_tristripper_tri_occurrence curr_tri_occurrence;

			if (rm_unlikely(!rm_tristripper_tri_occurrence_hashmap_get_ex(&verifier->occurrences, curr_tri_key, &curr_tri_occurrence)))
			{
				if (log_errors)
				{
					rm_log(RM_LOG_TYPE_WARNING, "Unknown triangle: (%zu, %zu, %zu) is in the strip, but not in the triangle list.", (rm_size)curr_ids[0], (rm_size)curr_ids[1], (rm_size)curr_ids[2]);
				}

				//That's an error:
				has_found_error = true;

				continue;
			}

			//Increment the multiplicity:
			rm_size new_multiplicity = ++(distinct_valid_tris_multiplicities[curr_tri_occurrence.index]);

			if (rm_unlikely(new_multiplicity > curr_tri_occurrence.multiplicity))
			{
				if (log_errors)
				{
					rm_log(RM_LOG_TYPE_WARNING, "Superfluous triangle: (%zu, %zu, %zu) is %zu times in the strip, but only %zu time(s) in the triangle list.", (rm_size)curr_ids[0], (rm_size)curr_ids[1], (rm_size)curr_ids[2], new_multiplicity, curr_tri_occurrence.multiplicity);
				}

				//That's another error:
				has_found_error = true;

				continue;
			}

			//Increment the number of valid triangles:
			valid_tris_count++;
		}
	}

	//Okay, we have not detected any overflow, that's nice.
	//But maybe there are missing triangles?
	if (rm_unlikely(valid_tris_count < verifier->valid_tris_count))
	{
		//Oh no :( find the missing triangles to provide a helpful error message:
		if (log_errors)
		{
			rm_tristripper_tri_occurrence_hashmap_for_each(&verifier->occurrences, rm_tristripper_verifier_log_missing_tris, (rm_void*)distinct_valid_tris_multiplicities);
		}

		//That's an error, too.
		has_found_error = true;
	}

	//Free the multiplicities:
	rm_free(distinct_valid_tris_multiplicities);

	return !has_found_error;
}
