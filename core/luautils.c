// luautils.c
// A collection of Lua helper functions

#include "luautils.h"

//#include "exceptions.h"
//#include "globals.h"
#include "log.h"
#include "macro.h"
#include "strutils.h"
#include "symbols.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>


// TODO: FIX ME!
#define FMTPTR	"%p"

// Lua key -> value helpers (include actual implementation / function bodies)
#undef LUAUTILS_KV_HEADER_ONLY
#include "luautils_kv.inc"


// Given (optional) module name and function name, push the corresponding
// function to the Lua stack. Return value indicates success or failure,
// unless you pass (propagate == true) - the latter will cause actual Lua
// errors. (Note that luaL_error won't return.)
bool luautils_getfunction(lua_State *L, const char *module,
		char const *function, bool propagate)
{
	char err[256]; // error message buffer

	if (module) {
		lua_getglobal(L, module);
		if (!lua_istable(L, -1)) {
			snprintf(err, sizeof(err), "%s() module '%s' not found",
					__func__, module);
			if (propagate) luaL_error(L, err); else error(err);
			lua_pop(L, 1);
			return false;
		}
		lua_getfield(L, -1, function);
		if (lua_type(L, -1) != LUA_TFUNCTION) {
			snprintf(err, sizeof(err), "%s() function '%s.%s' not found",
					__func__, module, function);
			if (propagate) luaL_error(L, err); else error(err);
			lua_pop(L, 2);
			return false;
		}
		// we need to remove the module table, and keep only the function value
		lua_remove(L, -2);
		return true;
	}

	// no module name, look for a global function
	lua_getglobal(L, function);
	if (lua_type(L, -1) != LUA_TFUNCTION) {
		snprintf(err, sizeof(err), "%s() global function '%s' not found",
				__func__, function);
		if (propagate) luaL_error(L, err); else error(err);
		lua_pop(L, 1);
		return false;
	}
	return true;
}

// create Lua registry reference for a function, using luautils_getfunction()
int luautils_getfuncref(lua_State *L, const char *module, char const *function)
{
	if (luautils_getfunction(L, module, function, false))
		return luaL_ref(L, LUA_REGISTRYINDEX); // (also pops function from stack)

	return LUA_NOREF; // indicating failure
}

// safeguarded Lua execution

/*
 * A simple recovery for (hard) exceptions while executing Lua
 *
 * We'll make use of the setjmp/longjmp mechanism. In case of abnormal termination
 * (e.g. an access violation), lua_exception_recovery gets invoked and does a
 * longjmp to restore the previously saved environment.
 *
 * The handler uses __fastcall calling convention to ensure the parameters will be
 * passed "by register". ECX (RCX) relates to ExceptionRecord, EDX (RDX) is Context
 * (both are pointers to the respective Windows structures). These register values
 * get setup by the KiUserExceptionDispatcher_hook (see exceptions.c), which then
 * in turn points execution (EIP/RIP) to this function.
 */
jmp_buf lua_safeguard;
#if _WINDOWS
#include <windows.h>
void __attribute__((fastcall)) lua_exception_recovery(PEXCEPTION_RECORD ExceptionRecord,
		CONTEXT* Context)
{
	//__asm("INT3");
	longjmp(lua_safeguard, 666); // resume execution (at the point previously saved within lua_guarded_pcall)
}

#else
// We currently have no exception handling on Linux. Just fake the dispatcher...
void lua_exception_recovery(void) {}

#endif
void *USER_EXCEPTION_HANDLER; // NOT FUNCTIONAL, just a stub as long as exceptions.c is missing

/*
 * This function is similar to lua_pcall, but it tries to safeguard execution
 * of the Lua code. If our handler kicks in, we print an error message and a stack
 * dump. In that case the return value gets forced to LUA_ERRRUN. (Since Lua currently
 * always returns values >= 0, we could alternatively use -1 as a special marker.)
 *
 * Note: The USER_EXCEPTION_HANDLER is declared and called in exceptions.c
 */
int lua_guarded_pcall(lua_State *L, int nargs, int nresults, int errfunc) {
	/* The initial call to setjmp will save the execution state and return 0,
	   meaning it's okay to continue normally... */
	if (setjmp(lua_safeguard)) {
		/* ... but if setjmp returns non-zero, control flow was restored via
		   longjmp (from lua_exception_recovery). Lua execution has stopped
		   and shouldn't be resumed.
		   To avoid duplicating code we'll push an appropriate error message,
		   so we can simply call luaStackTrace_C afterwards. */
		USER_EXCEPTION_HANDLER = NULL; // remove/disable lua exception handler
		//error("lua_guarded_pcall() exception in underlying lua or native code! base=%p, self=%p", BASE, DLL_HANDLE);
		lua_pushliteral(L, "FATAL: Exception in underlying lua or native code!");
		luaStackTrace_C(L);
		return LUA_ERRRUN;
	}

	USER_EXCEPTION_HANDLER = &lua_exception_recovery; // place/enable lua exception handler
	int result = lua_pcall(L, nargs, nresults, errfunc);
	USER_EXCEPTION_HANDLER = NULL; // remove/disable lua exception handler
	return result;
}

