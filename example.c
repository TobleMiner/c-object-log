#include <stdio.h>
#include <stdint.h>

#include "objectlog.h"

uint8_t storage0[100];
uint8_t storage1[150];
uint8_t storage2[175];
uint8_t storage3[180];

int main(void) {
	unsigned int idx;
	int err;
	objectlog_t log;
	scatter_object_t scatter_list[5];

	scatter_list[0].ptr = storage2;
	scatter_list[0].len = sizeof(storage2);
	scatter_list[1].ptr = storage0;
	scatter_list[1].len = sizeof(storage0);
	scatter_list[2].ptr = storage3;
	scatter_list[2].len = sizeof(storage3);
	scatter_list[3].ptr = storage1;
	scatter_list[3].len = sizeof(storage1);
	scatter_list[4].ptr = NULL;
	scatter_list[4].len = 0;

	err = objectlog_init_fragmented(&log, scatter_list);
	if (err) {
		fprintf(stderr, "Init failed: %d\n", err);
		return 1;
	}

	objectlog_write_string(&log, "Hello World!");
	objectlog_write_string(&log, "This is a longer test string");
	objectlog_write_string(&log, "This is a very long test string. "
				     "It is in fact so long that it won't fit within a single fragment. "
				     "So how was your day? Mine was great! "
				     "I got to play around with mind-numbing amounts of pointers on string fragments");

	printf("Stored strings: %u\n", log.num_entries);
	for (idx = 0; idx < log.num_entries; idx++) {
		objectlog_iterator_t iter;

		printf("String %u: ", idx);
		for (objectlog_iterator(&log, idx, &iter); !objectlog_iterator_is_err(&iter); objectlog_next(&log, &iter)) {
			uint8_t len;
			const char *str = objectlog_get_fragment(&log, &iter, &len);

			printf("%.*s", len, str);
			objectlog_next(&log, &iter);
		}
		puts("");
	}

	return 0;
}
