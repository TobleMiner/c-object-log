#include <stdbool.h>
#include <string.h>

#include "objectlog.h"

#define MAX_FRAGMENT_LEN 0x7f
#define FRAGMENT_FINAL 0x80
#define FRAGMENT_LEN(x) ((x) & MAX_FRAGMENT_LEN)

#define DIV_ROUND_UP(x, y) (((x) + ((y) - 1)) / (y))

static void get_next_entry(objectlog_t *log, multiring_ptr_t *offset) {
	uint8_t fragment_hdr;

	log->multiring.ptr_read = *offset;
	do {
		fragment_hdr = multiring_read_one(&log->multiring);
		multiring_advance_read(&log->multiring, FRAGMENT_LEN(fragment_hdr));
		/*
		 * FIXME:
		 * There might be no terminating entry in the list. Detect
		 * whether we have wrapped across @offset and terminate if
		 * we did
		 */
	} while (!(fragment_hdr & FRAGMENT_FINAL) && FRAGMENT_LEN(fragment_hdr));

	*offset = log->multiring.ptr_read;
}

static uint16_t objectlog_space_between(objectlog_t *log,
					multiring_ptr_t *first,
					multiring_ptr_t *second) {
	return multiring_byte_delta(&log->multiring, first, second);
}

static void drop_first_entry(objectlog_t *log) {
	get_next_entry(log, &log->ptr_first);
	log->num_entries--;
}

static uint16_t objectlog_free_space(objectlog_t *log, multiring_ptr_t *from) {
	return objectlog_space_between(log, from, &log->ptr_first);
}

static void objectlog_write_fragment_hdr(objectlog_t *log, uint16_t len, bool final) {
	uint8_t hdr = FRAGMENT_LEN(len);

	if (final) {
		hdr |= FRAGMENT_FINAL;
	}
	multiring_write_one(&log->multiring, hdr);
}

static void objectlog_write_fragment_data(objectlog_t *log, const void *data, uint16_t len) {
	multiring_write(&log->multiring, data, len);
}

int objectlog_init_fragmented(objectlog_t *log, const scatter_object_t *storage) {
	int err;

	err = multiring_init(&log->multiring, storage);
	if (err) {
		return err;
	}
	multiring_memset(&log->multiring, 0, log->multiring.size);

	log->ptr_first = log->multiring.ptr_read;
	log->ptr_last = log->multiring.ptr_read;
	log->num_entries = 0;
	return 0;
}

int objectlog_init(objectlog_t *log, void *storage, uint16_t size) {
	scatter_object_t scatter_storage[] = {
		{ .ptr = storage, .len = size },
		{ .len = 0 },
	};

	return objectlog_init_fragmented(log, scatter_storage);
}

/**
 * Write object from non-contiguous memory area to object log
 * Oftentimes data that needs to be stored is not available from a contiguous
 * memory region. This method accepts a list of (pointer, length) pairs and
 * constructs the object to be stored by iterating over it. In each iteration
 * @length bytes read from @pointer are appended to the object log.
 *
 * @returns: 0 on success, number of bytes missing for storage on failure
 */
uint16_t objectlog_write_scattered_object(objectlog_t *log, const scatter_object_t *scatter_list)
{
	const scatter_object_t *sc_list = scatter_list;
	const uint8_t *data8;
	uint16_t num_fragments;
	uint16_t data_len = 0;
	uint16_t total_len;
	multiring_ptr_t new_last;
	uint16_t free_space;
	multiring_ptr_t log_end = log->ptr_last;
	uint16_t scatter_entry_offset = 0;
	uint16_t fragment_offset = 0;
	uint16_t fragment_len;

	/* Calculate total length of all data in @scatter_list */
	data_len = scatter_list_size(sc_list);

	/* Calculate number of fragments required to store data */
	num_fragments = DIV_ROUND_UP(data_len, MAX_FRAGMENT_LEN);
	total_len = data_len + num_fragments;
	/* FIXME: assume safe maximum for number of extra headers from wraps */
	total_len += log->multiring.num_storage;
	/* We can not store any messages exceeding size of this buffer */
	if (total_len > log->multiring.size) {
		return total_len - log->multiring.size;
	}

	/* Get number of bytes not in use at the moment */
	get_next_entry(log, &log_end);
	free_space = objectlog_free_space(log, &log_end);
	/* Delete entries from start of list until object fits */
	while (free_space < total_len /* && log->ptr_first != log->ptr_last */) {
		drop_first_entry(log);
		free_space = objectlog_free_space(log, &log_end);
	}

	/* Store start of object header */
	new_last = log->multiring.ptr_write;

	/* Write object as @num_fragments fragments */
	sc_list = scatter_list;
	data8 = sc_list->ptr;
	while(sc_list->len) {
		uint16_t write_len = data_len;

		/* Calculate maximum permissible size for this fragment */
		if (!fragment_offset) {
			fragment_len = MAX_FRAGMENT_LEN;
			/* Ensure fragment does not wrap in ring buffer */
			if (fragment_len > multiring_available_contiguous(&log->multiring.ptr_write) - 1) {
				fragment_len = multiring_available_contiguous(&log->multiring.ptr_write) - 1;
			}
		}

		/* Limit write size to fragment length */
		if (write_len + fragment_offset > fragment_len) {
			write_len = fragment_len - fragment_offset;
		}

		/* Write fragment header whenever we start a new fragment */
		if (!fragment_offset) {
			objectlog_write_fragment_hdr(log, write_len, data_len == write_len);
		}

		/* Limit length of this write to scatter entry length */
		if (write_len > sc_list->len - scatter_entry_offset) {
			write_len = sc_list->len - scatter_entry_offset;
		}

		objectlog_write_fragment_data(log, data8, write_len);
		data8 += write_len;
		data_len -= write_len;
		scatter_entry_offset += write_len;
		fragment_offset += write_len;

		/* Switch to next scatter entry if there is no more data in current one */
		if (scatter_entry_offset >= sc_list->len) {
			sc_list++;
			data8 = sc_list->ptr;
			scatter_entry_offset = 0;
		}
		/* Start new fragment if there is no more space in current one */
		if (fragment_offset >= fragment_len) {
			fragment_offset = 0;
		}
	}
	log->ptr_last = new_last;
	log->num_entries++;

	return 0;
}

