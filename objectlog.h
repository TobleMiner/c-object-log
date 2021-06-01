#pragma once

#include <stddef.h>
#include <stdint.h>

#include "multiring.h"
#include "scatter.h"

typedef struct {
	multiring_t multiring;
	multiring_ptr_t ptr_first;
	multiring_ptr_t ptr_last;
	unsigned int num_entries;
} objectlog_t;

typedef multiring_ptr_t objectlog_iterator_t;

int objectlog_init(objectlog_t *log, void *storage, uint16_t size);
int objectlog_init_fragmented(objectlog_t *log, scatter_object_t *storage);
uint16_t objectlog_write_object(objectlog_t *log, const void *data, uint16_t len);
uint16_t objectlog_write_scattered_object(objectlog_t *log, const scatter_object_t *scatter_list);
uint16_t objectlog_write_string(objectlog_t *log, const char *str);
void objectlog_iterator(objectlog_t *log, int object_idx, objectlog_iterator_t *iterator);
const void *objectlog_get_fragment(objectlog_t *log, objectlog_iterator_t *iterator, uint8_t *len);
void objectlog_next(objectlog_t *log, objectlog_iterator_t *iterator);
long objectlog_get_object_size(objectlog_t *log, int object_idx);

static inline int objectlog_iterator_is_err(objectlog_iterator_t *iterator) {
	return !iterator->storage;
}
