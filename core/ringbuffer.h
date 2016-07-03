/// @file ringbuffer.h

#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stddef.h> // size_t

/// prototype for a "free" function (used as ringbuffer_t member)
typedef void free_func_t(void *ptr);

/// Ring buffer structure
/// @see ringbuffer.c
typedef struct {
	void* *entries;		///< dynamic memory for an array of element pointers
	size_t position;	///< the current start of the list (oldest entry, "tail" pointer)
	size_t count;		///< the number of elements in the buffer (0 -> empty; == capacity -> full)
	size_t capacity;	///< maximum number of elements, should correspond to *entries array size
	free_func_t *free;	///< (optional) function to release memory on element removal
} ringbuffer_t;

void ringbuffer_init(ringbuffer_t *rb, size_t capacity, free_func_t free_func);
void ringbuffer_done(ringbuffer_t *rb);

void ringbuffer_push(ringbuffer_t *rb, void *element);
void ringbuffer_pop(ringbuffer_t *rb);
void ringbuffer_clear(ringbuffer_t *rb);
void ringbuffer_resize(ringbuffer_t *rb, size_t new_capacity);
void *ringbuffer_element(ringbuffer_t *rb, size_t index);
void *ringbuffer_tail(ringbuffer_t *rb);
void *ringbuffer_head(ringbuffer_t *rb);

#endif // RINGBUFFER_H
