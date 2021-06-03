#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "objectlog.h"

uint8_t logbuf[1235];

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof(*arr))

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
	objectlog_iterator_t iter;
	uint8_t len;
	long object_size;

	object_size = objectlog_get_object_size(log, idx);
	if (object_size < 0) {
		printf("String %u is defective\n", idx);
	}
	objectlog_iterator(log, idx, &iter);
	printf("String %u (%ld): ", idx, object_size);
	while (!objectlog_iterator_is_err(&iter)) {
		const char *str = objectlog_get_fragment(log, &iter, &len);
		printf("%.*s", len, str);
		object_size -= len;
		objectlog_next(log, &iter);
	}
	printf("\n");
	if (object_size != 0) {
		printf("Accumulative length info does not match object size, delta: %ld\n", object_size);
	}
}

static bool multiring_ptr_equal(multiring_ptr_t *a, multiring_ptr_t *b) {
	return a->storage == b->storage && a->offset == b->offset;
}

static int check_integrity(objectlog_t *log) {
	multiring_ptr_t first_string = log->ptr_first;
	multiring_ptr_t last_string = log->ptr_last;
	size_t max_steps = log->multiring.size / 2;
	size_t steps = 0;

	if (log->num_entries < 2) {
		return 0;
	}

	log->multiring.ptr_read = first_string;
	while (!multiring_ptr_equal(&last_string, &log->multiring.ptr_read) && steps++ < max_steps) {
		char tmpstr[128] = { 0 };
		uint8_t hdr = multiring_read_one(&log->multiring);
		uint8_t len = hdr & 0x7f;

		multiring_read(&log->multiring, tmpstr, len);
		printf("%.*s", len, tmpstr);
		if (hdr & 0x80) {
			printf("\n");
		}
//		multiring_advance_read(&log->multiring, len);
	}

	printf("\nIntegrity verified in %zu steps for %u entries over %u fragments\n", steps, log->num_entries, log->multiring.num_storage);
	return steps <= log->num_entries * (log->multiring.size / 128) + log->multiring.num_storage;
}

int test_objectlog() {
	objectlog_t log;

	objectlog_init(&log, logbuf, sizeof(logbuf));
	objectlog_write_string(&log, "Hello World!");
	objectlog_write_string(&log, "This is a longer test string");
	objectlog_write_string(&log, "This is a very long test string. It is in fact so long that it won't fit within a single fragment. So how was your day? Mine was great! I got to play around with mind-numbing amounts of pointers on string fragments");

	for (int i = 0; i < 100; i++) {
		int j;
		size_t len, offset = 0;
		char strbuf[300];
		const char *suffix = "This is a very long test string. It is in fact so long that it won't fit within a single fragment. So how was your day? Mine was great! I got to play around with mind-numbing amounts of pointers on string fragments";
		scatter_object_t scatter_list[20];

		len = snprintf(strbuf, sizeof(strbuf), "This is test string %d %.*s", i, rand() % strlen(suffix), suffix);

		for (j = 0; j < ARRAY_SIZE(scatter_list) - 2 && offset < len; j++) {
			size_t left = len - offset;
			size_t entry_len = rand() % left + 1;
			scatter_list[j].ptr = strbuf + offset;
			scatter_list[j].len = entry_len;
			printf("Scatter %d, len %zu\n", j, entry_len);
			offset += entry_len;
		}
		scatter_list[j].ptr = strbuf + offset;
		scatter_list[j].len = len - offset;
		scatter_list[j + 1].len = 0;
		objectlog_write_scattered_object(&log, scatter_list);

		if(!check_integrity(&log)) {
			printf("Integrity compromised after adding string %d\n", i);
			assert(false);
		}
	}

	printf("Stored strings: %u\n", log.num_entries);
	for (unsigned int i = 0; i < log.num_entries; i++) {
		show_string(&log, i);
	}

	hexdump(logbuf, sizeof(logbuf));

	return 0;
}

uint8_t scatterbuf[4096];
uint8_t randombuf[1024 * 1024];
uint8_t cmpbuf[1024 * 1024];

void random_bytes(void *data, size_t len) {
	uint8_t *data8 = data;

	while (len--) {
		*data8++ = rand() % 0xff;
	}
}

multiring_t multi;

int test_multiring() {
	int j;
	scatter_object_t scatter_list[20];
	size_t len = sizeof(scatterbuf), offset = 0;
	size_t avail;

	for (j = 0; j < ARRAY_SIZE(scatter_list) - 2 && offset < len; j++) {
		size_t left = len - offset;
		size_t entry_len = rand() % left + 1;
		scatter_list[j].ptr = scatterbuf + offset;
		scatter_list[j].len = entry_len;
		printf("Scatter %d, len %zu\n", j, entry_len);
		offset += entry_len;
	}
	scatter_list[j].ptr = scatterbuf + offset;
	scatter_list[j].len = len - offset;
	scatter_list[j + 1].len = 0;

	assert(multiring_init(&multi, scatter_list) == 0);

	for (j = 0; j < 100; j++) {
		size_t data_len = rand() % 65536;
		data_len %= multi.size;
		random_bytes(randombuf, data_len);
		multiring_write(&multi, randombuf, data_len);

		avail = multiring_available(&multi);
		assert(avail == data_len);
		printf("Wrote %zu bytes, %zu bytes available, size: %u\n", data_len, avail, multi.size);
		multiring_read(&multi, cmpbuf, avail);
		if (memcmp(cmpbuf, randombuf, data_len)) {
			printf("Missmatch!\n");
			printf("Original:\n");
			hexdump(randombuf, data_len);
			printf("Copy:\n");
			hexdump(cmpbuf, data_len);
			printf("Scatter buffer:\n");
//			hexdump(multi.storage->ptr, sizeof(scatterbuf) - ((unsigned long long)multi.storage->ptr - (unsigned long long)scatterbuf));
			hexdump(multi.storage->ptr, data_len);
		}
		assert(!memcmp(cmpbuf, randombuf, data_len));
	}
}

int main() {
	unsigned long seed = time(NULL);
	seed = 1622589983;
	printf("Seed: %lu\n", seed);
	srand(seed);
/*
	for (int i = 0; i < 100; i++) {
		test_multiring();
	}
*/
//	return 0;
	test_objectlog();
	return 0;
}
