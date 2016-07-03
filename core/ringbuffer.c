/**
@file ringbuffer.c

Code to manage "ring" buffer logic.

A ring buffer is a 'cyclic' list of pointers that will normally be used as a
"queue" / FIFO stack (first in, first out).

We're assuming that all elements in the list have been allocated in a uniform
way, and that it may be necessary to call a specific "free" function before
discarding them (to release any associated memory).
E.g. for elements allocated via stdlib `malloc` or `calloc`,
the corresponding function would of course be `free`.

Removing elements from the list happens on two occasions: either with a
regular "pop", or in case you try to "push" a new element to a ring buffer
that is currently full. (The latter will overwrite the oldest entry to make
room for the new one.)

With "tail" we refer to the oldest element currently in the buffer (the one
that was pushed earliest), and "head" means the most recent one (the 'top'
element pushed last). The ring buffer keeps track of the number of
elements and their associated pointers, as long as the element count doesn't
exceed the capacity (maximum count = "size" of the buffer). As mentioned
above, pushing a new element to a buffer that's full will overwrite the
current "tail" (i.e. drop the oldest entry).
*/
/* ---------------------------------------------------------------------------
Copyright 2015 by the Lucciefr team

2015-08-11 [BNO] initial version
*/

// TODO: multithreading / thread safety

#include "ringbuffer.h"

#include <stdlib.h>

/** "constructor", prepare a ringbuffer_t before usage
@param rb the ring buffer to use
@param capacity maximum number of elements (pointers) the buffer can hold
@param free_func
free function to use on an element before discarding it. optional, may be `NULL`
*/
void ringbuffer_init(ringbuffer_t *rb, size_t capacity, free_func_t free_func) {
	rb->entries = NULL;
	rb->position = 0;
	rb->count = 0;
	rb->capacity = 0;
	if (capacity > 0) {
		rb->entries = calloc(capacity, sizeof(void*));
		if (rb->entries) rb->capacity = capacity;
	}
	rb->free = free_func;
}

/// "destructor", free up a ringbuffer_t's resources after you're done
void ringbuffer_done(ringbuffer_t *rb) {
	ringbuffer_clear(rb);
	rb->capacity = 0;
	free(rb->entries);
	rb->entries = NULL;
}

// private function to increment the "tail" position,
// so that it references the next entry in the ring buffer
static void ringbuffer_inc_tail(ringbuffer_t *rb) {
	rb->position++;
	// fix position if we ran past the "buffer wrap"
	if (rb->position >= rb->capacity) rb->position = 0;
}

// private function free up the element (entry) at given index
static void ringbuffer_free(ringbuffer_t *rb, size_t index) {
	if (index >= rb->capacity) index -= rb->capacity;
	if (rb->free) rb->free(rb->entries[index]); // release element via "free" function
	rb->entries[index] = NULL;
}

// private function to assign a new element pointer to a given 'slot'
static void ringbuffer_set(ringbuffer_t *rb, size_t index, void *element) {
	if (index >= rb->capacity) index -= rb->capacity;
	ringbuffer_free(rb, index);
	rb->entries[index] = element;
}

/// push an element pointer to the ring buffer (new "head" entry)
void ringbuffer_push(ringbuffer_t *rb, void *element) {
	if (element) { // (ignore NULL element)
		if (rb->count < rb->capacity) {
			// We still have room in the buffer - so we'll simply add the element
			// (by storing it as new "head" entry), incrementing the count.
			ringbuffer_set(rb, rb->position + rb->count, element);
			rb->count++;
		} else {
			// The ring buffer is full! This means we have to overwrite the
			// oldest entry (tail), losing it in the process. The list count
			// stays unchanged, but the tail pointer moves.
			ringbuffer_set(rb, rb->position, element); // overwrite "tail"
			ringbuffer_inc_tail(rb);
		}
	}
}

/// discard the oldest entry ("tail" element) from the buffer.
/// This decrements count (i.e. frees up one 'slot'), and moves the tail pointer.
void ringbuffer_pop(ringbuffer_t *rb) {
	if (rb->count > 0) {
		rb->count--;
		ringbuffer_free(rb, rb->position); // "position" is our tail pointer!
		ringbuffer_inc_tail(rb);
	}
}

/// remove (pop) all entries from the buffer, until it's empty again (count == 0)
void ringbuffer_clear(ringbuffer_t *rb) {
	while (rb->count > 0) ringbuffer_pop(rb);
}

/** resize the ring buffer (assign a new capacity).
This is done by allocating some new "entries" memory, copying over all
existing pointers using ringbuffer_element() and then replacing the old
"entries" with the new pointers memory. "position" gets reset to 0.
With sufficient new capacity the buffer count stays unchanged, otherwise
an appropriate amount will be discarded ("pop"ping the oldest elements).
*/
void ringbuffer_resize(ringbuffer_t *rb, size_t new_capacity) {
	if (new_capacity > 0) {
		void* *new_entries = calloc(new_capacity, sizeof(void*));
		if (!new_entries) return; // (failed to allocate memory)
		while (rb->count > new_capacity) ringbuffer_pop(rb); // trim count
		size_t i;
		for (i = 0; i < rb->count; i++)
			new_entries[i] = ringbuffer_element(rb, i);
		free(rb->entries);
		rb->entries = new_entries;
		rb->position = 0;
	}
}

/** Retrieve the n-th entry (element pointer) from the ring buffer.
Any invalid index will result in a `NULL` return value. Specifically, with
an empty buffer you'll always get back `NULL` (for any value of index).

@note Here the `index` parameter is *relative to the current buffer
position* - meaning `ringbuffer_element(rb, 0)` will always refer to the
"tail", and `ringbuffer_element(rb, count - 1)` is the "head" entry.
*/
void *ringbuffer_element(ringbuffer_t *rb, size_t index) {
	if (index < rb->count) {
		index += rb->position; // start at "tail"
		if (index >= rb->capacity) index -= rb->capacity; // check for 'wrap'
		return rb->entries[index];
	}
	return NULL;
}

/// shortcut to retrieve "tail" element
/// @note non-destructive, i.e. this does _not_ pop the element!
/// @see ringbuffer_pop
inline void *ringbuffer_tail(ringbuffer_t *rb) {
	return ringbuffer_element(rb, 0);
}

/// shortcut to retrieve "head" element
inline void *ringbuffer_head(ringbuffer_t *rb) {
	return ringbuffer_element(rb, rb->count - 1);
}
