#ifndef __RM_HASHMAP_H__
#define __RM_HASHMAP_H__

/*
	This hashmap implementation is (not so loosely) based on the Java HashMap (https://github.com/CyC2018/JDK-Source-Code/blob/master/src/HashMap.java).
	Except we omit the "treeification" of nodes. The Java version does that as soon as it reaches 8 entries in a bucket.
	That may help in edge cases, but adds a huge load of complexity - we really don't want this.

	This is a generic implementation. All types and functions are generated via macros *per key-value combination*.
*/

#include "rm_assert.h"
#include "rm_macro.h"
#include "rm_mem.h"
#include "rm_type.h"
#include "rm_vec.h"

//********************************************************
//	Type declarations
//********************************************************

//Hashes are used to address entries inside the table, so they should be rm_size.
typedef rm_size rm_hashmap_hash;

//Different update behaviors for rm_hashmap_update(...):
typedef enum __rm_hashmap_update_mode__
{
	RM_HASHMAP_UPDATE_MODE_REPLACE,
	RM_HASHMAP_UPDATE_MODE_INSERT,
	RM_HASHMAP_UPDATE_MODE_SET,
	RM_HASHMAP_UPDATE_MODE_MERGE,
	RM_HASHMAP_UPDATE_MODE_INSERT_OR_MERGE
} rm_hashmap_update_mode;

//********************************************************
//	Generic type declarations
//********************************************************

/*
	In a hashmap entry struct, there are four members:

	- The key addressing the entry (user-defined type)
	- The corresponding hash (RM_HASHMAP_HASH_EMPTY for empty entries)
	- The value (user-defined type)
	- The index of the next entry in the collision vector (or RM_HASHMAP_NO_MORE_ENTRIES; no pointers because of reallocs)

	Because of the potentially large array allocations, it is surely a good idea to optimize the struct member ordering for minimum size.
	The problem: Because there are two user-defined types in the struct (key and value), the optimal ordering depends on their size.
	Hash and index are 8 bytes in size (at least on all sane 64-bit platforms). So we group them together and call them "S".
	In the same manner, we have "K" for the key and "V" for the value.
	Now there are 6 permutations: SKV, SVK, KSV, KVS, VSK and VKS.
	Let's first define a macro for all of them:
*/

#define RM_HASHMAP_DECLARE_ENTRY_TYPE_SKV(name)	\
typedef struct __rm_##name##_hashmap_entry__   	\
{                                              	\
	rm_hashmap_hash hash;                      	\
	rm_size next_entry_index;                  	\
	rm_##name##_hashmap_key key;               	\
	rm_##name##_hashmap_value value;           	\
} rm_##name##_hashmap_entry;

#define RM_HASHMAP_DECLARE_ENTRY_TYPE_SVK(name)	\
typedef struct __rm_##name##_hashmap_entry__   	\
{                                              	\
	rm_hashmap_hash hash;                      	\
	rm_size next_entry_index;                  	\
	rm_##name##_hashmap_value value;           	\
	rm_##name##_hashmap_key key;               	\
} rm_##name##_hashmap_entry;

#define RM_HASHMAP_DECLARE_ENTRY_TYPE_KSV(name)	\
typedef struct __rm_##name##_hashmap_entry__   	\
{                                              	\
	rm_##name##_hashmap_key key;               	\
	rm_hashmap_hash hash;                      	\
	rm_size next_entry_index;                  	\
	rm_##name##_hashmap_value value;           	\
} rm_##name##_hashmap_entry;

#define RM_HASHMAP_DECLARE_ENTRY_TYPE_KVS(name)	\
typedef struct __rm_##name##_hashmap_entry__   	\
{                                              	\
	rm_##name##_hashmap_key key;               	\
	rm_##name##_hashmap_value value;           	\
	rm_hashmap_hash hash;                      	\
	rm_size next_entry_index;                  	\
} rm_##name##_hashmap_entry;

#define RM_HASHMAP_DECLARE_ENTRY_TYPE_VSK(name)	\
typedef struct __rm_##name##_hashmap_entry__   	\
{                                              	\
	rm_##name##_hashmap_value value;           	\
	rm_hashmap_hash hash;                      	\
	rm_size next_entry_index;                  	\
	rm_##name##_hashmap_key key;               	\
} rm_##name##_hashmap_entry;

#define RM_HASHMAP_DECLARE_ENTRY_TYPE_VKS(name)	\
typedef struct __rm_##name##_hashmap_entry__   	\
{                                              	\
	rm_##name##_hashmap_value value;           	\
	rm_##name##_hashmap_key key;               	\
	rm_hashmap_hash hash;                      	\
	rm_size next_entry_index;                  	\
} rm_##name##_hashmap_entry;

//... and also the packed variant:
#define RM_HASHMAP_DECLARE_ENTRY_TYPE_PACKED(name)	\
typedef packed_struct                             	\
(                                                 	\
	struct __rm_##name##_hashmap_entry__          	\
	{                                             	\
		rm_hashmap_hash hash;                     	\
		rm_size next_entry_index;                 	\
		rm_##name##_hashmap_key key;              	\
		rm_##name##_hashmap_value value;          	\
	}                                             	\
) rm_##name##_hashmap_entry;

#define RM_HASHMAP_DECLARE_TYPES(name, key_type, value_type, struct_order)                                                                         	\
                                                                                                                                                   	\
/* Key and value */                                                                                                                                	\
typedef key_type rm_##name##_hashmap_key;                                                                                                          	\
typedef value_type rm_##name##_hashmap_value;                                                                                                      	\
                                                                                                                                                   	\
