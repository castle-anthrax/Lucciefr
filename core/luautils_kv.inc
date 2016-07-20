/*
 * luautils_kv.inc
 *
 * include file for Lua key->value helper functions (assigning table fields)
 * implemented as macros
 */

// string key -> value

#define lua_table_kv_str_nil(L, key) \
	do { \
		lua_pushstring(L, key); \
		lua_pushnil(L); \
		lua_rawset(L, -3); \
	} while (0)

#define lua_table_kv_str_ptr(L, key, userdata) \
	do { \
		lua_pushstring(L, key); \
		/*luautils_pushptr(L, userdata);*/ \
		/* we explicitly allow/support NULL here */ \
		lua_pushlightuserdata(L, userdata); \
		lua_rawset(L, -3); \
	} while (0)

#define lua_table_kv_str_str(L, key, value) \
	do { \
		lua_pushstring(L, key); \
		if (value) lua_pushstring(L, value); else lua_pushnil(L); \
		lua_rawset(L, -3); \
	} while (0)

#define lua_table_kv_str_wstr(L, key, value) \
	do { \
		lua_pushstring(L, key); \
		luautils_pushwstring(L, value, -1); \
		lua_rawset(L, -3); \
	} while (0)

#define lua_table_kv_str_int(L, key, value) \
	do { \
		lua_pushstring(L, key); \
		lua_pushinteger(L, value); \
		lua_rawset(L, -3); \
	} while (0)

#define lua_table_kv_str_float(L, key, value) \
	do { \
		lua_pushstring(L, key); \
		lua_pushnumber(L, value); \
		lua_rawset(L, -3); \
	} while (0)

#define lua_table_kv_str_bool(L, key, value) \
	do { \
		lua_pushstring(L, key); \
		lua_pushboolean(L, value); \
		lua_rawset(L, -3); \
	} while (0)

#define lua_table_kv_str_cfunc(L, key, func) \
	do { \
		lua_pushstring(L, key); \
		lua_pushcfunction(L, func); \
		lua_rawset(L, -3); \
	} while (0)


// wide string key -> value

#define lua_table_kv_wstr_int(L, key, value) \
	do { \
		luautils_pushwstring(L, key, -1); \
		lua_pushinteger(L, value); \
		lua_rawset(L, -3); \
	} while (0)

#define lua_table_kv_wstr_wstr(L, key, value) \
	do { \
		luautils_pushwstring(L, key, -1); \
		luautils_pushwstring(L, value, -1); \
		lua_rawset(L, -3); \
	} while (0)


// integer key -> value

#define lua_table_kv_int_ptr(L, key, userdata) \
	do { \
		lua_pushinteger(L, key); \
		luautils_pushptr(L, userdata); \
		lua_rawset(L, -3); \
	} while (0)

#define lua_table_kv_int_wstr(L, key, value) \
	do { \
		lua_pushinteger(L, key); \
		luautils_pushwstring(L, value, -1); \
		lua_rawset(L, -3); \
	} while (0)

#define lua_table_kv_int_str(L, key, value) \
	do { \
		lua_pushinteger(L, key); \
		lua_pushstring(L, value); \
		lua_rawset(L, -3); \
	} while (0)

#define lua_table_kv_int_int(L, key, value) \
	do { \
		lua_pushinteger(L, key); \
		lua_pushinteger(L, value); \
		lua_rawset(L, -3); \
	} while (0)


// misc

#define lua_table_kv_addr(L, ptr) \
	do { \
		char string[24]; \
		/*snprintf(string, sizeof(string), " "FMTPTR" ", ptr);*/ \
		snprintf(string, sizeof(string), FMTPTR, ptr); \
		lua_table_kv_str_str(L, "addr", string); \
	} while (0)

#define lua_table_kv_ptr(L, ptr) \
	do { \
		/* "unnamed" pointer */ \
		lua_table_kv_str_ptr(L, "ptr", ptr); \
	} while (0)

#define lua_table_kv_struct(L, ptr, type) \
	do { \
		/* store "struct" entry identified by "type" string */ \
		char buf[256]; \
		snprintf(buf, sizeof(buf), "%s @ "FMTPTR, type, ptr); \
		lua_table_kv_str_str(L, "struct", buf); \
	} while (0)
