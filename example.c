#include "rm_tristripper.h"

#include <stdio.h>

int main(void)
{
	//Define triangles:
	rm_tristripper_id ids[] =
	{
		0, 1, 2,
		3, 1, 2,
		4, 2, 3
	};

	//Specify config:
	rm_tristripper_config config =
	{
		.use_tunneling = false,
		.preserve_orientation = false
	};

	//Execute tristripper:
	rm_tristripper_strip* strips;
	rm_size strips_count;

	rm_tristripper_create_strips(ids, 9, &config, &strips, &strips_count);

	//Print results:
	for (rm_size i = 0; i < strips_count; i++)
	{
		for (rm_size j = 0; j < strips[i].ids_count; j++)
		{
			printf("-%" PRIu32 "-", strips[i].ids[j]);
		}

		printf("\n");
	}

	//Free strips:
	rm_tristripper_dispose_strips(strips, strips_count);

	return 0;
}
