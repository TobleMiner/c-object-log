#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "objectlog.h"

uint8_t logbuf[1235];

static void hexdump(const void *data, size_t len) {
	const uint8_t *data8 = data;
	size_t full_len = len;

	while(len--) {
		printf("%02x ", *data8++);
		if ((full_len - len) % 32 == 0) {
			printf("\n");
		}
	}
	printf("\n");
}

static void show_string(objectlog_t *log, unsigned int idx) {
	long iter;
	uint8_t len;
	long object_size;

	object_size = objectlog_get_object_size(log, idx);
	if (object_size < 0) {
		printf("String %u is defective\n", idx);
	}
	iter = objectlog_iterator(log, idx);
	printf("String %u (%ld): ", idx, object_size);
	while (iter >= 0) {
		const char *str = objectlog_get_fragment(log, iter, &len);
		printf("%.*s", len, str);
		object_size -= len;
		iter = objectlog_next(log, iter);
	}
	printf("\n");
	if (object_size != 0) {
		printf("Accumulative length info does not match object size, delta: %ld\n", object_size);
	}
}

int main() {
	objectlog_t log;

	srand(time(NULL));

	objectlog_init(&log, logbuf, sizeof(logbuf));
	objectlog_write_string(&log, "Hello World!");
	objectlog_write_string(&log, "This is a longer test string");
	objectlog_write_string(&log, "This is a very long test string. It is in fact so long that it won't fit within a single fragment. So how was your day? Mine was great! I got to play around with mind-numbing amounts of pointers on string fragments");

	for (int i = 0; i < 100; i++) {
		char strbuf[300];
		const char *suffix = "This is a very long test string. It is in fact so long that it won't fit within a single fragment. So how was your day? Mine was great! I got to play around with mind-numbing amounts of pointers on string fragments";

		snprintf(strbuf, sizeof(strbuf), "This is test string %d %.*s", i, rand() % strlen(suffix), suffix);
		objectlog_write_string(&log, strbuf);
	}

	printf("Stored strings: %u\n", log.num_entries);
	for (unsigned int i = 0; i < log.num_entries; i++) {
		show_string(&log, i);
	}

	hexdump(logbuf, sizeof(logbuf));

	return 0;
}
