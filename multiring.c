#include <string.h>

#include "multiring.h"

#define ALIGN_UP(x, align) ((x) + ((align) - (x) % (align)))

int multiring_init(multiring_t *multiring, const scatter_object_t *storage) {
	const scatter_object_t *sc_entry = storage;
	unsigned int sc_max_entry_idx = 0;
	scatter_object_t *sc_list_copy;
	unsigned int num_storage_area = 0;
	uint16_t storage_list_size;
	uint16_t storage_size = 0;
	uint16_t storage_list_offset;

	/*
	 * Determine three parameters:
	 *  - largest scatter list entry
	 *  - accumulated size of all scatter list enties
	 *  - number of scatter list entries
	 */
	while (sc_entry->len) {
		if (sc_entry->len > storage[sc_max_entry_idx].len) {
			sc_max_entry_idx = num_storage_area;
		}
		storage_size += sc_entry->len;
		num_storage_area++;
		sc_entry++;
	}
	/* Fail if there are no scatter list enties */
	if (!num_storage_area) {
		return -1;
	}

	multiring->num_storage = num_storage_area;
	storage_list_size = (num_storage_area + 1) * sizeof(scatter_object_t);
	storage_list_offset = ALIGN_UP(storage_list_size, 8);

	/* Ensure largest scatterlist entry can store a copy of the scatter list */
	if (storage[sc_max_entry_idx].len <= storage_list_offset) {
		return -1;
	}

	/* Create copy of the scatter list */
	sc_list_copy = storage[sc_max_entry_idx].ptr;
	memcpy(sc_list_copy, storage, storage_list_size);
	sc_list_copy[sc_max_entry_idx].ptr =
		((uint8_t*)sc_list_copy[sc_max_entry_idx].ptr) + storage_list_offset;
	sc_list_copy[sc_max_entry_idx].len -= storage_list_offset;
	storage_size -= storage_list_offset;
	/* Point storeage to scatter list copy */
	multiring->storage = sc_list_copy;
	multiring->size = storage_size;

	/* Set pointers to start of storage */
	multiring->ptr_read.storage = sc_list_copy;
	multiring->ptr_read.offset = 0;
	multiring->ptr_write.storage = sc_list_copy;
	multiring->ptr_write.offset = 0;
	return 0;
}

void multiring_next_ring(multiring_t *multiring, multiring_ptr_t *ptr) {
	const scatter_object_t *storage = ptr->storage;

	storage++;
	if (!storage->len) {
		storage = multiring->storage;
	}
	ptr->storage = storage;
	ptr->offset = 0;
}

void multiring_advance(multiring_t *multiring, multiring_ptr_t *ptr,
		       uint16_t count) {
	multiring_ptr_t ring_ptr = *ptr;

	while (count) {
		uint16_t space_ring =
			multiring_available_contiguous(&ring_ptr);

		if (space_ring > count) {
			ring_ptr.offset += count;
			break;
		}
		count -= space_ring;
		multiring_next_ring(multiring, &ring_ptr);
	}

	*ptr = ring_ptr;
}

void multiring_write(multiring_t *multiring, const void *data, uint16_t len) {
	const uint8_t *data8 = data;

	while (len) {
		uint16_t write_size = len;
		uint16_t space_avail =
			multiring_available_contiguous(&multiring->ptr_write);

		if (write_size > space_avail) {
			write_size = space_avail;
		}

		memcpy(((uint8_t*)multiring->ptr_write.storage->ptr) + multiring->ptr_write.offset,
		       data8, write_size);
		multiring_advance_write(multiring, write_size);

		len -= write_size;
		data8 += write_size;
	}
}

void multiring_read(multiring_t *multiring, void *data, uint16_t len) {
	uint8_t *data8 = data;

	while (len) {
		uint16_t read_size = len;
		uint16_t space_avail =
			multiring_available_contiguous(&multiring->ptr_read);

		if (read_size > space_avail) {
			read_size = space_avail;
		}

		memcpy(data8,
		       ((uint8_t*)multiring->ptr_read.storage->ptr) + multiring->ptr_read.offset,
		       read_size);
		multiring_advance_read(multiring, read_size);

		len -= read_size;
		data8 += read_size;
	}
}

uint16_t multiring_num_wraps(multiring_t *multiring, uint16_t len) {
	uint16_t num_wraps = 0;
	multiring_ptr_t ptr = multiring->ptr_write;

	while (len) {
		uint16_t chunk_size = len;
		uint16_t space_avail = multiring_available_contiguous(&ptr);

		if (chunk_size > space_avail) {
			chunk_size = space_avail;
		}

		if (chunk_size != len) {
			num_wraps++;
		}

		multiring_advance(multiring, &ptr, chunk_size);
		len -= chunk_size;
	}

	return num_wraps;
}

void multiring_memset(multiring_t *multiring, uint8_t val, uint16_t len) {
	while (len) {
		uint16_t write_size = len;
		uint16_t space_avail =
			multiring_available_contiguous(&multiring->ptr_write);

		if (write_size > space_avail) {
			write_size = space_avail;
		}

		memset(((uint8_t*)multiring->ptr_write.storage->ptr) + multiring->ptr_write.offset,
		       val, write_size);
		multiring_advance_write(multiring, write_size);

		len -= write_size;
	}
}

uint16_t multiring_byte_delta(multiring_t *multiring,
			      multiring_ptr_t *first,
			      multiring_ptr_t *second) {
	uint16_t len;
	multiring_ptr_t ptr = *first;

	/* Simple cases: first and second are from the same scatter list entry */
	if (first->storage == second->storage) {
		if (first->offset <= second->offset) {
			return second->offset - first->offset;
		}
		return multiring->size - (first->offset - second->offset);
	}

	/* Complex case: first and second in different scatter list entries */
	len = first->storage->len - first->offset;
	do {
		multiring_next_ring(multiring, &ptr);
		len += ptr.storage->len;
	} while (ptr.storage != second->storage);
	len -= (second->storage->len - second->offset);

	return len;
}

