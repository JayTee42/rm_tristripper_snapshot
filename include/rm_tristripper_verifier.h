#ifndef __RM_TRISTRIPPER_VERIFIER_H__
#define __RM_TRISTRIPPER_VERIFIER_H__

#include "rm_hashmap.h"
#include "rm_type.h"

#include "rm_tristripper_common.h"

//A verifier is created from a list of tristripper IDs.
//It's exactly the same data format you would pass to one of the stripping functions.
//You can use the verifier to comapre a number of strips to the original triangles.
//This is pretty useful for testing:
//Each set of tristrips must cover exactly those triangles that form the input.
//The verifier is pretty robust, i. e. it can deal with duplicated triangles.

//A triangle key consists of the three vertex IDs of the triangle, in ascending order:
typedef struct __rm_tristripper_tri_key__
{
	rm_tristripper_id vertex_ids[3];
} rm_tristripper_tri_key;

//A triangle occurrence wraps the multiplicity of a triangle and an index to a corresponding array.
typedef struct __rm_tristripper_tri_occurrence__
{
	rm_size multiplicity;
	rm_size index;
} rm_tristripper_tri_occurrence;

//We need a hashmap monomorphization that maps triangles to their occurrences.
RM_HASHMAP_DECLARE(tristripper_tri_occurrence, rm_tristripper_tri_key, rm_tristripper_tri_occurrence, SKV)

typedef struct __rm_tristripper_verifier__
{
	//The total number of valid triangles:
	rm_size valid_tris_count;

	//The total number of *distinct* valid triangles:
	rm_size distinct_valid_tris_count;

	//The hashmap for the triangle occurrences:
	rm_tristripper_tri_occurrence_hashmap occurrences;
} rm_tristripper_verifier;

//Initialize the verifier from an ID list:
rm_void rm_tristripper_init_verifier(rm_tristripper_verifier* verifier, rm_tristripper_id* ids, rm_size ids_count);

//Dispose a verifier:
rm_void rm_tristripper_dispose_verifier(rm_tristripper_verifier* verifier);

//Verify the given tristrips:
rm_bool rm_tristripper_verify(const rm_tristripper_verifier* verifier, const rm_tristripper_strip* strips, rm_size strips_count, rm_bool log_errors);

#endif
