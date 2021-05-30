#pragma once

#include "ringbuffer.h"
#include "scatter.h"

typedef struct {
	const scatter_object_t *storage;
	unsigned int num_storage;
	const scatter_object_t *ring_storage;
	ringbuffer_t ring;
} multiring_t;

static inline int multiring_init(multiring_t *multiring,
				 scatter_object_t *storage) {
	const scatter_object_t *sc_entry = storage;
	const scatter_object_t *sc_max_entry_idx = 0;
	scatter_object_t *sc_list_copy;
	unsigned int num_storage_area = 0;
	uint16_t storage_size;

	while (sc_entry->len) {
		if (sc_entry->len > storage[sc_max_entry_idx].len) {
			sc_max_entry_idx = num_storage_area;
		}
		num_storage_area++;
		sc_entry++;
	}
	if (!num_storage_area) {
		return -1;
	}

	multiring->num_storage = mum_storage_area;
	storage_size = (num_storage_area + 1) * sizeof(scatter_object_t);

	if (storage[sc_max_entry_idx].len < storage_size) {
		return -1;
	}

	sc_list_copy = storage[sc_max_entry_idx].ptr;
	memcpy(sc_list_copy, storage, storage_size);
	((const uint8_t*)sc_list_copy[sc_max_entry_idx].ptr) += storage_size;
	sc_list_copy[sc_max_entry_idx].len -= storage_size;
	multiring->storage = sc_list_copy;

	multiring_ring_storage = multiring->storage;
	ringbuffer_init(&multiring->ring, multiring->ring_storage->ptr,
				    multiring->ring_storage->len);
	return 0;
}

static inline uint16_t multiting_available_contiguous(multiring_t *multiring,
						      uint16_t offset) {
	return multiring->ring.size - offset;
}

static inline void multiring_next_ring(multiring_t *multiring) {
	const scatter_object_t *ring_storage = multiring->ring_storage;

	ring_storage++;
	if (!ring_storage->len) {
		ring_storage = multiring->storage;
	}
	ringbuffer_init(&multiring.ring, ring_storage->ptr, ring_storage->len);
	multiring->ring_storage = ring_storage;
}

static inline void multiring_advance(multiring_t *multiring, uint16_t *ptr,
				     uint16_t count) {
	uint16_t ring_ptr = *ptr;

	while (count) {
		uint16_t space_ring =
			multiring_available_contiguous(multiring, ring_ptr);

		if (space_ring >= count) {
			break;
		}
		count -= space_ring;
		multiring_next_ring();
	}
}