// Alternative version of a safeguarded CFunction call, similar to lua_cpcall().
// However we keep the stack arguments intact / usable, and don't push an
// extra userdata pointer (so no changes are required to existing C functions).
// Internally, our function uses lua_pcall() and a custom error handler.

// This is the error handling function used by luautils_cpcall(). It's task
// is to retrieve the C function name (upvalue) from the C closure, and add
// that to the error message.
// (Upon entry, the stack will be empty, apart from the error message.)
LUA_CFUNC(cpcall_errfunc) {
	//error("%s(%s)", __func__, lua_tostring(L, -1));
	//error(lua_tostring(L, lua_upvalueindex(1)));
	lua_pushvalue(L, lua_upvalueindex(1)); // the C function name (upvalue)
	lua_pushliteral(L, "() error: ");
	lua_pushvalue(L, 1); // duplicate original error message
	lua_concat(L, 3); // (combine them to create the new message)
	return 1;
}

// This function is the lua_cpcall() counterpart. To be consistent with it,
// we will return `false` on successful execution, and `true` if an error
// occurs. For the latter, the function will push a suitable error message
// onto the Lua stack (including the function name).
//
// To be useful, this wrapper requires the name of the CFunction and the
// number of arguments it gets passed on the stack. (The latter is used
// to calculate a stack position where a custom error handler and the
// function itself will be inserted.)
//
bool luautils_cpcall(lua_State *L, lua_CFunction func, const char *fname,
		int nargs)
{
	//debug("%s(%p,%p,%d)", __func__, L, func, nargs);

	// We place an error handler before the CFunction arguments, associating
	// the function name with it at the same time (using an upvalue / C closure)
	lua_pushstring(L, fname);
	lua_pushcclosure(L, cpcall_errfunc, 1);
	int errfunc = lua_gettop(L) - nargs; // the handler's stack index
	if (errfunc <= 0) {
		luaL_error(L, "%s(%p,'%s',%d) invalid stack index for error handler, "
				"check your nargs", __func__, L, fname, nargs);
		return true;
	}
	lua_insert(L, errfunc);

	// Now insert the actual C function (at the stack position following the handler)
	lua_pushcfunction(L, func);
	lua_insert(L, errfunc + 1);

	int result = lua_guarded_pcall(L, nargs, LUA_MULTRET, errfunc);
	if (result) {
		debug("%s() intercepted Lua error %d, '%s'", __func__, result, lua_tostring(L, -1));
		return true;
	}

	lua_remove(L, errfunc); // remove the error handler from the stack
	return false; // no error
}

// checked luaL_dostring execution, will print a message if an error occurs
bool luautils_dostring (lua_State *L, const char *str) {
	bool result = (luaL_dostring(L, str) == 0);
	if (!result)
		error("%s(%p, %s) error: %s", __func__, L, str, lua_tostring(L, -1));
	return result;
}


/* LUA ADDITIONS */

// This function is supposed to handle both int32_t and uint32_t ranges
// gracefully, avoiding possible conversion / truncation issues with a
// 'direct' typecast to int32_t.
// The result may also be casted to uint32_t (see luautils_touint32 macro).
inline int32_t luautils_toint32(lua_State *L, int idx) {
	// Mike Pall strongly opposed the idea of doing this globally via
	// lj_num2int(), see https://github.com/LuaJIT/LuaJIT/issues/31
	lua_Number n = lua_tonumber(L, idx);
	if (n > 2147483647.0) n -= 4294967296.0;
	return (int32_t)n;
}

// Use this if you already expect a uint32_t number (n >= 0 && n < 2**32)
// (implicit conversion, might fail on negative numbers)
inline uint32_t luautils_asuint32(lua_State *L, int idx) {
	return (uint32_t)lua_tonumber(L, idx);
}

// clean up the stack of a Lua state using a basepointer
//
// basepointer < 0	leaves stack unchanged (no adjustment)
// basepointer = 0	removes anything from stack
// basepointer > 0	sets new stack size / top
inline void luautils_stackclean(lua_State *L, int basepointer) {
	if (L && (basepointer >= 0)) lua_settop(L, basepointer);
}

// a standard way to push a pointer to the Lua stack
// This function tests the pointer for a non-NULL value;
// such pointers will be transferred as light userdata.
// However, if (ptr == NULL) we'll push `nil` instead,
// which allows for simpler checking of the result in Lua.
// (as `userdata:NULL` evaluates to `true`, while `nil` doesn't)
inline void luautils_pushptr(lua_State *L, const void *ptr) {
	if (ptr)
		lua_pushlightuserdata(L, (void*)ptr);
	else
		lua_pushnil(L);
}

// push wide string, automatically converted to standard Lua string
// via a call to "wchar2char"
void luautils_pushwstring (lua_State *L, const wchar_t *s, int len) {
	if (s) {
		if (len < 0)
			len = wcslen(s); // retrieve length from string

		if (len > 0) {
			luautils_getfunction(L, NULL, "wchar2char", true);
			lua_pushlstring(L, (char*)s, len + len);
			lua_call(L, 1, 1);
		}
		else lua_pushliteral(L, ""); // empty string
	}
	else lua_pushnil(L); // s is NULL, push `nil`
}

/* Retrieve address and length for a buffer. The argument at the given index
 * may either be a Lua string (which implies a length), or a pointer type
 * followed by an explicit length. The function sets *len accordingly, and
 * returns NULL for any invalid buffer address.
 */
