#pragma once

#include <stdint.h>

typedef struct {
	void *ptr;
	uint16_t len;
} scatter_object_t;

static inline uint16_t scatter_list_size(const scatter_object_t *sc_list) {
	uint16_t len = 0;

	while(sc_list->len) {
		len += sc_list++->len;
	}

	return len;
}