/* A hash function to map from a key to a hash. */                                                                                                 	\
typedef rm_hashmap_hash (*rm_##name##_hashmap_hash_func)(rm_##name##_hashmap_key);                                                                 	\
                                                                                                                                                   	\
/* A predicate function to compare keys. */                                                                                                        	\
typedef rm_bool (*rm_##name##_hashmap_key_compare_func)(rm_##name##_hashmap_key, rm_##name##_hashmap_key);                                         	\
                                                                                                                                                   	\
/* Ref and unref functions for memory management. */                                                                                               	\
typedef rm_##name##_hashmap_key (*rm_##name##_hashmap_key_ref_func)(rm_##name##_hashmap_key);                                                      	\
typedef rm_void (*rm_##name##_hashmap_key_unref_func)(rm_##name##_hashmap_key);                                                                    	\
                                                                                                                                                   	\
typedef rm_##name##_hashmap_value (*rm_##name##_hashmap_value_ref_func)(rm_##name##_hashmap_value);                                                	\
typedef rm_void (*rm_##name##_hashmap_value_unref_func)(rm_##name##_hashmap_value);                                                                	\
                                                                                                                                                   	\
/* A merge function to combine values. */                                                                                                          	\
typedef rm_##name##_hashmap_value (*rm_##name##_hashmap_merge_func)(rm_##name##_hashmap_key, rm_##name##_hashmap_value, rm_##name##_hashmap_value);	\
                                                                                                                                                   	\
/* A single entry inside the hashmap (order is chosen by the user because we have no preprocessor-sizeof(...). */                                  	\
RM_HASHMAP_DECLARE_ENTRY_TYPE_##struct_order(name)                                                                                                 	\
                                                                                                                                                   	\
/* Pointers to (const) hashmap entries */                                                                                                          	\
typedef rm_##name##_hashmap_entry* rm_##name##_hashmap_entry_ptr;                                                                                  	\
typedef rm_##name##_hashmap_entry const* rm_##name##_hashmap_entry_const_ptr;                                                                      	\
                                                                                                                                                   	\
/* A vector type to store hashmap entries */                                                                                                       	\
typedef rm_vec(rm_##name##_hashmap_entry) rm_##name##_hashmap_collision_vec;                                                                       	\
                                                                                                                                                   	\
/* The hashmap itself */                                                                                                                           	\
typedef struct __rm_##name##_hashmap__                                                                                                             	\
{                                                                                                                                                  	\
	/* How many key-value-pairs are currently saved inside the hasmap? */                                                                          	\
	public_interface_get rm_size count;                                                                                                            	\
                                                                                                                                                   	\
	/*                                                                                                                                             	\
		The array of buckets and its size.                                                                                                         	\
		Every entry holds a "next index" (can be RM_HASHMAP_NO_MORE_ENTRIES) that points into the collision vector.                                	\
		The array size is guaranteed to be a positive PoT.                                                                                         	\
	*/                                                                                                                                             	\
	rm_##name##_hashmap_entry_ptr buckets;                                                                                                         	\
	rm_size buckets_count;                                                                                                                         	\
                                                                                                                                                   	\
	/* A vector holding additional entries that are used in the case of collisions */                                                              	\
	rm_##name##_hashmap_collision_vec collision_entries;                                                                                           	\
                                                                                                                                                   	\
	/*                                                                                                                                             	\
		The index of the head of the supply list (in relation to the start of the collision vector).                                               	\
		This is a single-linked list that contains entries                                                                                         	\
		from the collision vector that have been deleted.                                                                                          	\
		If we need to allocate a new entry, we first look into the supply list                                                                     	\
		before we increase the collision vector capacity.                                                                                          	\
		If the supply list is empty, this is set to RM_HASHMAP_NO_MORE_ENTRIES.                                                                    	\
	*/                                                                                                                                             	\
	rm_size supply_list;                                                                                                                           	\
                                                                                                                                                   	\
	/*                                                                                                                                             	\
		The count threshold is the number of key-value-pairs                                                                                       	\
		that can be present in the hashmap before rehashing occurs.                                                                                	\
		count_threshold := load_factor * buckets_count                                                                                             	\
	*/                                                                                                                                             	\
	public_interface_get rm_double load_factor;                                                                                                    	\
	public_interface_get rm_size count_threshold;                                                                                                  	\
} rm_##name##_hashmap;                                                                                                                             	\
                                                                                                                                                   	\
/* An iteration function that allows to process all key-value pairs in a hashmap */                                                                	\
typedef rm_bool (*rm_##name##_hashmap_iteration_func)(const rm_##name##_hashmap*, rm_##name##_hashmap_key, rm_##name##_hashmap_value, rm_void*);   	\
                                                                                                                                                   	\
/* A debug version of the iteration function */                                                                                                    	\
typedef rm_bool (*rm_##name##_hashmap_debug_iteration_func)(const rm_##name##_hashmap*, rm_##name##_hashmap_entry_const_ptr, rm_bool, rm_void*);

//********************************************************
//	Constants
//********************************************************

//The initial number of buckets when allocating:
#define RM_HASHMAP_START_BUCKET_COUNT ((rm_size)8)

//The maximum number of buckets a hashmap can hold (this must be a PoT):
#define RM_HASHMAP_MAX_BUCKETS_COUNT (((rm_size)1) << ((sizeof(rm_size) == 8) ? 56 : 30))

//Nice default value for the load factor:
#define RM_HASHMAP_DEFAULT_LOAD_FACTOR 0.5

//We use special hash and entry index values to indicate the absence of entries.
//RM_HASHMAP_HASH_EMPTY may still be returned from a hash function, but in that case, it will be mapped to something else (see rm_##name##_hashmap_hash_key(...)).
//_Caution_: Do not change these constants! We rely on rm_malloc_zero(...) to produce empty entries.
#define RM_HASHMAP_HASH_EMPTY ((rm_hashmap_hash)0)
#define RM_HASHMAP_NO_MORE_ENTRIES ((rm_size)0)

//********************************************************
//	Macro definitions
//********************************************************

//Calculate a sufficient bucket count for the given number of items and a load factor.
#define rm_hashmap_get_sufficient_bucket_count(items_count, load_factor) ((rm_size)(((rm_double)(items_count)) / (load_factor)) + 1)

/*
	Determine the new threshold for the count via bucket count and load factor.
	Make sure we stay >= 1 and <= SIZE_MAX, double values could overflow that.
	If we have already reached the maximum bucket count, we use SIZE_MAX as "infinity".
*/
#define rm_hashmap_calculate_count_threshold(load_factor, buckets_count)                                                                                	\
({                                                                                                                                                      	\
	rm_double _load_factor = (load_factor);                                                                                                             	\
	rm_size _buckets_count = (buckets_count);                                                                                                           	\
                                                                                                                                                        	\
	(_buckets_count == RM_HASHMAP_MAX_BUCKETS_COUNT) ? SIZE_MAX : (rm_size)rm_clamp(_load_factor * (rm_double)_buckets_count, 1.0, (rm_double)SIZE_MAX);	\
})

//Get the bucket for an entry with the given hash.
//This must always succeed - therefore, the bucket count must be >= 1.
#define rm_hashmap_get_bucket_for_hash(map, hash)                                      	\
({                                                                                     	\
	typeof(map) _map = (map);                                                          	\
	rm_assert(_map->buckets_count > 0, "Tried to get a bucket from an empty hashmap.");	\
                                                                                       	\
	&_map->buckets[(hash) & (_map->buckets_count - 1)];                                	\
})

//Check if an entry pointer points to an empty entry:
#define rm_hashmap_is_empty_entry(entry)     	\
({                                           	\
	((entry)->hash == RM_HASHMAP_HASH_EMPTY);	\
})

//Get an entry pointer from an entry index. If RM_HASHMAP_NO_MORE_ENTRIES is passed, null will be returned.
#define rm_hashmap_get_entry_ptr(map, index)                                                                         	\
({                                                                                                                   	\
	/*                                                                                                               	\
		We avoid the misinterpretation of the valid index 0 as RM_HASHMAP_NO_MORE_ENTRIES by adding 1 to every index.	\
		At this point, we have to undo that.                                                                         	\
	*/                                                                                                               	\
	rm_size _index = (index);                                                                                        	\
	(_index == RM_HASHMAP_NO_MORE_ENTRIES) ? null : rm_vec_ptr_at(&(map)->collision_entries, _index - 1);            	\
})

//Get an entry index from an entry pointer (null is not allowed):
#define rm_hashmap_get_entry_index(map, entry)                                         	\
({                                                                                     	\
	typeof(entry) _entry = (entry);                                                    	\
	rm_assert(_entry, "Passing null to rm_hashmap_get_entry_index(...) is forbidden.");	\
                                                                                       	\
	/* See rm_hashmap_get_entry_ptr(...). */                                           	\
	(((rm_size)(_entry - rm_vec_ptr_at(&(map)->collision_entries, 0))) + 1);           	\
})

//Attach a collision vector entry to the head of the supply list.
//Do not call this for bucket base entries!
#define rm_hashmap_add_to_supply_list(map, entry)                             	\
({                                                                            	\
	typeof(map) _map = (map);                                                 	\
	typeof(entry) _outer_entry = (entry);                                     	\
                                                                              	\
	/* This also works if map->supply_list is RM_HASHMAP_NO_MORE_ENTRIES :) */	\
	_outer_entry->next_entry_index = _map->supply_list;                       	\
	_map->supply_list = rm_hashmap_get_entry_index(_map, _outer_entry);       	\
})

//********************************************************
//	Generic function declarations
//********************************************************

#define RM_HASHMAP_DECLARE_FUNCTIONS(name)                                                                                                                                                        	\
                                                                                                                                                                                                  	\
/*                                                                                                                                                                                                	\
	Initialize a new hashmap.                                                                                                                                                                     	\
	The extended version also allows to choose an initial bucket count € [0, RM_HASHMAP_MAX_BUCKETS_COUNT]                                                                                        	\
	(will be ceiled to a PoT >= 1) and a load factor >= 0.                                                                                                                                        	\
	Hash and key compare functions are required.                                                                                                                                                  	\
	But all ref / unref funcs may be null (even in an asymmetric manner).                                                                                                                         	\
*/                                                                                                                                                                                                	\
inline rm_void rm_##name##_hashmap_init(rm_##name##_hashmap* map);                                                                                                                                	\
rm_void rm_##name##_hashmap_init_ex(rm_##name##_hashmap* map, rm_size initial_buckets_count, rm_double load_factor);                                                                              	\
                                                                                                                                                                                                  	\
/* Dispose an existing hashmap. */                                                                                                                                                                	\
rm_void rm_##name##_hashmap_dispose(rm_##name##_hashmap* map);                                                                                                                                    	\
                                                                                                                                                                                                  	\
/* Clear the given hashmap. The current allocated capacities (bucket count, collision vector capacity) are kept. */                                                                               	\
rm_void rm_##name##_hashmap_clear(rm_##name##_hashmap* map);                                                                                                                                      	\
                                                                                                                                                                                                  	\
/* Check if a given key is in the hashmap. */                                                                                                                                                     	\
rm_bool rm_##name##_hashmap_contains_key(const rm_##name##_hashmap* map, rm_##name##_hashmap_key key);                                                                                            	\
                                                                                                                                                                                                  	\
/*                                                                                                                                                                                                	\
	Try to retrieve the value for the given key.                                                                                                                                                  	\
	Return "default_value" if that fails.                                                                                                                                                         	\
*/                                                                                                                                                                                                	\
inline rm_##name##_hashmap_value rm_##name##_hashmap_get(const rm_##name##_hashmap* map, rm_##name##_hashmap_key key, rm_##name##_hashmap_value default_value);                                   	\
                                                                                                                                                                                                  	\
/*                                                                                                                                                                                                	\
	Try to retrieve the value for the given key.                                                                                                                                                  	\
	If "true" is returned, the given key has been found and the corresponding value has been saved to "*value_ptr".                                                                               	\
	If "false" is returned, the key is not present and the value is left unchanged.                                                                                                               	\
*/                                                                                                                                                                                                	\
rm_bool rm_##name##_hashmap_get_ex(const rm_##name##_hashmap* map, rm_##name##_hashmap_key key, rm_##name##_hashmap_value* value_ptr);                                                            	\
                                                                                                                                                                                                  	\
/* Set the value for a given key. Return if the value has been overwritten (true) or newly inserted (false). */                                                                                   	\
rm_bool rm_##name##_hashmap_set(rm_##name##_hashmap* map, rm_##name##_hashmap_key key, rm_##name##_hashmap_value value);                                                                          	\
                                                                                                                                                                                                  	\
/*                                                                                                                                                                                                	\
	Update the value for a given key. Return if a value for this key already exists.                                                                                                              	\
	If "true" is returned, "*old_value_ptr" receives the old value. For "false", it is left unchanged.                                                                                            	\
	What "updating" means depends on the mode:                                                                                                                                                    	\
                                                                                                                                                                                                  	\
	 - RM_HASHMAP_UPDATE_MODE_REPLACE: Replace, but never insert                                                                                                                                  	\
	 - RM_HASHMAP_UPDATE_MODE_INSERT: Insert, but never replace                                                                                                                                   	\
	 - RM_HASHMAP_UPDATE_MODE_SET: Replace and insert (same as rm_hashmap_set(...))                                                                                                               	\
	 - RM_HASHMAP_UPDATE_MODE_MERGE: Combine old and new value using a merge function, but never insert                                                                                           	\
	 - RM_HASHMAP_UPDATE_MODE_INSERT_OR_MERGE: Like RM_HASHMAP_UPDATE_MODE_MERGE, but also insert if no value is present                                                                          	\
                                                                                                                                                                                                  	\
	*Important*: In case of return value "true", a potential unref function for the old value *will not be called*!                                                                               	\
	You have to do that manually, but not for RM_HASHMAP_UPDATE_MODE_INSERT                                                                                                                       	\
	(in that case, the value is not replaced and still referenced by the hashmap).                                                                                                                	\
*/                                                                                                                                                                                                	\
rm_bool rm_##name##_hashmap_update(rm_##name##_hashmap* map, rm_##name##_hashmap_key key, rm_##name##_hashmap_value value, rm_hashmap_update_mode mode, rm_##name##_hashmap_value* old_value_ptr);	\
                                                                                                                                                                                                  	\
/* Remove a potential value for the given key. Return if it was found (and removed). */                                                                                                           	\
rm_bool rm_##name##_hashmap_remove(rm_##name##_hashmap* map, rm_##name##_hashmap_key key);                                                                                                        	\
                                                                                                                                                                                                  	\
/*                                                                                                                                                                                                	\
	Call "iteration_func" for each key-value pair inside the hashmap.                                                                                                                             	\
	The function receives the map, the pair and a context. Order is undefined.                                                                                                                    	\
*/                                                                                                                                                                                                	\
inline rm_void rm_##name##_hashmap_for_each(const rm_##name##_hashmap* map, rm_##name##_hashmap_iteration_func iteration_func, rm_void* context);                                                 	\
                                                                                                                                                                                                  	\
/*                                                                                                                                                                                                	\
	Call "_hashmap_debug_iteration_func" for each key-value pair inside the hashmap.                                                                                                              	\
	The function receives the map, the entry, a flag to detect collisions and a context. Order is undefined.                                                                                      	\
*/                                                                                                                                                                                                	\
inline rm_void rm_##name##_hashmap_debug_for_each(const rm_##name##_hashmap* map, rm_##name##_hashmap_debug_iteration_func debug_iteration_func, rm_void* context);

//********************************************************
//	Generic function definitions (private)
//********************************************************

#define RM_HASHMAP_DEFINE_HASH_KEY(name, hash_func)                                                                       	\
                                                                                                                          	\
/* Calculate the hash for a given key. */                                                                                 	\
static inline rm_hashmap_hash rm_##name##_hashmap_hash_key(rm_##name##_hashmap_key key)                                   	\
{                                                                                                                         	\
	/* Get a function pointer to the hash function. */                                                                    	\
	rm_##name##_hashmap_hash_func hash = (rm_##name##_hashmap_hash_func)hash_func;                                        	\
                                                                                                                          	\
	/* Calculate the hash with it. */                                                                                     	\
	rm_hashmap_hash new_hash = hash(key);                                                                                 	\
                                                                                                                          	\
	/* Make sure we don't encounter RM_HASHMAP_HASH_EMPTY in the wild, but still make use of bucket 0. */                 	\
	return (new_hash == RM_HASHMAP_HASH_EMPTY) ? (((rm_hashmap_hash)1) << ((8 * sizeof(rm_hashmap_hash)) - 1)) : new_hash;	\
}

#define RM_HASHMAP_DEFINE_KEY_MATCHES_ENTRY(name, key_compare_func)                                                                                       	\
                                                                                                                                                          	\
/* Does the given key correspond to the given entry? */                                                                                                   	\
static inline rm_bool rm_##name##_hashmap_key_matches_entry(rm_##name##_hashmap_key key, rm_hashmap_hash hash, rm_##name##_hashmap_entry_const_ptr entry) 	\
{                                                                                                                                                         	\
	/* Get a function pointer to the key compare function. */                                                                                             	\
	rm_##name##_hashmap_key_compare_func key_compare = (rm_##name##_hashmap_key_compare_func)key_compare_func;                                            	\
                                                                                                                                                          	\
	/*                                                                                                                                                    	\
		Perform the following two-step comparison algorithm:                                                                                              	\
                                                                                                                                                          	\
		1.) Compare the hashes. They are not necessarily equal because we cut off some bits when determining the offset.                                  	\
		    Comparing hashes is fast and cheap. Equal hashes are required, but not sufficient for equality.                                               	\
                                                                                                                                                          	\
	    2.) If the hashes are equal, we have to invoke the key comparison function.                                                                       	\
	        It is required and sufficient for equality.                                                                                                   	\
	*/                                                                                                                                                    	\
                                                                                                                                                          	\
	return (entry->hash == hash) && key_compare(key, entry->key);                                                                                         	\
}

#define RM_HASHMAP_DEFINE_RESIZE(name)                                                                                                                                                                   	\
                                                                                                                                                                                                         	\
/*                                                                                                                                                                                                       	\
	Double the bucket count of the hashmap and rehash the entries.                                                                                                                                       	\
	"Rehashing" is a pretty misleading term, the hashes will remain the same.                                                                                                                            	\
	But some entries (50% for a perfect hash func) will move to different buckets.                                                                                                                       	\
	That's because we increment the number of *relevant* bits in a hash.                                                                                                                                 	\
*/                                                                                                                                                                                                       	\
static rm_void rm_##name##_hashmap_resize(rm_##name##_hashmap* map)                                                                                                                                      	\
{                                                                                                                                                                                                        	\
	/* We must not end up here without having allocated. */                                                                                                                                              	\
	rm_assert(map->buckets_count > 0, "Started hashmap resizing without having allocated yet.");                                                                                                         	\
                                                                                                                                                                                                         	\
	/* Double the number of buckets and calculate the new count threshold. */                                                                                                                            	\
	rm_size new_buckets_count = map->buckets_count << 1;                                                                                                                                                 	\
	rm_size new_count_threshold = rm_hashmap_calculate_count_threshold(map->load_factor, new_buckets_count);                                                                                             	\
                                                                                                                                                                                                         	\
	/* Reallocate the table and zero the upper half out. */                                                                                                                                              	\
	map->buckets = rm_realloc(map->buckets, new_buckets_count * sizeof(rm_##name##_hashmap_entry));                                                                                                      	\
	rm_mem_set(&map->buckets[map->buckets_count], 0, map->buckets_count * sizeof(rm_##name##_hashmap_entry));                                                                                            	\
                                                                                                                                                                                                         	\
	/* Print some debug log info. */                                                                                                                                                                     	\
	rm_log(RM_LOG_TYPE_DEBUG, "Hashmap resize (entries: %zu, buckets: %zu -> %zu, threshold: %zu -> %zu)", map->count, map->buckets_count, new_buckets_count, map->count_threshold, new_count_threshold);	\
                                                                                                                                                                                                         	\
	/*                                                                                                                                                                                                   	\
		Iterate over the old table in order to rehash it.                                                                                                                                                	\
	*/                                                                                                                                                                                                   	\
	for (rm_size i = 0; i < map->buckets_count; i++)                                                                                                                                                     	\
	{                                                                                                                                                                                                    	\
		/* Get the current entry. */                                                                                                                                                                     	\
		rm_##name##_hashmap_entry_ptr entry = &map->buckets[i];                                                                                                                                          	\
                                                                                                                                                                                                         	\
		/* Is the whole bucket empty? */                                                                                                                                                                 	\
		if (rm_hashmap_is_empty_entry(entry))                                                                                                                                                            	\
		{                                                                                                                                                                                                	\
			continue;                                                                                                                                                                                    	\
		}                                                                                                                                                                                                	\
                                                                                                                                                                                                         	\
 		/*                                                                                                                                                                                               	\
			Okay, "rehashing" a bucket (especially one with multiple entries) is a little bit tricky.                                                                                                    	\
			Let n: (2^n = map->buckets_count).                                                                                                                                                           	\
			map->buckets_count (the old bucket count) is a PoT >= 1, so n >= 0.                                                                                                                          	\
			All hashes that have been used as index were ANDed with 2^n - 1.                                                                                                                             	\
			That means bit n in the hash had no influence on the resulting index.                                                                                                                        	\
			But now (new_buckets_count = 2^(n + 1)) it has.                                                                                                                                              	\
			Hence, entries in one bucket must be split into two new buckets based on bit n.                                                                                                              	\
			We call them the "zero bucket" and the "one bucket".                                                                                                                                         	\
                                                                                                                                                                                                         	\
			There is another pitfall: One of the two resulting lists starts with the current bucket base entry (depending on bit n).                                                                     	\
			But the other one (if present) starts with a collision vector entry that's being moved to a bucket base entry.                                                                               	\
			In that case, we have to mark the original collision vector entry as empty and queue it to the supply list.                                                                                  	\
                                                                                                                                                                                                         	\
			Let's start by determining the value of bit n in the hash of the bucket base entry.                                                                                                          	\
		*/                                                                                                                                                                                               	\
                                                                                                                                                                                                         	\
		rm_bool bucket_base_bit_n = (entry->hash & map->buckets_count) != 0;                                                                                                                             	\
                                                                                                                                                                                                         	\
		/*                                                                                                                                                                                               	\
			If there is only the bucket base entry, we can perform a cheap early exit.                                                                                                                   	\
			And if bit n is 0, we do not even have to move the entry at all :)                                                                                                                           	\
		*/                                                                                                                                                                                               	\
		if (rm_likely(entry->next_entry_index == RM_HASHMAP_NO_MORE_ENTRIES))                                                                                                                            	\
		{                                                                                                                                                                                                	\
			if (bucket_base_bit_n)                                                                                                                                                                       	\
			{                                                                                                                                                                                            	\
				map->buckets[map->buckets_count | i] = *entry;                                                                                                                                           	\
                                                                                                                                                                                                         	\
				/* The old bucket (zero bucket) is empty now. */                                                                                                                                         	\
				entry->hash = RM_HASHMAP_HASH_EMPTY;                                                                                                                                                     	\
			}                                                                                                                                                                                            	\
                                                                                                                                                                                                         	\
			continue;                                                                                                                                                                                    	\
		}                                                                                                                                                                                                	\
                                                                                                                                                                                                         	\
		/* Sort all the entries into either the zero or the one bucket. */                                                                                                                               	\
		rm_##name##_hashmap_entry_ptr zero_bucket_head = null;                                                                                                                                           	\
		rm_##name##_hashmap_entry_ptr zero_bucket_tail = null;                                                                                                                                           	\
		rm_##name##_hashmap_entry_ptr one_bucket_head = null;                                                                                                                                            	\
		rm_##name##_hashmap_entry_ptr one_bucket_tail = null;                                                                                                                                            	\
                                                                                                                                                                                                         	\
		do                                                                                                                                                                                               	\
		{                                                                                                                                                                                                	\
			if (entry->hash & map->buckets_count)                                                                                                                                                        	\
			{                                                                                                                                                                                            	\
				/* Bit n is 1. */                                                                                                                                                                        	\
				if (rm_unlikely(one_bucket_head != null))                                                                                                                                                	\
				{                                                                                                                                                                                        	\
					one_bucket_tail->next_entry_index = rm_hashmap_get_entry_index(map, entry);                                                                                                          	\
				}                                                                                                                                                                                        	\
				else                                                                                                                                                                                     	\
				{                                                                                                                                                                                        	\
					one_bucket_head = entry;                                                                                                                                                             	\
				}                                                                                                                                                                                        	\
                                                                                                                                                                                                         	\
				one_bucket_tail = entry;                                                                                                                                                                 	\
			}                                                                                                                                                                                            	\
			else                                                                                                                                                                                         	\
			{                                                                                                                                                                                            	\
				/* Bit n is 0. */                                                                                                                                                                        	\
				if (rm_unlikely(zero_bucket_head != null))                                                                                                                                               	\
				{                                                                                                                                                                                        	\
					zero_bucket_tail->next_entry_index = rm_hashmap_get_entry_index(map, entry);                                                                                                         	\
				}                                                                                                                                                                                        	\
				else                                                                                                                                                                                     	\
				{                                                                                                                                                                                        	\
					zero_bucket_head = entry;                                                                                                                                                            	\
				}                                                                                                                                                                                        	\
                                                                                                                                                                                                         	\
				zero_bucket_tail = entry;                                                                                                                                                                	\
			}                                                                                                                                                                                            	\
		} while ((entry = rm_hashmap_get_entry_ptr(map, entry->next_entry_index)) != null);                                                                                                              	\
                                                                                                                                                                                                         	\
		/*                                                                                                                                                                                               	\
			*Important*: The one bucket must be handled first!                                                                                                                                           	\
			Consider the following configuration:                                                                                                                                                        	\
                                                                                                                                                                                                         	\
				- Bucket base entry -> one bucket                                                                                                                                                        	\
				- First collision entry -> zero bucket                                                                                                                                                   	\
                                                                                                                                                                                                         	\
			If we handled the zero bucket first, its head would overwrite the new one bucket head.                                                                                                       	\
		*/                                                                                                                                                                                               	\
                                                                                                                                                                                                         	\
		/* Is there anything inside the one bucket? */                                                                                                                                                   	\
		if (one_bucket_head)                                                                                                                                                                             	\
		{                                                                                                                                                                                                	\
			/* Fix the tail. */                                                                                                                                                                          	\
			one_bucket_tail->next_entry_index = RM_HASHMAP_NO_MORE_ENTRIES;                                                                                                                              	\
                                                                                                                                                                                                         	\
			/* Assign the head of the one bucket to the newly-created bucket in the upper half of the table. */                                                                                          	\
			map->buckets[map->buckets_count | i] = *one_bucket_head;                                                                                                                                     	\
                                                                                                                                                                                                         	\
			/*                                                                                                                                                                                           	\
				If bit n in the old bucket base entry hash is 0, the head of the one bucket is from the collision vector.                                                                                	\
				In that case, we have to put the collision vector entry into the supply list.                                                                                                            	\
			*/                                                                                                                                                                                           	\
			if (!bucket_base_bit_n)                                                                                                                                                                      	\
			{                                                                                                                                                                                            	\
				rm_hashmap_add_to_supply_list(map, one_bucket_head);                                                                                                                                     	\
			}                                                                                                                                                                                            	\
		}                                                                                                                                                                                                	\
                                                                                                                                                                                                         	\
		/* Is there anything inside the zero bucket? */                                                                                                                                                  	\
		if (zero_bucket_head)                                                                                                                                                                            	\
		{                                                                                                                                                                                                	\
			/* Fix the tail. */                                                                                                                                                                          	\
			zero_bucket_tail->next_entry_index = RM_HASHMAP_NO_MORE_ENTRIES;                                                                                                                             	\
                                                                                                                                                                                                         	\
			/*                                                                                                                                                                                           	\
				This is nearly the same logic as for the one bucket, with an important exception:                                                                                                        	\
				If the old bucket base entry is the head of the zero bucket (bit n is 0), we don't have to move anything at all.                                                                         	\
			*/                                                                                                                                                                                           	\
			if (bucket_base_bit_n)                                                                                                                                                                       	\
			{                                                                                                                                                                                            	\
				map->buckets[i] = *zero_bucket_head;                                                                                                                                                     	\
				rm_hashmap_add_to_supply_list(map, zero_bucket_head);                                                                                                                                    	\
			}                                                                                                                                                                                            	\
		}                                                                                                                                                                                                	\
		else                                                                                                                                                                                             	\
		{                                                                                                                                                                                                	\
			/*                                                                                                                                                                                           	\
				The zero bucket is empty -> everything has been moved into the one bucket.                                                                                                               	\
				In that case, we still have to mark the existing bucket base entry as empty.                                                                                                             	\
				This step is not necessary for the one bucket whichis empty at the beginning because of the memset with 0.                                                                               	\
			*/                                                                                                                                                                                           	\
			map->buckets[i].hash = RM_HASHMAP_HASH_EMPTY;                                                                                                                                                	\
		}                                                                                                                                                                                                	\
	}                                                                                                                                                                                                    	\
                                                                                                                                                                                                         	\
	/* Assign the new values. */                                                                                                                                                                         	\
	map->buckets_count = new_buckets_count;                                                                                                                                                              	\
	map->count_threshold = new_count_threshold;                                                                                                                                                          	\
}

#define RM_HASHMAP_DEFINE_FIND_ENTRY_IN_BUCKET(name)                                                                                                                                                                                          	\
                                                                                                                                                                                                                                              	\
/*                                                                                                                                                                                                                                            	\
	Try to retrieve a pointer to the entry identified by the given key.                                                                                                                                                                       	\
	If the corresponding entry cannot be found, the function will return null.                                                                                                                                                                	\
                                                                                                                                                                                                                                              	\
	"entry" is the first entry in the bucket to search.                                                                                                                                                                                       	\
                                                                                                                                                                                                                                              	\
	"*prev_ptr" receives a pointer to the previous entry.                                                                                                                                                                                     	\
	If the entry is (or would be) located at the bucket base, it is set to null.                                                                                                                                                              	\
                                                                                                                                                                                                                                              	\
	Passing null for "prev_ptr" is okay if this result is not needed.                                                                                                                                                                         	\
*/                                                                                                                                                                                                                                            	\
static rm_##name##_hashmap_entry_ptr rm_##name##_hashmap_find_entry_in_bucket(const rm_##name##_hashmap* map, rm_##name##_hashmap_entry_ptr entry, rm_##name##_hashmap_key key, rm_hashmap_hash hash, rm_##name##_hashmap_entry_ptr* prev_ptr)	\
{                                                                                                                                                                                                                                             	\
	/* Track state of previous entry. */                                                                                                                                                                                                      	\
	rm_##name##_hashmap_entry_ptr prev;                                                                                                                                                                                                       	\
                                                                                                                                                                                                                                              	\
	if (rm_hashmap_is_empty_entry(entry))                                                                                                                                                                                                     	\
	{                                                                                                                                                                                                                                         	\
		/* If the whole bucket is empty, we cannot return anything. */                                                                                                                                                                        	\
		entry = null;                                                                                                                                                                                                                         	\
		prev = null;                                                                                                                                                                                                                          	\
	}                                                                                                                                                                                                                                         	\
	else if (rm_likely(rm_##name##_hashmap_key_matches_entry(key, hash, entry)))                                                                                                                                                              	\
	{                                                                                                                                                                                                                                         	\
		/*                                                                                                                                                                                                                                    	\
			The bucket base entry is a match.                                                                                                                                                                                                 	\
			Set "prev" to null to indicate that.                                                                                                                                                                                              	\
		*/                                                                                                                                                                                                                                    	\
		prev = null;                                                                                                                                                                                                                          	\
	}                                                                                                                                                                                                                                         	\
	else                                                                                                                                                                                                                                      	\
	{                                                                                                                                                                                                                                         	\
		/* Check the whole linked list inside the collision vector. */                                                                                                                                                                        	\
		prev = entry;                                                                                                                                                                                                                         	\
                                                                                                                                                                                                                                              	\
		while (((entry = rm_hashmap_get_entry_ptr(map, entry->next_entry_index)) != null) && !rm_##name##_hashmap_key_matches_entry(key, hash, entry))                                                                                        	\
		{                                                                                                                                                                                                                                     	\
			prev = entry;                                                                                                                                                                                                                     	\
		}                                                                                                                                                                                                                                     	\
	}                                                                                                                                                                                                                                         	\
                                                                                                                                                                                                                                              	\
	/* Assign the info about the previous entry if it is needed. */                                                                                                                                                                           	\
	if (prev_ptr)                                                                                                                                                                                                                             	\
	{                                                                                                                                                                                                                                         	\
		*prev_ptr = prev;                                                                                                                                                                                                                     	\
	}                                                                                                                                                                                                                                         	\
                                                                                                                                                                                                                                              	\
	/* Return the entry or null. */                                                                                                                                                                                                           	\
	return entry;                                                                                                                                                                                                                             	\
}

#define RM_HASHMAP_DEFINE_FIND_ENTRY(name)                                                                                                                   	\
                                                                                                                                                             	\
/*                                                                                                                                                           	\
	Try to retrieve a pointer to the entry identified by the given key.                                                                                      	\
	If the corresponding entry cannot be found, the function will return null.                                                                               	\
	This is also safe to call for a hashmap without allocated buckets.                                                                                       	\
*/                                                                                                                                                           	\
static inline rm_##name##_hashmap_entry_ptr rm_##name##_hashmap_find_entry(const rm_##name##_hashmap* map, rm_##name##_hashmap_key key, rm_hashmap_hash hash)	\
{                                                                                                                                                            	\
	/* If we have not yet allocated, the entry will definitely not be there. */                                                                              	\
	if (map->buckets_count == 0)                                                                                                                             	\
	{                                                                                                                                                        	\
		return null;                                                                                                                                         	\
	}                                                                                                                                                        	\
                                                                                                                                                             	\
	/* Get the pointer to the first bucket the entry must be in. */                                                                                          	\
	rm_##name##_hashmap_entry_ptr entry = rm_hashmap_get_bucket_for_hash(map, hash);                                                                         	\
                                                                                                                                                             	\
	/* Delegate to the extended version. */                                                                                                                  	\
	return rm_##name##_hashmap_find_entry_in_bucket(map, entry, key, hash, null);                                                                            	\
}

#define RM_HASHMAP_DEFINE_UNREF_ENTRY(name, key_unref_func, value_unref_func)                                                                                        	\
                                                                                                                                                                     	\
/* Unref key and / or value of a single hashmap entry. */                                                                                                            	\
static inline rm_bool rm_##name##_hashmap_unref_entry(const rm_##name##_hashmap* map, rm_##name##_hashmap_key key, rm_##name##_hashmap_value value, rm_void* context)	\
{                                                                                                                                                                    	\
	rm_unused(map);                                                                                                                                                  	\
	rm_unused(context);                                                                                                                                              	\
                                                                                                                                                                     	\
	/* Get function pointers to the unref funcs. */                                                                                                                  	\
	rm_##name##_hashmap_key_unref_func key_unref = (rm_##name##_hashmap_key_unref_func)key_unref_func;                                                               	\
	rm_##name##_hashmap_value_unref_func value_unref = (rm_##name##_hashmap_value_unref_func)value_unref_func;                                                       	\
                                                                                                                                                                     	\
	/* Key */                                                                                                                                                        	\
	if (key_unref)                                                                                                                                                   	\
	{                                                                                                                                                                	\
		key_unref(key);                                                                                                                                              	\
	}                                                                                                                                                                	\
                                                                                                                                                                     	\
	/* Value */                                                                                                                                                      	\
	if (value_unref)                                                                                                                                                 	\
	{                                                                                                                                                                	\
		value_unref(value);                                                                                                                                          	\
	}                                                                                                                                                                	\
                                                                                                                                                                     	\
	/* Keep going! */                                                                                                                                                	\
	return true;                                                                                                                                                     	\
}

#define RM_HASHMAP_DEFINE_UNREF_ALL(name, key_unref_func, value_unref_func)                                   	\
                                                                                                              	\
/* Unref all keys and / or values of entries in the hashmap. */                                               	\
static inline rm_void rm_##name##_hashmap_unref_all(const rm_##name##_hashmap* map)                           	\
{                                                                                                             	\
	/*                                                                                                        	\
		Unref all keys and values via iteration.                                                              	\
		This is an expensive operation, so we only perform it                                                 	\
		if there are entries and at least one unref function pointer is present.                              	\
	*/                                                                                                        	\
	rm_##name##_hashmap_key_unref_func key_unref = (rm_##name##_hashmap_key_unref_func)key_unref_func;        	\
	rm_##name##_hashmap_value_unref_func value_unref = (rm_##name##_hashmap_value_unref_func)value_unref_func;	\
                                                                                                              	\
	if ((map->count > 0) && (key_unref || value_unref))                                                       	\
	{                                                                                                         	\
		rm_##name##_hashmap_for_each(map, rm_##name##_hashmap_unref_entry, null);                             	\
	}                                                                                                         	\
}

//********************************************************
//	Generic function definitions (public, inline)
//********************************************************

#define RM_HASHMAP_DEFINE_INLINE_INIT(name)                                    	\
                                                                               	\
inline rm_void rm_##name##_hashmap_init(rm_##name##_hashmap* map)              	\
{                                                                              	\
	/* Start without allocating and use a default value for the load factor. */	\
	rm_##name##_hashmap_init_ex(map, 0, RM_HASHMAP_DEFAULT_LOAD_FACTOR);       	\
}

#define RM_HASHMAP_DEFINE_INLINE_GET(name)                                                                                                                    	\
                                                                                                                                                              	\
inline rm_##name##_hashmap_value rm_##name##_hashmap_get(const rm_##name##_hashmap* map, rm_##name##_hashmap_key key, rm_##name##_hashmap_value default_value)	\
{                                                                                                                                                             	\
	/* Invoke the extended version. */                                                                                                                        	\
	rm_##name##_hashmap_value value;                                                                                                                          	\
                                                                                                                                                              	\
	return rm_##name##_hashmap_get_ex(map, key, &value) ? value : default_value;                                                                              	\
}

#define RM_HASHMAP_DEFINE_INLINE_FOR_EACH(name)                                                                                                 	\
                                                                                                                                                	\
inline rm_void rm_##name##_hashmap_for_each(const rm_##name##_hashmap* map, rm_##name##_hashmap_iteration_func iteration_func, rm_void* context)	\
{                                                                                                                                               	\
	/* Iterate over the buckets. */                                                                                                             	\
	for (rm_size i = 0; i < map->buckets_count; i++)                                                                                            	\
	{                                                                                                                                           	\
		/* Get the entry. */                                                                                                                    	\
		rm_##name##_hashmap_entry_ptr entry = &map->buckets[i];                                                                                 	\
                                                                                                                                                	\
		/* Ignore entries with empty hashes. */                                                                                                 	\
		if (entry->hash == RM_HASHMAP_HASH_EMPTY)                                                                                               	\
		{                                                                                                                                       	\
			continue;                                                                                                                           	\
		}                                                                                                                                       	\
                                                                                                                                                	\
		/* Traverse the whole bucket. */                                                                                                        	\
		rm_bool shall_continue;                                                                                                                 	\
                                                                                                                                                	\
		do                                                                                                                                      	\
		{                                                                                                                                       	\
			/* Invoke the iteration function and pass the user's context while "true" is returned. */                                           	\
			shall_continue = iteration_func(map, entry->key, entry->value, context);                                                            	\
		} while (shall_continue && ((entry = rm_hashmap_get_entry_ptr(map, entry->next_entry_index)) != null));                                 	\
	}                                                                                                                                           	\
}

#define RM_HASHMAP_DEFINE_INLINE_DEBUG_FOR_EACH(name)                                                                                                             	\
                                                                                                                                                                  	\
inline rm_void rm_##name##_hashmap_debug_for_each(const rm_##name##_hashmap* map, rm_##name##_hashmap_debug_iteration_func debug_iteration_func, rm_void* context)	\
{                                                                                                                                                                 	\
	/* Iterate over the buckets. */                                                                                                                               	\
	for (rm_size i = 0; i < map->buckets_count; i++)                                                                                                              	\
	{                                                                                                                                                             	\
		/* Get the entry. */                                                                                                                                      	\
		rm_##name##_hashmap_entry_const_ptr entry = &map->buckets[i];                                                                                             	\
                                                                                                                                                                  	\
		/* Ignore entries with empty hashes. */                                                                                                                   	\
		if (entry->hash == RM_HASHMAP_HASH_EMPTY)                                                                                                                 	\
		{                                                                                                                                                         	\
			continue;                                                                                                                                             	\
		}                                                                                                                                                         	\
                                                                                                                                                                  	\
		/* Traverse the whole bucket. */                                                                                                                          	\
		rm_bool shall_continue = debug_iteration_func(map, entry, false, context);                                                                                	\
                                                                                                                                                                  	\
		while (shall_continue && ((entry = rm_hashmap_get_entry_ptr(map, entry->next_entry_index)) != null))                                                      	\
		{                                                                                                                                                         	\
			shall_continue = debug_iteration_func(map, entry, true, context);                                                                                     	\
		}                                                                                                                                                         	\
	}                                                                                                                                                             	\
}

//********************************************************
//	Generic function definitions (public, non-inline)
//********************************************************

#define RM_HASHMAP_DEFINE_INIT(name) \
extern rm_void rm_##name##_hashmap_init(rm_##name##_hashmap* map);

#define RM_HASHMAP_DEFINE_INIT_EX(name)                                                                                                               	\
                                                                                                                                                      	\
rm_void rm_##name##_hashmap_init_ex(rm_##name##_hashmap* map, rm_size initial_buckets_count, rm_double load_factor)                                   	\
{                                                                                                                                                     	\
	/* Start with a count of 0. */                                                                                                                    	\
	map->count = 0;                                                                                                                                   	\
                                                                                                                                                      	\
	/* Limit the bucket count. */                                                                                                                     	\
	rm_precond(initial_buckets_count <= RM_HASHMAP_MAX_BUCKETS_COUNT, "Initial bucket count is too big (maximum: %zu)", RM_HASHMAP_MAX_BUCKETS_COUNT);	\
	rm_precond(load_factor >= 0, "Invalid load factor: %lf", load_factor);                                                                            	\
                                                                                                                                                      	\
	if (initial_buckets_count == 0)                                                                                                                   	\
	{                                                                                                                                                 	\
		/* Start without allocating. */                                                                                                               	\
		map->buckets_count = 0;                                                                                                                       	\
		map->buckets = null;                                                                                                                          	\
	}                                                                                                                                                 	\
	else                                                                                                                                              	\
	{                                                                                                                                                 	\
		/* Select the first PoT that is sufficient. */                                                                                                	\
		map->buckets_count = 1;                                                                                                                       	\
                                                                                                                                                      	\
		while (map->buckets_count < initial_buckets_count)                                                                                            	\
		{                                                                                                                                             	\
			map->buckets_count <<= 1;                                                                                                                 	\
		}                                                                                                                                             	\
                                                                                                                                                      	\
		/*                                                                                                                                            	\
			Allocate the first array of buckets.                                                                                                      	\
			Make sure it is zeroed out so we can detect empty entries via nullpointers.                                                               	\
		*/                                                                                                                                            	\
		map->buckets = rm_malloc_zero(map->buckets_count * sizeof(rm_##name##_hashmap_entry));                                                        	\
	}                                                                                                                                                 	\
                                                                                                                                                      	\
	/* Initialize the vector that will hold the entries. */                                                                                           	\
	rm_vec_init(&map->collision_entries);                                                                                                             	\
                                                                                                                                                      	\
	/* The supply list is empty in the beginning. */                                                                                                  	\
	map->supply_list = RM_HASHMAP_NO_MORE_ENTRIES;                                                                                                    	\
                                                                                                                                                      	\
	/* Assign the load factor and calculate the first threshold for the count. */                                                                     	\
	map->load_factor = load_factor;                                                                                                                   	\
	map->count_threshold = rm_hashmap_calculate_count_threshold(map->load_factor, map->buckets_count);                                                	\
}

#define RM_HASHMAP_DEFINE_DISPOSE(name)                            	\
                                                                   	\
rm_void rm_##name##_hashmap_dispose(rm_##name##_hashmap* map)      	\
{                                                                  	\
	/* Unref all keys and values. */                               	\
	rm_##name##_hashmap_unref_all(map);                            	\
                                                                   	\
	/*                                                             	\
		Free the buckets.                                          	\
		If we have not yet allocated, this becomes "rm_free(null)".	\
	*/                                                             	\
	rm_free(map->buckets);                                         	\
                                                                   	\
	/* Dispose the vector of entries. */                           	\
	rm_vec_dispose(&map->collision_entries);                       	\
}

#define RM_HASHMAP_DEFINE_CLEAR(name)                                                           	\
                                                                                                	\
rm_void rm_##name##_hashmap_clear(rm_##name##_hashmap* map)                                     	\
{                                                                                               	\
	/* Unref all keys and values. */                                                            	\
	rm_##name##_hashmap_unref_all(map);                                                         	\
                                                                                                	\
	/* Reset the count. */                                                                      	\
	map->count = 0;                                                                             	\
                                                                                                	\
	/*                                                                                          	\
		Null the buckets -> empty hashes.                                                       	\
		rm_memset() does *not* accept a nullpointer - even for a length of 0.                   	\
	*/                                                                                          	\
	if (rm_likely(map->buckets_count > 0))                                                      	\
	{                                                                                           	\
		rm_mem_set(map->buckets, 0, map->buckets_count * sizeof(rm_##name##_hashmap_entry_ptr));	\
	}                                                                                           	\
                                                                                                	\
	/* Clear the vector of entries. */                                                          	\
	rm_vec_clear(&map->collision_entries);                                                      	\
                                                                                                	\
	/* Clear the supply list. */                                                                	\
	map->supply_list = RM_HASHMAP_NO_MORE_ENTRIES;                                              	\
}

#define RM_HASHMAP_DEFINE_CONTAINS_KEY(name)                                                         	\
                                                                                                     	\
rm_bool rm_##name##_hashmap_contains_key(const rm_##name##_hashmap* map, rm_##name##_hashmap_key key)	\
{                                                                                                    	\
	/* Calculate the hash for the key. */                                                            	\
	rm_hashmap_hash hash = rm_##name##_hashmap_hash_key(key);                                        	\
                                                                                                     	\
	/* Check if the corresponding entry is null. */                                                  	\
	return rm_##name##_hashmap_find_entry(map, key, hash) != null;                                   	\
}

#define RM_HASHMAP_DEFINE_GET(name) \
extern rm_##name##_hashmap_value rm_##name##_hashmap_get(const rm_##name##_hashmap* map, rm_##name##_hashmap_key key, rm_##name##_hashmap_value default_value);

#define RM_HASHMAP_DEFINE_GET_EX(name)                                                                                               	\
                                                                                                                                     	\
rm_bool rm_##name##_hashmap_get_ex(const rm_##name##_hashmap* map, rm_##name##_hashmap_key key, rm_##name##_hashmap_value* value_ptr)	\
{                                                                                                                                    	\
	/* Calculate the hash for the key. */                                                                                            	\
	rm_hashmap_hash hash = rm_##name##_hashmap_hash_key(key);                                                                        	\
                                                                                                                                     	\
	/* Get the corresponding entry. */                                                                                               	\
	rm_##name##_hashmap_entry_ptr entry = rm_##name##_hashmap_find_entry(map, key, hash);                                            	\
                                                                                                                                     	\
	/* Is it present? */                                                                                                             	\
	if (!entry)                                                                                                                      	\
	{                                                                                                                                	\
		return false;                                                                                                                	\
	}                                                                                                                                	\
                                                                                                                                     	\
	/* Assign the value to the out parameter. */                                                                                     	\
	*value_ptr = entry->value;                                                                                                       	\
                                                                                                                                     	\
	return true;                                                                                                                     	\
}

#define RM_HASHMAP_DEFINE_SET(name, value_unref_func)                                                                  	\
                                                                                                                       	\
rm_bool rm_##name##_hashmap_set(rm_##name##_hashmap* map, rm_##name##_hashmap_key key, rm_##name##_hashmap_value value)	\
{                                                                                                                      	\
	/* Get a function pointer to the unref func. */                                                                    	\
	rm_##name##_hashmap_value_unref_func value_unref = (rm_##name##_hashmap_value_unref_func)value_unref_func;         	\
                                                                                                                       	\
	/* Delegate to the more complex update function. */                                                                	\
	rm_##name##_hashmap_value old_value;                                                                               	\
                                                                                                                       	\
	if (rm_##name##_hashmap_update(map, key, value, RM_HASHMAP_UPDATE_MODE_SET, &old_value))                           	\
	{                                                                                                                  	\
		/* The new value has been set. Unref the old one if necessary (the update function does not do that for us). */	\
		if (value_unref)                                                                                               	\
		{                                                                                                              	\
			value_unref(old_value);                                                                                    	\
		}                                                                                                              	\
                                                                                                                       	\
		return true;                                                                                                   	\
	}                                                                                                                  	\
	else                                                                                                               	\
	{                                                                                                                  	\
		return false;                                                                                                  	\
	}                                                                                                                  	\
}

#define RM_HASHMAP_DEFINE_UPDATE(name, key_ref_func, value_ref_func, merge_func)                                                                                                                     \
                                                                                                                                                                                                     \
rm_bool rm_##name##_hashmap_update(rm_##name##_hashmap* map, rm_##name##_hashmap_key key, rm_##name##_hashmap_value value, rm_hashmap_update_mode mode, rm_##name##_hashmap_value* old_value_ptr)    \
{                                                                                                                                                                                                    \
	/* If we have not yet allocated, we must do that now. */                                                                                                                                         \
	if (rm_unlikely(map->buckets_count == 0))                                                                                                                                                        \
	{                                                                                                                                                                                                \
		/* If the mode requires an existing entry, we can leave early. */                                                                                                                            \
		if ((mode == RM_HASHMAP_UPDATE_MODE_REPLACE) || (mode == RM_HASHMAP_UPDATE_MODE_MERGE))                                                                                                      \
		{                                                                                                                                                                                            \
			return false;                                                                                                                                                                            \
		}                                                                                                                                                                                            \
                                                                                                                                                                                                     \
		/*                                                                                                                                                                                           \
			RM_HASHMAP_START_BUCKET_COUNT is a PoT.                                                                                                                                                  \
			It might be above 1 to avoid some reallocations.                                                                                                                                         \
		*/                                                                                                                                                                                           \
		map->buckets_count = RM_HASHMAP_START_BUCKET_COUNT;                                                                                                                                          \
		map->buckets = rm_malloc_zero(map->buckets_count * sizeof(rm_##name##_hashmap_entry));                                                                                                       \
                                                                                                                                                                                                     \
		/* Select the first threshold. */                                                                                                                                                            \
		map->count_threshold = rm_hashmap_calculate_count_threshold(map->load_factor, map->buckets_count);                                                                                           \
	}                                                                                                                                                                                                \
                                                                                                                                                                                                     \
	/* Get function pointers to the ref funcs. */                                                                                                                                                    \
	rm_##name##_hashmap_key_ref_func key_ref = (rm_##name##_hashmap_key_ref_func)key_ref_func;                                                                                                       \
	rm_##name##_hashmap_value_ref_func value_ref = (rm_##name##_hashmap_value_ref_func)value_ref_func;                                                                                               \
	rm_##name##_hashmap_merge_func merge = (rm_##name##_hashmap_merge_func)merge_func;                                                                                                               \
                                                                                                                                                                                                     \
	/* Calculate the hash for the key. */                                                                                                                                                            \
	rm_hashmap_hash hash = rm_##name##_hashmap_hash_key(key);                                                                                                                                        \
                                                                                                                                                                                                     \
	/* Get the bucket for this hash. */                                                                                                                                                              \
	rm_##name##_hashmap_entry_ptr bucket_base = rm_hashmap_get_bucket_for_hash(map, hash);                                                                                                           \
                                                                                                                                                                                                     \
	/* Get the corresponding entry and the previous one. */                                                                                                                                          \
	rm_##name##_hashmap_entry_ptr prev;                                                                                                                                                              \
	rm_##name##_hashmap_entry_ptr entry = rm_##name##_hashmap_find_entry_in_bucket(map, bucket_base, key, hash, &prev);                                                                              \
                                                                                                                                                                                                     \
	/* Does the entry already exist? */                                                                                                                                                              \
	if (entry)                                                                                                                                                                                       \
	{                                                                                                                                                                                                \
		/* Do not assign the old value to the pointee yet. "old_value_ptr" might be "&value" - in that case, we have a problem. */                                                                   \
		rm_##name##_hashmap_value old_value = entry->value;                                                                                                                                          \
                                                                                                                                                                                                     \
		/* How to cope with the existing value? */                                                                                                                                                   \
		switch (mode)                                                                                                                                                                                \
		{                                                                                                                                                                                            \
		case RM_HASHMAP_UPDATE_MODE_REPLACE:                                                                                                                                                         \
		case RM_HASHMAP_UPDATE_MODE_SET:                                                                                                                                                             \
                                                                                                                                                                                                     \
			/* Replace the existing value. Ref it if necessary. */                                                                                                                                   \
			entry->value = value_ref ? value_ref(value) : value;                                                                                                                                     \
			break;                                                                                                                                                                                   \
                                                                                                                                                                                                     \
		case RM_HASHMAP_UPDATE_MODE_INSERT_OR_MERGE:                                                                                                                                                 \
		case RM_HASHMAP_UPDATE_MODE_MERGE:                                                                                                                                                           \
                                                                                                                                                                                                     \
			/* Merge old and new value. Ref the result if necessary. */                                                                                                                              \
			rm_assert(merge, "RM_HASHMAP_UPDATE_MODE_MERGE specified, but merge func pointer is null.");                                                                                             \
                                                                                                                                                                                                     \
			rm_##name##_hashmap_value merged_value = merge(key, entry->value, value);                                                                                                                \
			entry->value = value_ref ? value_ref(merged_value) : merged_value;                                                                                                                       \
                                                                                                                                                                                                     \
			break;                                                                                                                                                                                   \
                                                                                                                                                                                                     \
		case RM_HASHMAP_UPDATE_MODE_INSERT:                                                                                                                                                          \
                                                                                                                                                                                                     \
			/* Nothing to do here, we don't overwrite old values in this mode. */                                                                                                                    \
			break;                                                                                                                                                                                   \
		}                                                                                                                                                                                            \
                                                                                                                                                                                                     \
		/* Now we can safely assign to the pointee (the user has to unref the old value manually if we have replaced it). */                                                                         \
		*old_value_ptr = old_value;                                                                                                                                                                  \
                                                                                                                                                                                                     \
		return true;                                                                                                                                                                                 \
	}                                                                                                                                                                                                \
                                                                                                                                                                                                     \
	/*                                                                                                                                                                                               \
		The entry does not exist yet.                                                                                                                                                                \
		In replace and pure merge mode, we can stop here because inserting new entries is forbidden.                                                                                                 \
	*/                                                                                                                                                                                               \
	if ((mode == RM_HASHMAP_UPDATE_MODE_REPLACE) || (mode == RM_HASHMAP_UPDATE_MODE_MERGE))                                                                                                          \
	{                                                                                                                                                                                                \
		return false;                                                                                                                                                                                \
	}                                                                                                                                                                                                \
                                                                                                                                                                                                     \
	/*                                                                                                                                                                                               \
		If the previous entry is null, we have to place ours at the bucket base.                                                                                                                     \
		*Caution*: At the end of this compound statement, prev is poisoned and must not be used anymore!                                                                                             \
		The reason is the call to rm_vec_push_empty(...): It may reallocate the collision vector.                                                                                                    \
	*/                                                                                                                                                                                               \
	if (rm_likely(prev == null))                                                                                                                                                                     \
	{                                                                                                                                                                                                \
		entry = bucket_base;                                                                                                                                                                         \
	}                                                                                                                                                                                                \
	else                                                                                                                                                                                             \
	{                                                                                                                                                                                                \
		/* Pull a new entry from the supply list resp. from the collision vector. */                                                                                                                 \
		if (map->supply_list != RM_HASHMAP_NO_MORE_ENTRIES)                                                                                                                                          \
		{                                                                                                                                                                                            \
			/* The next entry will be the head of the supply list. */                                                                                                                                \
			prev->next_entry_index = map->supply_list;                                                                                                                                               \
                                                                                                                                                                                                     \
			/* Obtain the entry and assign the new supply list head. */                                                                                                                              \
			entry = rm_hashmap_get_entry_ptr(map, prev->next_entry_index);                                                                                                                           \
			map->supply_list = entry->next_entry_index;                                                                                                                                              \
		}                                                                                                                                                                                            \
		else                                                                                                                                                                                         \
		{                                                                                                                                                                                            \
			/* *Important*: Fix the previous entry *first*! */                                                                                                                                       \
			prev->next_entry_index = map->collision_entries.count + 1;                                                                                                                               \
                                                                                                                                                                                                     \
			/* Down from here, "prev" might be poisoned! */                                                                                                                                          \
			entry = rm_vec_push_empty(&map->collision_entries);                                                                                                                                      \
		}                                                                                                                                                                                            \
	}                                                                                                                                                                                                \
                                                                                                                                                                                                     \
	/*                                                                                                                                                                                               \
		Initialize the entry.                                                                                                                                                                        \
		Ref key and / or value if necessary.                                                                                                                                                         \
	*/                                                                                                                                                                                               \
	entry->key = key_ref ? key_ref(key) : key;                                                                                                                                                       \
	entry->hash = hash;                                                                                                                                                                              \
	entry->value = value_ref ? value_ref(value) : value;                                                                                                                                             \
	entry->next_entry_index = RM_HASHMAP_NO_MORE_ENTRIES;                                                                                                                                            \
                                                                                                                                                                                                     \
	/* Increment the count and resize if necessary. */                                                                                                                                               \
	if (rm_unlikely(++map->count > map->count_threshold))                                                                                                                                            \
	{                                                                                                                                                                                                \
		rm_##name##_hashmap_resize(map);                                                                                                                                                             \
	}                                                                                                                                                                                                \
                                                                                                                                                                                                     \
	return false;                                                                                                                                                                                	 \
}

#define RM_HASHMAP_DEFINE_REMOVE(name)                                                                                                               	\
                                                                                                                                                     	\
rm_bool rm_##name##_hashmap_remove(rm_##name##_hashmap* map, rm_##name##_hashmap_key key)                                                            	\
{                                                                                                                                                    	\
	/* If we have not yet allocated, there is nothing to remove. */                                                                                  	\
	if (rm_unlikely(map->buckets_count == 0))                                                                                                        	\
	{                                                                                                                                                	\
		return false;                                                                                                                                	\
	}                                                                                                                                                	\
                                                                                                                                                     	\
	/* Calculate the hash for the key. */                                                                                                            	\
	rm_hashmap_hash hash = rm_##name##_hashmap_hash_key(key);                                                                                        	\
                                                                                                                                                     	\
	/* Get the corresponding entry and the previous one. */                                                                                          	\
	rm_##name##_hashmap_entry_ptr prev;                                                                                                              	\
	rm_##name##_hashmap_entry_ptr entry = rm_##name##_hashmap_find_entry_in_bucket(map, rm_hashmap_get_bucket_for_hash(map, hash), key, hash, &prev);	\
                                                                                                                                                     	\
	/* If the entry does not exist, there is nothing to do. */                                                                                       	\
	if (!entry)                                                                                                                                      	\
	{                                                                                                                                                	\
		return false;                                                                                                                                	\
	}                                                                                                                                                	\
                                                                                                                                                     	\
	/* Unref key and value if necessary. */                                                                                                          	\
	rm_##name##_hashmap_unref_entry(map, key, entry->value, null);                                                                                   	\
                                                                                                                                                     	\
	/* "prev" indicates if our entry is located inside the collision vector (non-null) or at the bucket base (null). */                              	\
	if (rm_unlikely(prev != null))                                                                                                                   	\
	{                                                                                                                                                	\
		/* If the entry is located inside the collision vector, we just have to move it to the supply list after re-linking the bucket. */           	\
		prev->next_entry_index = entry->next_entry_index;                                                                                            	\
		rm_hashmap_add_to_supply_list(map, entry);                                                                                                   	\
	}                                                                                                                                                	\
	else                                                                                                                                             	\
	{                                                                                                                                                	\
		/*                                                                                                                                           	\
			Our entry is located at the bucket base.                                                                                                 	\
			If there is a following entry inside the collision vector, we "promote" it up to the bucket base instead.                                	\
		*/                                                                                                                                           	\
		rm_##name##_hashmap_entry_ptr next_entry = rm_hashmap_get_entry_ptr(map, entry->next_entry_index);                                           	\
                                                                                                                                                     	\
		if (rm_unlikely(next_entry != null))                                                                                                         	\
		{                                                                                                                                            	\
			*entry = *next_entry;                                                                                                                    	\
                                                                                                                                                     	\
			/* The old "next_entry" from the collision vector can be added to the supply list now. */                                                	\
			rm_hashmap_add_to_supply_list(map, next_entry);                                                                                          	\
		}                                                                                                                                            	\
		else                                                                                                                                         	\
		{                                                                                                                                            	\
			/*                                                                                                                                       	\
				The entry is at the bucket base and there are no more entries.                                                                       	\
				So we just have to clean out the hash to mark this bucket as empty.                                                                  	\
			*/                                                                                                                                       	\
			entry->hash = RM_HASHMAP_HASH_EMPTY;                                                                                                     	\
		}                                                                                                                                            	\
	}                                                                                                                                                	\
                                                                                                                                                     	\
	/* Decrement the count, we definitely have removed one entry at this point. */                                                                   	\
	map->count--;                                                                                                                                    	\
                                                                                                                                                     	\
	return true;                                                                                                                                     	\
}

#define RM_HASHMAP_DEFINE_FOR_EACH(name) \
extern rm_void rm_##name##_hashmap_for_each(const rm_##name##_hashmap* map, rm_##name##_hashmap_iteration_func iteration_func, rm_void* context);

#define RM_HASHMAP_DEFINE_DEBUG_FOR_EACH(name) \
extern rm_void rm_##name##_hashmap_debug_for_each(const rm_##name##_hashmap* map, rm_##name##_hashmap_debug_iteration_func debug_iteration_func, rm_void* context);

//********************************************************
//	Collective macros for decl. and def.
//********************************************************

//Note: The declarations also include the definitions of some public inline functions.
//Those functions will also reappear in the list of definitions below (without INLINE annotation).
//We generate a non-inline version of them.
#define RM_HASHMAP_DECLARE(name, key_type, value_type, struct_order)	\
RM_HASHMAP_DECLARE_TYPES(name, key_type, value_type, struct_order)  	\
RM_HASHMAP_DECLARE_FUNCTIONS(name)                                  	\
RM_HASHMAP_DEFINE_INLINE_INIT(name)                                 	\
RM_HASHMAP_DEFINE_INLINE_GET(name)                                  	\
RM_HASHMAP_DEFINE_INLINE_FOR_EACH(name)                             	\
RM_HASHMAP_DEFINE_INLINE_DEBUG_FOR_EACH(name)

/*
	Hash func: Take a key, return an rm_hashmap_hash.
	Key compare func: Take two keys, return an rm_bool indicating if they are equal.
	Ref and unref functions: They are used to take resp. release ownership of a hashmap key or value.
	For example, consider the case of string keys: In most cases, you want to duplicate the strings before inserting them into the hashmap.
	That also requires to free them later. For these cases, the ref / unref funcs can be used.
	Ref functions are allowed to return a different result (e. g. the result of strdup(...)) *if the following codition is fulfilled*:
	The result of ref_key(some_key) must be a key that is equal to some_key in terms of the key compare function.
*/

#define RM_HASHMAP_DEFINE(name, hash_func, key_compare_func, key_ref_func, key_unref_func, value_ref_func, value_unref_func, merge_func)	\
RM_HASHMAP_DEFINE_HASH_KEY(name, hash_func)                                                                                             	\
RM_HASHMAP_DEFINE_KEY_MATCHES_ENTRY(name, key_compare_func)                                                                             	\
RM_HASHMAP_DEFINE_RESIZE(name)                                                                                                          	\
RM_HASHMAP_DEFINE_FIND_ENTRY_IN_BUCKET(name)                                                                                            	\
RM_HASHMAP_DEFINE_FIND_ENTRY(name)                                                                                                      	\
RM_HASHMAP_DEFINE_UNREF_ENTRY(name, key_unref_func, value_unref_func)                                                                   	\
RM_HASHMAP_DEFINE_UNREF_ALL(name, key_unref_func, value_unref_func)                                                                     	\
RM_HASHMAP_DEFINE_INIT(name)                                                                                                            	\
RM_HASHMAP_DEFINE_INIT_EX(name)                                                                                                         	\
RM_HASHMAP_DEFINE_DISPOSE(name)                                                                                                         	\
RM_HASHMAP_DEFINE_CLEAR(name)                                                                                                           	\
RM_HASHMAP_DEFINE_CONTAINS_KEY(name)                                                                                                    	\
RM_HASHMAP_DEFINE_GET(name)                                                                                                             	\
RM_HASHMAP_DEFINE_GET_EX(name)                                                                                                          	\
RM_HASHMAP_DEFINE_SET(name, value_unref_func)                                                                                           	\
RM_HASHMAP_DEFINE_UPDATE(name, key_ref_func, value_ref_func, merge_func)                                                                	\
RM_HASHMAP_DEFINE_REMOVE(name)                                                                                                          	\
RM_HASHMAP_DEFINE_FOR_EACH(name)                                                                                                        	\
RM_HASHMAP_DEFINE_DEBUG_FOR_EACH(name)

#endif