void* lua_getBuffer(lua_State *L, int idx, size_t *len) {
	LUA_CHECKIDX(L, idx);
	if (!len) {
		error("lua_getBuffer(): you must pass a len pointer!");
		return NULL;
	}
	if (!lua_isnumber(L, idx + 1)) {
		// with missing or invalid length, we require the first argument to be a Lua string
		// lua_tolstring will return NULL for invalid argument
		return (void*)lua_tolstring(L, idx, len);
		// TODO: How do we deal with widestrings passed as Lua string? The length from
		// lua_tolstring is a byte count, while the caller or target function might expect
		// a (wide) char count... (possibly also an inconsistency with the 'explicit' form)
	} else {
		*len = lua_tointeger(L, idx + 1);
		// we got a length (possibly 0), so we can allow a more general pointer type for the string
		return luautils_checkptr(L, idx);
	}
}

// checks if Lua function argument idx is acceptable as a pointer type.
// if optional "value" (ptr) is set, it will receive the resulting pointer
bool luautils_isptr(lua_State *L, int idx, void* *value) {
	LUA_CHECKIDX(L, idx);
	switch (lua_type(L, idx)) {
		case LUA_TNUMBER:
			// accept integers as numeric addresses
			if (value) *value = (void*)luautils_toint32(L, idx);
			return true;

		case LUA_TUSERDATA:
		case LUA_TLIGHTUSERDATA:
			if (value) *value = lua_touserdata(L, idx);
			return true;

		// TODO: Do we want strings to be valid "pointers"? This might be a bit dangerous...
		//
		// For now: Yes. This function is e.g. used by dbg.lua memory access helpers,
		// which in turn end up calling lua_checkptr() below - and we have legacy code that
		// relies on being able to work on string "pointers".
		// (e.g. some of the packet code in hell_common/hell_net.lua)
		case LUA_TSTRING:
			if (value) *value = (void*)lua_tostring(L, idx);
			return true;

		case LUA_TCDATA:
		case LUA_TFUNCTION:
			if (value) *value = (void*)lua_topointer(L, idx);
			return true;
	}
	return false;
}

inline void *luautils_toptr(lua_State *L, int idx) {
	LUA_CHECKIDX(L, idx);
	void *result;

	if (luautils_isptr(L, idx, &result)) return result;

	// try standard conversion. if that fails, the result will be NULL
	return (void *)lua_topointer(L, idx);
}

inline void *luautils_checkptr(lua_State *L, int idx) {
	LUA_CHECKIDX(L, idx);
	void *result;

	if (!luautils_isptr(L, idx, &result))
		luaL_error(L, "Supplied argument type '%s' was not convertible "
			"to a C pointer", luaL_typename(L, idx));

	return result;
}

inline cpureg_t luautils_tocpu(lua_State *L, int idx) {
	LUA_CHECKIDX(L, idx);
	switch (lua_type(L, idx)){
		case LUA_TBOOLEAN:	return lua_toboolean(L, idx);
		case LUA_TSTRING:	return (cpureg_t)lua_tostring(L, idx);
	}
	return (cpureg_t)luautils_toptr(L, idx);
}

// pointer to number conversion (pushes result to Lua stack, or raises error)
void luautils_ptrtonumber(lua_State *L, int idx, int offset, bool nil_is_zero) {
	int type = lua_type(L, idx);
	switch (type) {
		case LUA_TNIL:
		case LUA_TNONE:
			if (nil_is_zero)
				lua_pushinteger(L, 0);
			else
				lua_pushnil(L);
			return;
		case LUA_TNUMBER: {
			// treat numbers as (32-bit) integers, always return them "unsigned"
			lua_Integer result = luautils_toint32(L, idx);
			if (result) result += offset; // add offset only if != 0
			luautils_pushptrnum(L, result);
			return;
		}
		default: {
			void *result;
			if (luautils_isptr(L, idx, &result)) {
				if (!result) { // NULL pointer
					if (nil_is_zero)
						lua_pushinteger(L, 0);
					else
						lua_pushnil(L);
				}
				//else lua_pushinteger(L, (uintptr_t)result + offset);
				else luautils_pushptrnum(L, result + offset);
				return;
			}
		}
	}
	// error / conversion failed
	luaL_error(L, "ptr value of type '%s' not convertible to a number",
		lua_typename(L, type));
}

// pointer to string conversion (pushes result to Lua stack, or raises error)
// format can specify the stack index of an optional string, 0 = use FMTPTR
void luautils_ptrtostring(lua_State *L, int idx, int format) {
	const char *fmt = format ? lua_tostring(L, format) : NULL;
	void *value = NULL;
	if (!lua_isnoneornil(L, idx))
		if (!luautils_isptr(L, idx, &value))
			luaL_error(L, "ptr value not convertible to a string");

	// either nil (value = NULL), or successful conversion
	if (value || fmt) {
		// (if a format string was given, we'll respect that even if value == 0)
		char result[20];
		snprintf(result, sizeof(result), fmt ? fmt : FMTPTR, value);
		lua_pushstring(L, result);
	}
	else lua_pushliteral(L, "<NULL>");
}

