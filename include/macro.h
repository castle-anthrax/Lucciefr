/// @file macro.h

#ifndef MACRO_H
#define MACRO_H

/* utility macros */

/// this macro is useful to get rid of compiler warnings
#ifndef UNUSED
	#define UNUSED(x)	(void)(x)
#endif

/// macro to retrieve "length" (= number of elements) of an array
#define lengthof(array)	(sizeof(array) / sizeof(array[0]))

/** start a `TRY` block.
`TRY { ... } FINALLY` is syntactic sugar for creating an explicit
`do { ... } while (0);` block. The idea is that such a block can be left with
a `break` statement in case of errors, to properly handle cleaning up etc.

	stuff = malloc(some_size);
	TRY {
		do_something_with_stuff();
		if (error) break;
		do_more_stuff();
	} FINALLY
	free(stuff);
*/
#define TRY		do
/// close a `TRY` block
#define FINALLY	while (0);


/// maximum of (x, y)
#ifndef MAX
	#define MAX(x, y)	((x) > (y) ? (x) : (y))
#endif
/// minimum of (x, y)
#ifndef MIN
	#define MIN(x, y)	((x) < (y) ? (x) : (y))
#endif
/// absolute value of `a`
#ifndef ABS
	#define ABS(a)		((a) < 0 ? -(a) : (a))
#endif

#endif // MACRO_H
