/**
@file list.h

Generic list management macros. These (preprocessor) macros will directly
work with pointers to data structures (`mystruct*`), and operate on them as
a double-linked list.

The only requirement is that your structure provides two `(void*)` pointers
`prev` and `next`, which indicate the previous and next elements in the list.
The list itself is referred to (and kept track of) by a pointer to the first
element (`head`).

@note These macros are "raw meat", and don't try to prevent you from doing
silly things - like passing `NULL` pointers or breaking the pointer chain
(e.g. by appending the same element twice, or deleting elements not contained
within the list).
If you need to safeguard against this, do so in your own code!
*/
/* ---------------------------------------------------------------------------
Copyright 2015 by the Lucciefr team

2015-08-13 [BNO] adapted to Lucciefr (and doxygen comments)
2013-05-01 [BNO] initial version
*/

#ifndef LIST_H
#define LIST_H

/// Iterate over the list, starting at head and moving 'forward'.
/// @code{.c} mylist *element; LIST_ITERATE(element, head) { /* do something */ } @endcode
#define LIST_ITERATE(element, head) \
	for (element = head; element; element = element->next)

/// Find the last element in the list.
#define LIST_LAST(element, head) \
	do { element = head; \
		while (element) { if (!element->next) break; element = element->next; } \
	} while (0)

/// Iterate over the list in 'reverse' order, starting at end and moving 'backwards'.
/// @code{.c} mylist *element; LIST_ITERATE_REVERSED(element, head) { /* do something */ } @endcode
/**
@warning This two-statement macro is **not encapsulated in a block** of its
own! (The `for` loop prevents that, and syntactic reasons won't allow macro
expansion/assignment of `element` via the initializer expression in a single
statement.) Be careful to avoid situations where that might lead to problems:
@code{.c}
// WRONG!
if (bad_idea) LIST_ITERATE_REVERSED(foo, bar) do_something();

// RIGHT:
if (good_boy) {
	LIST_ITERATE_REVERSED(foo, bar) do_something();
} @endcode
*/
#define LIST_ITERATE_REVERSED(element, head) \
	LIST_LAST(element, head); \
	for (; element; element = element->prev)

/// Find the first match to a specific condition, returns either an element
/// pointer or `NULL`. This is simply done by testing `condition` while
/// (partially) iterating the list.
#define LIST_MATCH(element, head, condition) \
	do { \
		LIST_ITERATE(element, head) { if (condition) break; } \
	} while (0)

/// Find a specific element pointer (the first one matching "value") within the list.
#define LIST_FIND(element, head, value) \
	LIST_MATCH(element, head, element == value)

/// Count the elements in the list.
#define LIST_COUNT(counter, head) \
	do { __typeof__ (head) LIST_element; \
		counter = 0; LIST_ITERATE(LIST_element, head) { counter++; } \
	} while (0)

/// Append a new element to the list (i.e. add at the end).
#define LIST_APPEND(element, head) \
	do { __typeof__ (head) LIST_last; \
		LIST_LAST(LIST_last, head); /* find last element */ \
		element->prev = LIST_last; \
		element->next = NULL; \
		if (LIST_last) LIST_last->next = element; \
		else /* should only happen on (head == NULL) */ head = element; \
	} while (0)

/// Delete an element from the list.
#define LIST_DELETE(element, head) \
	do { __typeof__ (head) LIST_prev = element->prev; \
		__typeof__ (head) LIST_next = element->next; \
		if (LIST_next) LIST_next->prev = LIST_prev; \
		if (LIST_prev) LIST_prev->next = LIST_next; \
		else /* should only happen on (element == head) */ head = LIST_next; \
	} while (0)

#endif // LIST_H
