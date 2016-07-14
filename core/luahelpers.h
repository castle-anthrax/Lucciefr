/**
 * @file luahelpers.h
 *
 * Lua helpers declared as macros
 */

#ifndef LUAHELPERS_H
#define LUAHELPERS_H

/// a macro for proper Lua CFunction declarations
#ifndef LUA_CFUNC
# if BITS == 32
#  define LUA_CFUNC(fname)	int __attribute__((cdecl)) fname(lua_State *L)
# else // cdecl convention is inappropriate for 64 bits
#  define LUA_CFUNC(fname)	int fname(lua_State *L)
# endif
#endif


/// register a C function for Lua (with a given name string)
#define LREG_NAME(L, func, name) \
do { \
	lua_pushcfunction(L, (lua_CFunction)func); \
	lua_setglobal(L, name); \
} while (0)

/// register a C function for Lua, simply using the name of the function
#define LREG(L, func)	LREG_NAME(L, func, #func)

/// register an enum value for Lua, using its name
#define LENUM(L, value)	lua_setglobal_int(L, #value, value)

/// raise Lua error, but auto-prefix message with the function name "%s() "
/// (see http://www.lua.org/manual/5.1/manual.html#luaL_error)
#define luaL_error_fname(L, ...) \
do { \
	lua_pushstring(L, __func__); \
	lua_pushfstring(L, "() " __VA_ARGS__); \
	lua_concat(L, 2); \
	lua_error(L); \
} while (0)

/// protect against bad Lua stack index, zero is always invalid
#define LUA_CHECKIDX(L, idx) \
do { \
	if (idx == 0) \
		luaL_error(L, "%s() Lua state %p, error: " \
			"zero is not an acceptable stack index!", __func__, L); \
} while (0)

/// relative Lua stack indices sometimes are hard to maintain,
/// so convert it to an absolute one (when applicable)
#define LUA_ABSIDX(L, idx) \
do { \
	if (idx < 0) { idx += lua_gettop(L); idx++; } \
} while (0)

/// combine the two macros (LUA_ABSIDX and LUA_CHECKIDX)
#define LUA_CHKABSIDX(L, idx) \
do { \
	if (idx == 0) \
		luaL_error(L, "%s() Lua state %p, error: " \
			"zero is not an acceptable stack index!", __func__, L); \
	if (idx < 0) { idx += lua_gettop(L); idx++; } \
} while (0)

/// test for FFI `<cdata>` type
#define lua_iscdata(L, index)	(lua_type(L, index) == LUA_TCDATA)

/// shortcut for pushinteger + setglobal
#define lua_setglobal_int(L, name, value) \
do { \
	lua_pushinteger(L, value); \
	lua_setglobal(L, name); \
} while (0)

/// shortcut for pushstring + setglobal
#define lua_setglobal_str(L, name, value) \
do { \
	lua_pushstring(L, value); \
	lua_setglobal(L, name); \
} while (0)

/// shortcut for pushlightuserdata + setglobal
#define lua_setglobal_ptr(L, name, value) \
do { \
	lua_pushlightuserdata(L, value); \
	lua_setglobal(L, name); \
} while (0)

#endif // LUAHELPERS_H