/* DEPRECATED
// get a global variable in 'CPU format'
inline cpureg_t luautils_get(lua_State *L, const char *global) {
	lua_getglobal(L, global);
	cpureg_t result = luautils_tocpu(L, -1);
	lua_pop(L, 1);
	return result;
}
*/


// If you expect having to deal with LuaJIT FFI <cdata> types,
// you can't rely on a meaningful comparison using lua_equal().
// see http://www.freelists.org/post/luajit/Type-of-CDATA-as-returned-by-lua-type,3
//
// This function tries to work around that by converting both arguments
// to pointers if any LUA_TCDATA is involved. While the solution may not
// be perfect, it gets the job done in most cases...
bool luautils_equal(lua_State *L, int index1, int index2) {
	if (!lua_iscdata(L, index1) && !lua_iscdata(L, index2))
		return lua_equal(L, index1, index2); // no cdata, just use lua_equal()

	// now at least one argument is of type LUA_TCDATA

	// if the other is `nil` or missing, consider that a mismatch
	if lua_isnoneornil(L, index1) return false;
	if lua_isnoneornil(L, index2) return false;

	// okay, let's just consider both arguments pointers
	// (and raise an error if they can't be converted)
	void *ptr1 = luautils_checkptr(L, index1);
	void *ptr2 = luautils_checkptr(L, index2);
	return (ptr1 == ptr2);
}

// a general-purpose function that tests for an 'empty' value
// at given stack index
bool luautils_isEmpty(lua_State *L, int idx) {
	LUA_CHECKIDX(L, idx);

	if (lua_isnoneornil(L, idx))
		return true; // (no value at index)

	switch (lua_type(L, idx)) {
		case LUA_TSTRING:
			// a string is 'empty' if it has zero length (== "")
			return (lua_objlen(L, idx) == 0);

		case LUA_TTABLE:
			LUA_ABSIDX(L, idx);
			// we'll test if the table contains at least one element (index)
			lua_pushnil(L);
			if (lua_next(L, idx)) {
				lua_pop(L, 2);
				return false; // this is a non-empty table
			}
			return true; // empty table

		case LUA_TLIGHTUSERDATA:
		case LUA_TUSERDATA:
			// test if it's a NULL pointer
			return lua_touserdata(L, idx) == NULL;

		case LUA_TCDATA:
			/* LuaJIT <cdata> is a bit tricky. A first attempt to simply compare
			 * the value to `nil` using lua_equal() FAILED.
			 * According to http://comments.gmane.org/gmane.comp.lang.lua.luajit/4488
			 * the Lua C API isn't guaranteed to fully interoperate with cdata.
			 *
			 * So we'll simply try to convert the cdata to a pointer type here.
			 * Be aware that this might not deliver expected (or meaningful)
			 * results if the cdata is not a pointer or array type!
			 */
			return lua_topointer(L, idx) == NULL;
	}
	return false; // consider anything else an non-empty value
}


// Run a .lua script, but don't use the file-based luaL_dofile() - call our
// "dofile" override from symbols.c instead.
// This allows C code to use the compiled-in Lua script 'fallbacks' (where
// applicable). The function expects the file name to be relative (to the
// mmBBQ base directory), and will always prefix it with the PWD.
// Errors get caught using pcall, and won't propagate to the Lua state.
// Use (stacktrace == true) to push an additional (verbose) error handler.
// The return value is the status code from lua_pcall(). A non-zero value
// indicates an error, with the error message on top of the Lua stack.
int luautils_dofile(lua_State *L, const char *filename, bool stacktrace) {
	int error_handler = 0;
	if (stacktrace) {
		lua_pushcfunction(L, luaStackTrace_C);
		error_handler = lua_gettop(L); // stack index
	}

	lua_pushcfunction(L, symbol_dofile_C);

	lua_pushstring(L, get_dll_dir());
	lua_pushstring(L, filename);
	lua_concat(L, 2); // concat/prefix filename with PWD

	int status = lua_pcall(L, 1, LUA_MULTRET, error_handler);
	if (stacktrace) lua_remove(L, error_handler);
	return status;
}


// Push a new sequential table ("array") with all the keys from a given table.
// Parameter "table" is the stack index of the table to process.
// "filter" can optionally specify the stack index of a function to be called
// for each key. If set (i.e. != 0), the filter is expected to receive a single
// parameter (the current key), and to return a boolean result indicating
// whether to include that particular key.
// This function pushes the resulting array (table) as the topmost value onto
// the Lua stack, and returns the number of keys (elements in the array).
int luautils_table_keys(lua_State *L, int table, int filter) {
	LUA_ABSIDX(L, table);
	luaL_checktype(L, table, LUA_TTABLE);
	if (filter) {
		LUA_ABSIDX(L, filter);
		luaL_checktype(L, filter, LUA_TFUNCTION);
	}
	lua_newtable(L);
	int result = lua_gettop(L); // stack index of result table
	int count = 0; // key count, = index for result table
	bool use_key = true;

	// iterate the source table
	lua_pushnil(L);
	while (lua_next(L, table)) {
		lua_pop(L, 1); // ignore (and pop) the value
		if (filter) {
			lua_pushvalue(L, filter); // duplicate filter function
			lua_pushvalue(L, -2); // duplicate key
			lua_call(L, 1, 1);
			use_key = lua_toboolean(L, -1);
			lua_pop(L, 1); // remove result (realign Lua stack)
		}
		if (use_key) {
			lua_pushvalue(L, -1); // duplicate the key
			lua_rawseti(L, result, ++count); // and store it to the result table
		}
	}
	return count;
}

