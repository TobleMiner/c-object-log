#pragma once

#include <stddef.h>
#include <stdint.h>

typedef size_t scatter_size_t;

typedef struct {
	void *ptr;
	scatter_size_t len;
} scatter_object_t;

static inline scatter_size_t scatter_list_size(const scatter_object_t *sc_list) {
	scatter_size_t len = 0;

	while(sc_list->len) {
		len += sc_list++->len;
	}

	return len;
}