uint16_t objectlog_write_object(objectlog_t *log, const void *data, uint16_t len) {
	scatter_object_t scatter_list[] = {
		/* Cast to non-const for compatibility, still never written */
		{ .ptr = (void *)data, .len = len },
		{ .len = 0 }
	};

	return objectlog_write_scattered_object(log, scatter_list);
}

uint16_t objectlog_write_string(objectlog_t *log, const char *str) {
	return objectlog_write_object(log, str, strlen(str));
}

/**
 * Obtain iterator for object at index @object_idx
 * Negative indices count from last to first string.
 *
 * @returns: non-negative iterator value on success, -1 on failure
 */
void objectlog_iterator(objectlog_t *log, int object_idx,
			objectlog_iterator_t *iterator) {
	multiring_ptr_t object_ptr = log->ptr_first;

	if (object_idx < 0) {
		object_idx = -object_idx;
		if (object_idx >= log->num_entries) {
			iterator->storage = NULL;
			return;
		}
		object_idx = log->num_entries - object_idx;
	} else {
		if (object_idx >= log->num_entries) {
			iterator->storage = NULL;
			return;
		}

	}

	while (object_idx--) {
		get_next_entry(log, &object_ptr);
	}

	*iterator = object_ptr;
}

/**
 * Get current iteration fragment
 *
 * @returns: non-NULL pointer to data on success, NULL on failure
 */
const void *objectlog_get_fragment(objectlog_t *log,
				   objectlog_iterator_t *iterator,
				   uint8_t *len) {
	if (objectlog_iterator_is_err(iterator)) {
		return NULL;
	}

	log->multiring.ptr_read = *iterator;

	*len = FRAGMENT_LEN(multiring_read_one(&log->multiring));
	return ((uint8_t*)log->multiring.ptr_read.storage->ptr) +
		log->multiring.ptr_read.offset;
}

/**
 * Advance iterator to next fragment
 *
 */
void objectlog_next(objectlog_t *log, objectlog_iterator_t *iterator) {
	uint16_t len;
	uint8_t hdr;

	if (objectlog_iterator_is_err(iterator)) {
		return;
	}

	log->multiring.ptr_read = *iterator;
	hdr = multiring_read_one(&log->multiring);
	if (hdr & FRAGMENT_FINAL) {
		iterator->storage = NULL;
		return;
	}
	len = FRAGMENT_LEN(hdr);

	multiring_advance_read(&log->multiring, len);
	*iterator = log->multiring.ptr_read;
}

/**
 * Get size of object at index @object_idx
 *
 * @returns: -1 on failure, else
 *	     non-negative length of object
 */
long objectlog_get_object_size(objectlog_t *log, int object_idx) {
	objectlog_iterator_t iter;
	uint16_t len = 0;

	objectlog_iterator(log, object_idx, &iter);
	if (objectlog_iterator_is_err(&iter)) {
		return -1;
	}
	while (!objectlog_iterator_is_err(&iter)) {
		uint8_t fragment_size;

		if (!objectlog_get_fragment(log, &iter, &fragment_size)) {
			return -1;
		}
		len += fragment_size;
		objectlog_next(log, &iter);
	}

	return len;
}