// Try to find value in a table, and return its key (table index) or `nil` if
// not found. Parameter "table" is the stack index of the table to search, and
// the value to look for is expected at the top of the stack. Will push the
// first key k that satisfies (t[k] == value), or `nil` if there's no match.
// (The return value also indicates if there was a match.)
//
// Note: This function can be 'expensive', in that it will cause a sequential
// access to many (or even all) table keys with a value that has a disadvantageous
// position or isn't contained in the table. Each step in the search iteration
// also requires a comparison, which might in turn invoke metamethods ("__eq").
bool luautils_table_keyof(lua_State *L, int table) {
	LUA_ABSIDX(L, table);
	luaL_checktype(L, table, LUA_TTABLE);

	lua_pushnil(L);
	// we'll do a quick exit if the value is `nil`
	// (which is impossible to match as a table element)
	if (lua_isnil(L, -2)) return false;

	while (lua_next(L, table)) {
		/* DEBUG only
		luautils_getfunction(L, NULL, "tostring", true);
		lua_pushvalue(L, -2); // duplicate value
		lua_call(L, 1, 1); // string conversion
		debug("%s(%d) iterating %s '%s'", __func__, table,
				luaL_typename(L, -2), lua_tostring(L, -1));
		lua_pop(L, 1); // (drop value string)
		*/
		// Note: DON't use lua_equal() when having to deal with LUA_TCDATA!
		// if (lua_equal(L, -1, -3)) {
		if (luautils_equal(L, -1, -3)) {
			// we have a match. pop value, replace initial value with key
			lua_pop(L, 1);
			lua_replace(L, -2);
			return true;
		}
		lua_pop(L, 1); // (discard value)
	}

	// no matching key, pop value and push `nil`
	lua_pop(L, 1);
	lua_pushnil(L);
	return false;
}

// This implements a 'foolproof' table count (retrieving the number of elements)
// and "maxn" (highest numerical index similar to table.maxn). These have been
// combined into a single function, as both require a linear traversal of all
// indices (table keys).
// Unlike the Lua "length" operator (#), the counting here doesn't rely on
// sequential indices (however, the "maxn" only considers numerical values).
// The function returns the element count, and (optionally) sets and updates
// "maxn" while processing.
// If you don't need/want "maxn", pass a NULL pointer instead.
size_t luautils_table_count(lua_State *L, int idx, int *maxn) {
	LUA_CHKABSIDX(L, idx);
	//debug("%s(%p,%d,%p)", __func__, L, idx, maxn);
	size_t count = 0;
	if (maxn) *maxn = 0;

	if (lua_istable(L, idx)) {
		// iterate table at given index
		lua_pushnil(L);
		while (lua_next(L, idx)) {
			lua_pop(L, 1); // pop (unused) value, keep key for next iteration

			// examine numerical indices for maxn
			if (maxn && lua_isnumber(L, -1)) {
				register int key = lua_tointeger(L, -1);
				if (key > *maxn) *maxn = key;
			}
			count++;
		}
	}

	return count;
}

// Test if a table is a sequential array
// see e.g. http://stackoverflow.com/questions/6077006/how-can-i-check-if-a-lua-table-contains-only-sequential-numeric-indices
// Note: This function also returns `false` for an empty table (zero element count).
bool luautils_table_issequential(lua_State *L, int idx) {
	LUA_CHKABSIDX(L, idx);
	luaL_checktype(L, idx, LUA_TTABLE);
	register bool result = true;
	int seq = 0;
	// loop over all table elements
	lua_pushnil(L);
	while (result && lua_next(L, idx)) {
		// we got a key-value pair from the table, but don't really care for it
		// instead, we'll test for a value at sequential index
		lua_rawgeti(L, idx, ++seq);
		result = !lua_isnil(L, -1);
		// drop both values, preserving the key for "next" iteration
		lua_pop(L, 2);
	}
	if (!result) lua_pop(L, 1); // if we did not complete the next() loop, pop the key
	return result && (seq > 0); // (for empty tables, seq will be 0)
}

// "Append" a value to the table at the given stack index. You can pass a
// predefined position, or use (pos <= 0) to auto-calculate it. The topmost
// stack element is used for the value, and gets stored at the given position:
// t[pos] = v; or the 'next' sequential table index (pos <- table.maxn(t) + 1).
// The function finishes by discarding "pop" elements from the Lua stack.
// (Usually you'll want pop = 1 to discard the value that was appended.)
void luautils_table_append(lua_State *L, int idx, int pos, int pop) {
	LUA_CHKABSIDX(L, idx);
	luaL_checktype(L, idx, LUA_TTABLE);
	//debug("%s(%p,%d,%d)", __func__, L, idx, pop);
	if (pos < 1) {
		luautils_table_count(L, idx, &pos); // get "maxn"
		pos++; // 'next' (available) index
	}
	//debug("%s() maxn = %d", __func__, maxn);
	lua_pushinteger(L, pos); // push index
	lua_pushvalue(L, -2); // duplicate value - needed to get the (k,v) order right
	lua_settable(L, idx); // use rawset instead?
	if (pop > 0) lua_pop(L, pop);
}

