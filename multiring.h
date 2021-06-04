#pragma once

#include "scatter.h"

typedef struct {
	const scatter_object_t *storage;
	uint16_t offset;
} multiring_ptr_t;

typedef struct {
	const scatter_object_t *storage;
	unsigned int num_storage;
	multiring_ptr_t ptr_read;
	multiring_ptr_t ptr_write;
	uint16_t size;
} multiring_t;

int multiring_init(multiring_t *multiring, const scatter_object_t *storage);
void multiring_next_ring(multiring_t *multiring, multiring_ptr_t *ptr);
void multiring_advance(multiring_t *multiring, multiring_ptr_t *ptr,
		       uint16_t count);
void multiring_write(multiring_t *multiring, const void *data, uint16_t len);
void multiring_read(multiring_t *multiring, void *data, uint16_t len);
uint16_t multiring_num_wraps(multiring_t *multiring, uint16_t len);
void multiring_memset(multiring_t *multiring, uint8_t val, uint16_t len);
uint16_t multiring_byte_delta(multiring_t *multiring,
			      multiring_ptr_t *first,
			      multiring_ptr_t *second);

static inline uint16_t multiring_available_contiguous(const multiring_ptr_t *ptr) {
	return ptr->storage->len - ptr->offset;
}

static inline void multiring_advance_read(multiring_t *multiring,
					   uint16_t count) {
	multiring_advance(multiring, &multiring->ptr_read, count);
}

static inline void multiring_advance_write(multiring_t *multiring,
					   uint16_t count) {
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

static inline uint16_t multiring_available(multiring_t *multiring) {
	return multiring_byte_delta(multiring, &multiring->ptr_read,
				     &multiring->ptr_write);
}
