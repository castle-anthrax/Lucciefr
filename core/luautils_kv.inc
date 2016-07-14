/*
 * luautils_kv.inc
 *
 * include file for Lua key->value helper functions (assigning table fields)
 */

#ifdef LUAUTILS_KV_HEADER_ONLY
# define INLINE /* ignore */
# define DECLARE(body) ;	/* transform into prototype declaration (strip body) */
#else
# define INLINE inline
# define DECLARE(body) { body }
#endif


// string key -> value

INLINE void lua_table_kv_str_nil(lua_State *L, const char* key)
DECLARE(
	lua_pushstring(L, key);
	lua_pushnil(L);
	lua_rawset(L, -3);
)

INLINE void lua_table_kv_str_ptr(lua_State *L, const char* key, void* userdata)
DECLARE(
	lua_pushstring(L, key);
	//luautils_pushptr(L, userdata);
	lua_pushlightuserdata(L, userdata); // we explicitly allow/support NULL here
	lua_rawset(L, -3);
)

INLINE void lua_table_kv_str_str(lua_State *L, const char* key, const char* value)
DECLARE(
	lua_pushstring(L, key);
	if (value) lua_pushstring(L, value); else lua_pushnil(L);
	lua_rawset(L, -3);
)

INLINE void lua_table_kv_str_wstr(lua_State *L, const char* key, const wchar_t* value)
DECLARE(
	lua_pushstring(L, key);
	luautils_pushwstring(L, value, -1);
	lua_rawset(L, -3);
)

INLINE void lua_table_kv_str_int(lua_State *L, const char* key, int value)
DECLARE(
	lua_pushstring(L, key);
	lua_pushinteger(L, value);
	lua_rawset(L, -3);
)

INLINE void lua_table_kv_str_float(lua_State *L, const char* key, float value)
DECLARE(
	lua_pushstring(L, key);
	lua_pushnumber(L, value);
	lua_rawset(L, -3);
)

INLINE void lua_table_kv_str_bool(lua_State *L, const char* key, int value)
DECLARE(
	lua_pushstring(L, key);
	lua_pushboolean(L, value);
	lua_rawset(L, -3);
)

INLINE void lua_table_kv_str_cfunc(lua_State *L, const char* key, lua_CFunction f)
DECLARE(
	lua_pushstring(L, key);
	lua_pushcfunction(L, f);
	lua_rawset(L, -3);
)


// wide string key -> value

INLINE void lua_table_kv_wstr_int(lua_State *L, const wchar_t* key, int value)
DECLARE(
	luautils_pushwstring(L, key, -1);
	lua_pushinteger(L, value);
	lua_rawset(L, -3);
)

INLINE void lua_table_kv_wstr_wstr(lua_State *L, const wchar_t* key, const wchar_t* value)
DECLARE(
	luautils_pushwstring(L, key, -1);
	luautils_pushwstring(L, value, -1);
	lua_rawset(L, -3);
)


// integer key -> value

INLINE void lua_table_kv_int_ptr(lua_State *L, int key, void* userdata)
DECLARE(
	lua_pushinteger(L, key);
	luautils_pushptr(L, userdata);
	lua_rawset(L, -3);
)

INLINE void lua_table_kv_int_wstr(lua_State *L, int key, wchar_t* value)
DECLARE(
	lua_pushinteger(L, key);
	luautils_pushwstring(L, value, -1);
	lua_rawset(L, -3);
)

INLINE void lua_table_kv_int_str(lua_State *L, int key, const char* value)
DECLARE(
	lua_pushinteger(L, key);
	lua_pushstring(L, value);
	lua_rawset(L, -3);
)

INLINE void lua_table_kv_int_int(lua_State *L, int key, int value)
DECLARE(
	lua_pushinteger(L, key);
	lua_pushinteger(L, value);
	lua_rawset(L, -3);
)


// misc

INLINE void lua_table_kv_addr(lua_State *L, void* ptr)
DECLARE(
	char string[20];
	//snprintf(string, sizeof(string), " "FMTPTR" ", ptr);
	snprintf(string, sizeof(string), FMTPTR, ptr);
	lua_table_kv_str_str(L, "addr", string);
)

INLINE void lua_table_kv_ptr(lua_State *L, void* ptr)
DECLARE(
	// "unnamed" pointer
	lua_table_kv_str_ptr(L, "ptr", ptr);
)

INLINE void lua_table_kv_struct(lua_State *L, void* ptr, const char* type)
DECLARE(
	// store "struct" entry identified by "type" string
	char buf[255];
	snprintf(buf, sizeof(buf), "%s @ "FMTPTR, type, ptr);
	lua_table_kv_str_str(L, "struct", buf);
)

// (clean up by undoing definition, to allow multiple includes)
#undef INLINE
#undef DECLARE