// A helper function to 'merge' associative tables. It operates on the existing
// destination table at Lua stack index "dest", and processes stack indices
// "from" to "to". Each value processed must either be `nil` or another table.
// For the latter, all fields are copied into the destination table - in case
// of duplicate keys, values will overwrite existing entries.
void luautils_table_merge(lua_State *L, int from, int to, int dest) {
	LUA_CHKABSIDX(L, dest);
	luaL_checktype(L, dest, LUA_TTABLE); // make sure "dest" is a table
	LUA_CHKABSIDX(L, from);
	LUA_CHKABSIDX(L, to);

	register int index;
	for (index = from; index <= to; index++) {
		switch (lua_type(L, index)) {
			case LUA_TNIL:
				break;
			case LUA_TTABLE:
				// iterate over it
				lua_pushnil(L);
				while (lua_next(L, index)) {
					// duplicate the key, placing it before the value
					lua_pushvalue(L, -2);
					lua_insert(L, -2);
					// store key/value to destination table
					lua_settable(L, dest);
				}
				break;
			default:
				luaL_typerror(L, index, "table or nil");
		}
	}
}

// table "pack", [x]unpack counterpart
// This function creates a new table as the topmost value on the Lua stack,
// sequentially duplicates all values between stack indices "from" and "to"
// into it, and then stores the resulting element count into t[0]. The idea is
// that this constitutes a 'safer' (more reliable) way of storing nil values
// when processing varargs, without having to rely on a separate "count".
// The function returns the number of sequential entries stored (=t[0]).
int luautils_xpack(lua_State *L, int from, int to) {
	LUA_CHKABSIDX(L, from);
	LUA_ABSIDX(L, to);
	int count = 0;
	lua_createtable(L, (to >= from) ? from - to + 2 : 1, 0); // (preallocate)

	register int index;
	for (index = from; index <= to; index++) {
		lua_pushinteger(L, ++count); // (next) key
		lua_pushvalue(L, index); // duplicate value
		lua_rawset(L, -3);
	}

	// finally store count to t[0], and return it
	lua_pushinteger(L, 0);
	lua_pushinteger(L, count);
	lua_rawset(L, -3);
	return count;
}

// table unpack, but this considers t[0]
// This function extracts values from the table at the given index ("table"),
// in the range "from" to "to" and pushes them onto the Lua stack: t[from],
// t[from+1], ..., t[to]. An invalid "from" defaults to 1. xunpack() will
// use "to" either as-is, try to take the value from t[0] (assuming it was
// stored by xpack), or otherwise default to the table count (#t).
// Returns the number of values that were pushed onto the Lua stack.
int luautils_xunpack(lua_State *L, int table, int from, int to) {
	// we may safely accept table upvalues here! (detectable via pseudo index)
	if (table >= lua_upvalueindex(0)) LUA_CHKABSIDX(L, table);
	from = MAX(1, from); // (minimum "from" is 1)
	if (to < 1) {
		lua_pushinteger(L, 0); // key
		lua_rawget(L, table);
		if (lua_isnumber(L, -1)) {
			to = lua_tointeger(L, -1);
			if (to < 0)
				warn("%s(%p,%d) retrieved invalid 'to' %d from key 0",
					__func__, L, table, to);
		}
		else to = lua_objlen(L, table); // no usable t[0], try #t instead
		lua_pop(L, 1); // (drop value)
	}

	int count = 0;
	register int index;
	//debug("%s(%p,%d): %d to %d", __func__, L, table, from, to);
	for (index = from; index <= to; index++) {
		count++;
		lua_pushinteger(L, index); // key
		lua_rawget(L, table);
	}
	return count;
}


// The lua_pushfstring() function is somewhat limited in its abilities,
// see http://www.lua.org/manual/5.1/manual.html#lua_pushfstring
// This is an extended version that calls vformatmsg_len() from strings.c,
// and thus can use the full range of vsnprintf() features.
const char *luautils_pushfstring(lua_State *L, const char *fmt, ...) {
	char *str;
	va_list ap;
	va_start(ap, fmt);
	int len = vformatmsg_len(&str, fmt, ap);
	va_end(ap);
	lua_pushlstring(L, str, len);
	free(str);
	// We return a pointer to the string, to be consistent with lua_pushfstring().
	// (Note that the returned char* will eventually be subject to Lua garbage collection!)
	return lua_tostring(L, -1);
}

// A helper function for library initialization / setup of bindings.
// This calls a registering luaopen_* style CFunction with error protection,
// afterwards checks the stack balance and has an option to discard surplus
// stack arguments (e.g. if the CFunction pushes a module table that's unused).
// (Errors will be propagated to the Lua state with a suitable message.)
void libopen(lua_State *L, lua_CFunction func, const char *fname,
		int expect_args, int pop_args)
{
	//debug("%s(%p,%p,'%s',%d,%d)", __func__, L, func, fname, expect_args, pop_args);
	int top = lua_gettop(L);

	// luaopen_* type functions take no arguments (nargs = 0)
	if (luautils_cpcall(L, func, fname, 0)) lua_error(L); // (bail out on errors)

	if (expect_args >= 0)
		if (lua_gettop(L) != top + expect_args)
			luaL_error(L, "%s('%s') stack misaligned: expected %d, got %d",
				__func__, fname, top + expect_args, lua_gettop(L));

	if (pop_args > 0)
		luautils_stackclean(L, lua_gettop(L) - pop_args);
}

