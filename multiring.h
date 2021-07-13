#pragma once

#include "scatter.h"

typedef struct {
	const scatter_object_t *storage;
	scatter_size_t offset;
} multiring_ptr_t;

typedef struct {
	const scatter_object_t *storage;
	unsigned int num_storage;
	multiring_ptr_t ptr_read;
	multiring_ptr_t ptr_write;
	scatter_size_t size;
} multiring_t;

int multiring_init(multiring_t *multiring, const scatter_object_t *storage);
void multiring_next_ring(multiring_t *multiring, multiring_ptr_t *ptr);
void multiring_advance(multiring_t *multiring, multiring_ptr_t *ptr,
		       scatter_size_t count);
void multiring_write(multiring_t *multiring, const void *data, scatter_size_t len);
void multiring_read(multiring_t *multiring, void *data, scatter_size_t len);
scatter_size_t multiring_num_wraps(multiring_t *multiring, scatter_size_t len);
void multiring_memset(multiring_t *multiring, uint8_t val, scatter_size_t len);
scatter_size_t multiring_byte_delta(multiring_t *multiring,
			      multiring_ptr_t *first,
			      multiring_ptr_t *second);

static inline scatter_size_t multiring_available_contiguous(const multiring_ptr_t *ptr) {
	return ptr->storage->len - ptr->offset;
}

static inline void multiring_advance_read(multiring_t *multiring,
					   scatter_size_t count) {
	multiring_advance(multiring, &multiring->ptr_read, count);
}

static inline void multiring_advance_write(multiring_t *multiring,
					   scatter_size_t count) {
	multiring_advance(multiring, &multiring->ptr_write, count);
}

static inline uint8_t multiring_read_one(multiring_t *multiring) {
	uint8_t datum;

	multiring_read(multiring, &datum, 1);
	return datum;
}

static inline void multiring_write_one(multiring_t *multiring, uint8_t datum) {
	multiring_write(multiring, &datum, 1);
}

static inline scatter_size_t multiring_available(multiring_t *multiring) {
	return multiring_byte_delta(multiring, &multiring->ptr_read,
				     &multiring->ptr_write);
}

static inline int multiring_ptr_cmp(multiring_ptr_t *a, multiring_ptr_t *b) {
	return a->storage != b->storage ||
	       a->offset != b->offset;
}
