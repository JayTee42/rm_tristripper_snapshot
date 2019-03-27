#include "rm_vec.h"

//Emit non-inline versions:
extern rm_void __rm_vec_ensure_capacity__(rm_void* data, rm_size* capacity, rm_size min_capacity, rm_size element_size);
extern rm_void __rm_vec_trim_capacity__(rm_void* data, rm_size count, rm_size* capacity, rm_size element_size);
extern rm_void __rm_vec_increment_count__(rm_void* data, rm_size* count, rm_size* capacity, rm_size element_size);