// Call require(module_name) - return value indicates success (true) or error
// Note: This function also pushes the return value of "require" to the Lua stack.
bool luautils_require(lua_State *L, const char *module_name) {
	if (luautils_getfunction(L, NULL, "require", false)) {
		lua_pushstring(L, module_name);
		// execute: non-zero result indicates error
		if (!lua_pcall(L, 1, 1, 0)) return true;
		error("%s(%p,'%s') error: %s", __func__, L, module_name, lua_tostring(L, -1));
		return false;
	}
	error("%s(%p,'%s'): \"require\" function not found!", __func__, L, module_name);
	return false;
}

// Set a name as "loaded". This is done by retrieving the value with the
// given stack index and storing it as package.loaded[name].
void luautils_setloaded(lua_State *L, int index, const char *name) {
	//debug("%s(%p,%d,'%s')", __func__, L, index, name);
	LUA_CHKABSIDX(L, index);
	lua_getglobal(L, "package");
	if (lua_istable(L, -1)) {
		lua_getfield(L, -1, "loaded");
		if (lua_istable(L, -1)) {
			lua_getfield(L, -1, name);
			if (!lua_isnil(L, -1)) {
				warn("%s(%p,%d,'%s') about to overwrite existing entry!",
					__func__, L, index, name);
			}
			lua_pop(L, 1);
			lua_pushvalue(L, index); // duplicate value
			lua_setfield(L, -2, name); // and store to package.loaded
		}
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
}

// Create a C module (table), striving to be compatible to package.module()
// This function creates a new module table and registers it as global var
// and under package.loaded with the given module name. It also initializes
// the _NAME and _M fields. The resulting table is left on top of the stack.
// So a call to this can easily be followed by luaL_register(L, NULL, ...)
void luautils_cmodule(lua_State *L, const char *module_name) {
	//debug("%s(%p,'%s')", __func__, L, module_name);
	lua_newtable(L);
	lua_pushvalue(L, -1); // duplicate table
	lua_setglobal(L, module_name); // and set as global var
	luautils_setloaded(L, -1, module_name); // set package.loaded[module_name]
	lua_pushvalue(L, -1); // duplicate table
	lua_setfield(L, -2, "_M"); // 'self' reference _M
	lua_pushstring(L, module_name);
	lua_setfield(L, -2, "_NAME"); // module name
}


/*
 * LUA debug functions (moved from mmbbq.c, 20130423 [BNO])
 */

// macros defining output function and prefixes
#define TRACE(...)	log_error("LUA_TRACE", __VA_ARGS__)
#define DUMP(...)	log_error("LUA_DUMP", __VA_ARGS__)
#define LIST(...)	log_error("LUA_LIST", __VA_ARGS__)

// Lua execution stack (back)trace, this is also our standard error handler
// This function supports a string-type upvalue to identify the calling
// context (e.g. a C function) - if set, it will be printed before the trace.
LUA_CFUNC(luaStackTrace_C) {
	// check for upvalue, and output it (if present)
	const char *label = lua_tostring(L, lua_upvalueindex(1));
	if (label) TRACE("[%s]", label);

	// the error message should be on top of the stack. print it first because stack is reverted
	int top = lua_gettop(L);
	if (top > 0 && lua_isstring(L, top)) TRACE("%s", lua_tostring(L, top));

	lua_Debug entry;
	int depth = -1;
	while (lua_getstack(L, ++depth, &entry)) {
		if (depth >= MAX_TRACE_DEPTH) {
			TRACE("maximum trace depth exceeded!");
			break;
		}
		if (!lua_getinfo(L, "Sln", &entry)) {
			TRACE("[%d] lua_getinfo failed (returned 0)", depth);
			continue;
		}
		// if (entry.currentline == -1 || !entry.name) continue;	// filter useless entries
		TRACE("@%d %s(%d): %s", depth, entry.short_src, entry.currentline, entry.name);
		// luaListVars(L, depth);	// produces unreadable garbage
	}

	//luaStackDump_C(L);
	return 0;
}

LUA_CFUNC(luaStackDump_C) {
	int i = lua_gettop(L);
	DUMP("Lua state %p, gettop (stack size) = %d", L, i);

	// output top-to-bottom
	for (; i > 0; i--) {
		/* repeat for each level */
		int t = lua_type(L, i);
		switch (t) {
			case LUA_TNIL:
				DUMP("#%d = nil", i);
				break;
			case LUA_TSTRING:  /* strings */
				DUMP("#%d = string: '%s'", i, lua_tostring(L, i));
				break;
			case LUA_TBOOLEAN:  /* booleans */
				DUMP("#%d = boolean: %s", i, lua_toboolean(L, i) ? "true" : "false");
				break;
			case LUA_TNUMBER:  /* numbers */
				DUMP("#%d = number: %g", i, lua_tonumber(L, i));
				break;
			case LUA_TFUNCTION:  /* functions */
				DUMP("#%d = function (%p)", i, lua_topointer(L, i));
				break;
			case LUA_TTABLE:  /* tables */
				DUMP("#%d = table (%p)", i, lua_topointer(L, i));
				break;
			case LUA_TUSERDATA:  /* userdata */
				DUMP("#%d = userdata @%p", i, lua_topointer(L, i));
				break;
			case LUA_TLIGHTUSERDATA:  /* light userdata */
				DUMP("#%d = light userdata @%p", i, lua_topointer(L, i));
				break;
			default:  /* other values */
				DUMP("#%d = %s @%p", i, lua_typename(L, t), lua_topointer(L, i));
				break;
		}
	}
	return 0;
}

bool luaListVars(lua_State *L, int level) {
	lua_Debug ar;
	if (lua_getstack(L, level, &ar) == 0)
		return false; /* failure: no such level in the arg_stack */

	const char *name;
	int i = 1;
	while ((name = lua_getlocal(L, &ar, i++)) != NULL) {
		LIST("local %d %s", i - 1, name);
		lua_pop(L, 1); // remove variable value
	}
	if (lua_getinfo(L, "f", &ar)) { /* retrieves function */
		i = 1;
		while ((name = lua_getupvalue(L, -1, i++)) != NULL) {
			LIST("upvalue %d %s", i - 1, name);
			lua_pop(L, 1); // remove upvalue value
		}
		lua_pop(L, 1); // remove function
	}
	return true;
}

// Lua wrapper
LUA_CFUNC(luaListVars_C) {
	lua_pushboolean(L, luaListVars(L, luaL_checkint(L, 1))); // arg1 = level
	return 1;
}

// try to return a string describing a function's caller (source position)
// (the string is malloc'ed, you are responsible for calling free() on it later!)
// level is the function nesting level
// TODO: check how this compares to (or might be superseded by) luaL_where()
char *lua_callerPosition(lua_State *L, int level) {
	lua_Debug ar; // activation record (see Lua documentation for lua_getstack and lua_getinfo)
	if (lua_getstack(L, level, &ar))
		if (lua_getinfo(L, "Sl", &ar)) { // try to get "what" = source and line information
			if (ar.currentline < 0)
				return strdup(ar.short_src); // (no valid line number)
			else
				return formatmsg("%s, line %d", ar.short_src, ar.currentline);
		}
	return strdup("<unknown caller>"); // in case of errors
}

// retrieve information via lua_getinfo(), and push result as a table
bool luautils_pushinfo(lua_State *L, const char *what, lua_Debug *ar) {
	bool result = lua_getinfo(L, what, ar);
	lua_newtable(L);
	if (result) {
		if (strchr(what, 'n')) {
			lua_pushstring(L, ar->name);
			lua_setfield(L, -2, "name");
			lua_pushstring(L, ar->namewhat);
			lua_setfield(L, -2, "namewhat");
		}
		if (strchr(what, 'S')) {
			lua_pushstring(L, ar->source);
			lua_setfield(L, -2, "source");
			lua_pushstring(L, ar->short_src);
			lua_setfield(L, -2, "short_src");
			lua_pushinteger(L, ar->linedefined);
			lua_setfield(L, -2, "linedefined");
			lua_pushinteger(L, ar->lastlinedefined);
			lua_setfield(L, -2, "lastlinedefined");
			lua_pushstring(L, ar->what);
			lua_setfield(L, -2, "what");
		}
		if (strchr(what, 'l')) {
			lua_pushinteger(L, ar->currentline);
			lua_setfield(L, -2, "currentline");
		}
		if (strchr(what, 'u')) {
			lua_pushinteger(L, ar->nups);
			lua_setfield(L, -2, "nups");
		}
	}
	return result;
}

char *lua_callerName(lua_State *L, int level) {
	lua_Debug ar; // activation record (see Lua documentation for lua_getstack and lua_getinfo)
	if (lua_getstack(L, level, &ar))
		if (lua_getinfo(L, "n", &ar)) // try to get "name" information
			return strdup(ar.name);
	return strdup("<unknown caller>"); // in case of errors
}

// A printf-style extension to the luaL_add* output functions
void luaL_addfmt(luaL_Buffer *B, const char *fmt, ...) {
	char *str;
	va_list ap;
	va_start(ap, fmt);
	int len = vformatmsg_len(&str, fmt, ap);
	va_end(ap);
	if (len > 0) luaL_addlstring(B, str, len);
	free(str);
}

// debugging / sanity check helper, will print an error on type mismatch, but
// without actually raising a Lua error
void luautils_checktype(lua_State *L, int index, int type, const char *where) {
	if (lua_type(L, index) != type) {
		luaL_Buffer msg;
		luaL_buffinit(L, &msg);

		luaL_addstring(&msg, where ? where : "<unknown>");
		luaL_addfmt(&msg,
			"(%p,stack=%d) type check @ %d FAILED: expected %s, got %s",
			L, lua_gettop(L), index,
			lua_typename(L, type), luaL_typename(L, index));

		luaL_pushresult(&msg);
		//luaStackTrace_C(L);
		//lua_pop(L, 1); // remove our error string again, rebalancing stack

		// The stack dump is probably more interesting than a full stack trace?
		error(lua_tostring(L, -1));
		lua_pop(L, 1); // remove our error string again, rebalancing stack
		luaStackDump_C(L);
	}
}

// Note: Corresponding Lua bindings are registered from utils_lua.c
