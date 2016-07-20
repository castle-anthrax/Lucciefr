// luautils.h

#ifndef LUAUTILS_H
#define LUAUTILS_H

#include "bool.h"
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "lj_obj.h" // LUA_TCDATA (luajit FFI type identifier for <cdata>)

#define MAX_TRACE_DEPTH 20	/* maximum depth for stack traces */

// helper macros
#include "luahelpers.h"

// Lua table key->value helpers
#include "luautils_kv.inc"

#if _WINDOWS
	#include <windows.h>
	#define GET_LAST_ERROR	GetLastError()
#else
	#include <errno.h>
	#define GET_LAST_ERROR	errno
#endif

// native/'natural' CPU register type
// TODO: I moved these here for luautils_get(), but they probably belong into another .h?
#if BITS == 32
	typedef uint32_t cpureg_t;
#elif BITS == 64
	typedef uint64_t cpureg_t;
#endif

// retrieve (and push) function via module and name
bool luautils_getfunction(lua_State *L, const char *module,
		char const *function, bool propagate);

// create (Lua registry) reference for a function, via module and name
int luautils_getfuncref(lua_State *L, const char *module,
		char const *function);

// safeguarded Lua execution

// protect against exceptions during Lua execution, similar to lua_pcall()
int lua_guarded_pcall(lua_State *L, int nargs, int nresults, int errfunc);

// protected CFunction execution, similar to lua_cpcall()
bool luautils_cpcall(lua_State *L, lua_CFunction func, const char *fname, int nargs);

// helper macro to pass the function name automatically
#define LUA_CPCALL(L, func, nargs)	luautils_cpcall(L, func, #func, nargs)

// check luaL_dostring result, will print error message
bool luautils_dostring (lua_State *L, const char *str);


/* LUA ADDITIONS */

// numeric conversions that support / work with int32_t and uint32_t ranges
int32_t luautils_toint32(lua_State *L, int idx);
#define luautils_touint32(L, idx)	(uint32_t)luautils_toint32(L, idx)

uint32_t luautils_asuint32(lua_State *L, int idx); // 'implicit' conversion

// utility functions (for C)

void luautils_stackclean(lua_State *L, int basepointer); // stack cleanup

void luautils_pushptr(lua_State *L, const void *ptr); // push pointer to Lua
void luautils_pushwstring(lua_State *L, const wchar_t *s, int len); // push wide string

// push unsigned 32-bit integer. DON'T use this for pointers, unless you're asking for 64-bit trouble
// #define luautils_pushuint32(L, number)	lua_pushnumber(L, (uint32_t)(number))
// "push pointer numeric" - achieves something similar, while staying safe on pointer conversions
#define luautils_pushptrnum(L, value)	lua_pushnumber(L, (uintptr_t)(value))

void *lua_getBuffer(lua_State *L, int idx, size_t *len); // retrieve address and length for a buffer

// checks whether the lua object is convertible to a C void*. If so returns it. If not throws error.
void *luautils_checkptr(lua_State *L, int idx);
bool  luautils_isptr(lua_State *L, int idx, void* *value);
void *luautils_toptr(lua_State *L, int idx);
cpureg_t luautils_tocpu(lua_State *L, int idx);	// also converts bools, strings... to register arguments

// pointer to number (pushes result to Lua stack)
void luautils_ptrtonumber(lua_State *L, int idx, int offset, bool nil_is_zero);
// pointer to string (pushes result to Lua stack)
void luautils_ptrtostring(lua_State *L, int idx, int format);

/* DEPRECATED
// returns the value of a global Lua variable in "CPU format"
cpureg_t luautils_get(lua_State *L, const char *global);
*/

// a lua_equal() counterpart that handles <cdata> types
bool luautils_equal(lua_State *L, int index1, int index2);

bool luautils_isEmpty(lua_State *L, int idx); // test for "empty" value

// run .lua script with support for compiled-in "fallbacks"
int luautils_dofile(lua_State *L, const char *filename, bool stacktrace);

// tables
int luautils_table_keys(lua_State *L, int table, int filter); // push table with keys of a given table
bool luautils_table_keyof(lua_State *L, int table); // find key of value on stack (for given table index)
size_t luautils_table_count(lua_State *L, int idx, int *maxn); // a low-level table count / maxn
bool luautils_table_issequential(lua_State *L, int idx);
void luautils_table_append(lua_State *L, int idx, int pos, int pop); // append value to table
void luautils_table_merge(lua_State *L, int from, int to, int dest); // merge associative tables
// luautils_table_concat // merge non-associative tables (concat "arrays")

int luautils_xpack(lua_State *L, int from, int to); // table "pack", [x]unpack counterpart
int luautils_xunpack(lua_State *L, int table, int from, int to); // table unpack, considers t[0]

// helper function to report (system) errors
int luautils_push_syserrorno(lua_State *L, int err, const char *fmt, ...);

// push the last system error (message)
#define luautils_push_syserror(L, ...) \
	luautils_push_syserrorno(L, GET_LAST_ERROR, __VA_ARGS__)

// an extended version of lua_pushfstring (with full vsnprintf capabilities)
const char *luautils_pushfstring(lua_State *L, const char *fmt, ...);

// helper function for library initialization / setup of bindings
void libopen(lua_State *L, lua_CFunction func, const char *fname,
		int expect_args, int pop_args);

// C modules
bool luautils_require(lua_State *L, const char *module_name);
//void luautils_setloaded(lua_State *L, int index, const char *name);
void luautils_cmodule(lua_State *L, const char *module_name);

// Lua debug functions

LUA_CFUNC(luaStackTrace_C);
LUA_CFUNC(luaStackDump_C);
bool luaListVars(lua_State *L, int level);
LUA_CFUNC(luaListVars_C);

char *lua_callerPosition(lua_State *L, int level);
bool luautils_pushinfo(lua_State *L, const char *what, lua_Debug *ar);

#define luaL_addliteral(B, s)	luaL_addlstring(B, "" s, sizeof(s)-1)
void luaL_addfmt(luaL_Buffer *B, const char *fmt, ...);
void luautils_checktype(lua_State *L, int index, int type, const char *where);

#endif // LUAUTILS_H
