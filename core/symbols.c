/** @file symbols.c

Module for accessing exported symbols at runtime.
Specifically this also contains a specialized loader function that will allow
us to retrieve 'compiled-in' `.lua` scripts. See dll_symbolLoader_C().
*/
#include "symbols.h"

#include "globals.h"
#include "log.h"
#include "strutils.h"
#include "utils.h"

#if _LINUX
#include <dlfcn.h>
#endif

// show debug information (and fire diagnostic events) for all loaded packages
#define DEBUG_LOADERS	DEBUG | 0
// warn when falling back to compiled-in scripts
#define DEBUG_FALLBACK	DEBUG | 0

// Lua function names
#define DOFILE			"dofile"
#define DOFILE_BACKUP	"dofile_backup"

#if 0
// cache the mmBBQ export directory, as it will be accessed frequently
static void *CACHED_EXPORT_DIR = NULL;


// private helper function: convert offset ("relative virtual address")
// to type-neutral pointer (absolute address)
// Note: module may be NULL; in that case it will default to mmBBQ itself
inline void *rva_to_addr(size_t offset, HMODULE module) {
	if (!module) module = DLL_HANDLE;
	return (void*)module + offset;
}

// get pointer to the NT header of a given module
inline IMAGE_NT_HEADERS *getNtHeader(HMODULE module) {
	IMAGE_DOS_HEADER *dos_header = rva_to_addr(0, module); // DOS header at offset 0
	return rva_to_addr(dos_header->e_lfanew, module);
}

// returns the IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE flag
inline bool getDynamicBase(HMODULE module) {
	IMAGE_NT_HEADERS *nt_header = getNtHeader(module);
	return (nt_header->OptionalHeader.DllCharacteristics & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE) != 0;
}

// retrieve section header
IMAGE_SECTION_HEADER *getSectionHeader(HMODULE module, int index) {
	IMAGE_NT_HEADERS *nt_header = getNtHeader(module);
	if (index < 1 || index > nt_header->FileHeader.NumberOfSections) {
		error("getSectionHeader(%p, %d): invalid section index", module, index);
		return NULL;
	}
	// the array of section descriptors starts right after the NT header,
	// so calculate the offset (= header's size)
	IMAGE_SECTION_HEADER *result = (void*)nt_header
		+ sizeof("ULONG") + sizeof("IMAGE_FILE_HEADER") + nt_header->FileHeader.SizeOfOptionalHeader;
	return (result + (index - 1));
}

// retrieve the export directory structure for a given module,
// using information from the various headers
IMAGE_EXPORT_DIRECTORY *getExportDir(HMODULE module) {
	if (!module && CACHED_EXPORT_DIR) return CACHED_EXPORT_DIR;

	IMAGE_NT_HEADERS *nt_header = getNtHeader(module);
	IMAGE_DATA_DIRECTORY *datadir = &nt_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
	if (datadir->Size == 0) return NULL; // no export directory

	void *result = rva_to_addr(datadir->VirtualAddress, module);
	if (!module) /* i.e. "self" */ CACHED_EXPORT_DIR = result;
	return result;
}

inline unsigned int getExportedSymbolCount(HMODULE module) {
	IMAGE_EXPORT_DIRECTORY *export_dir = getExportDir(module);
	return (export_dir ? export_dir->NumberOfNames : 0);
}

char *getExportedSymbolName(HMODULE module, int index) {
	IMAGE_EXPORT_DIRECTORY *export_dir = getExportDir(module);
	if (!export_dir) {
		error("getExportedSymbolName(%p,%d): module has no exports", module, index);
		return NULL;
	}
	uint32_t *addressOfNames = rva_to_addr(export_dir->AddressOfNames, module);
	return rva_to_addr(addressOfNames[index], module);
}

void *getExportedSymbol(HMODULE module, int index) {
	IMAGE_EXPORT_DIRECTORY* export_dir = getExportDir(module);
	if (!export_dir) {
		error("getExportedSymbol(%p,%d): module has no exports", module, index);
		return NULL;
	}
	uint32_t *addressOfFunctions = rva_to_addr(export_dir->AddressOfFunctions, module);
	return rva_to_addr(addressOfFunctions[index], module);
}

/* The problem with *_size symbols is that they get badly messed up
 * being 'adjusted' by both the linker and the loader, to compensate
 * for the difference between compile time ("image base") and actual
 * load address ("module base").
 * We need to undo this if we want usable values - meaning we won't
 * convert the relative value (by adding the module address), but
 * instead correct it by the image base (undoing the linker's antics).
 *
 * Actually, it's probably much more straightforward to simply
 * calculate the size from *_start and *_end ...
 */
/*
size_t getExportedSymbolAsSizeValue(HMODULE module, int index) {
	IMAGE_DOS_HEADER* dos_header = rva_to_addr(0, module);
	IMAGE_NT_HEADERS* nt_header = rva_to_addr(dos_header->e_lfanew, module);
	IMAGE_EXPORT_DIRECTORY* export_dir
		= rva_to_addr(nt_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress, module);
	uint32_t* addressOfFunctions = rva_to_addr(export_dir->AddressOfFunctions, module);
	return addressOfFunctions[index] + nt_header->OptionalHeader.ImageBase;
}
*/
#endif

inline void *getExportedSymbolByName(HMODULE module, const char *name) {
/*
	int i = getExportedSymbolCount(module);
	while (i-- > 0) {
		char *func_name = getExportedSymbolName(module, i);
		if (strcmp(func_name, name) == 0) return getExportedSymbol(module, i);	// return address for matching name
	}
	return NULL;
*/
#if _WINDOWS
	return GetProcAddress(module, name);
#else
	return dlsym(module, name);
#endif
}

// helper function that strips the DLL's base path ("PWD") from a path
// (or returns it unchanged otherwise)
const char *strip_pwd(const char *pathname) {
	const char *pwd = get_dll_dir();
	// Test if this is an absolute path name starting with the DLL directory,
	// which we want to strip here. Skip number of leading chars accordingly.
	size_t len = strlen(pwd);
	return pathname + (strncmp(pathname, pwd, len) == 0 ? len : 0);
}


// Try to find a binary symbol (statically linked resource) within the library.
// (This is used to retrieve the compiled-in versions of .lua script files.) If
// found, data and *len get set accordingly, otherwise they'll be (NULL, 0).
// Caution: The matching depends on some naming conventions, and so far has only
// been tested for MinGW. Also: have a look at objwrap.lua and its comments.
char *getBinarySymbol(const char *path, size_t *len, char *buffer, size_t size) {
	if (!len) {
		error("%s(): you must pass a 'len' pointer!", __func__);
		return NULL;
	}
	*len = 0;
	char *data = NULL;	// _start symbol (beginning of data resource)
	char *end = NULL;	// _end symbol (end of data resource)

	// pattern is the search string (slightly modified pathname)
	// that we'll be looking for in our binary's symbols
	char pattern[PATH_MAX];

	strncpy(pattern, strip_pwd(path), sizeof(pattern));

	// convert pattern to our symbol name prefix convention
	// (which simply replaces any non-alphanumerical chars with '_')
	unsigned int i = strlen(pattern);
	while (i-- > 0) {
		if ('a' <= pattern[i] && pattern[i] <= 'z') continue;
		if ('A' <= pattern[i] && pattern[i] <= 'Z') continue;
		if ('0' <= pattern[i] && pattern[i] <= '9') continue;
		pattern[i] = '_';
	}
#ifdef RELAXED_NAME_CHECKING
	// append "binary" to the pattern, and allow partial matching later (ignore path)
	//strcat(pattern, "_binary_");
#endif
	extra("%s('%s') pattern:'%s'", __func__, path, pattern);

	char temp[PATH_MAX + 32];
	snprintf(temp, sizeof(temp), "%s_binary_obj_data_end", pattern);
	end = getExportedSymbolByName(lcfr_globals.libhandle, temp);
	extra("end symbol: %s = %p", temp, end);
	snprintf(temp, sizeof(temp), "%s_binary_obj_data_start", pattern);
	data = getExportedSymbolByName(lcfr_globals.libhandle, temp);
	extra("start symbol: %s = %p", temp, data);

	// we simply calculate *len (instead of relying on the XXX_size symbol)
	if (data && end) {
		*len = (end - data);
		debug("%s('%s') %s, start:%p, end:%p, size:%d",
			  __func__, pattern, temp, data, end, *len);
		if (buffer && size)
			snprintf(buffer, size, "%s", temp);
	}
	return (*len > 0) ? data : NULL;
}

/*
 * now lua bindings
 */

/*
LUA_CFUNC(dll_getSymbolCount_C) {
	HMODULE module = (HMODULE)lua_topointer(L, 1);	// (optional) module address
	lua_pushinteger(L, getExportedSymbolCount(module));
	return 1;
}

LUA_CFUNC(dll_getSymbolName_C) {
	int index = luaL_checkinteger(L, 1);
	HMODULE module = (HMODULE)lua_topointer(L, 2);	// (optional) module address

	char *data = getExportedSymbolName(module, index);
	if (!data) luaL_error(L, "dll_getSymbolName_C() index not found: %d", index);
	lua_pushstring(L, data);
	return 1;
}

LUA_CFUNC(dll_getSymbol_C) {
	int index = luaL_checkinteger(L, 1);
	HMODULE module = (HMODULE)lua_topointer(L, 2);	// (optional) module address

	void *data = getExportedSymbol(module, index);
	if (!data) luaL_error(L, "dll_getSymbol_C() index not found: %d", index);
	//lua_pushstring(L, data);
	luautils_pushptr(L, data);
	return 1;
}
*/

// Note: this returns the entire data for a symbol/binary as a Lua string
LUA_CFUNC(dll_getBinarySymbol_C) {
	const char *name = luaL_checkstring(L, 1);
	size_t len;
	char *data = getBinarySymbol(name, &len, NULL, 0);
	if (data) {
		if (!is_gzipped(data)) {
			// this should be plain text, push to Lua 'as-is'
			lua_pushlstring(L, data, len);
			return 1;
		}
		// gzipped data, we'll decompress it on the fly! :D
		size_t decompressed_size;
		char *decompressed = gzip_decompress(data, len, &decompressed_size);
		if (decompressed) {
			// we have successfully decompressed the resource and can transfer it to Lua
			lua_pushlstring(L, decompressed, decompressed_size);
			free(decompressed);
			return 1;
		}
		return luaL_error(L, "dll_getBinarySymbol_C(): decompression of gzipped resource FAILED");
	}
	return 0;
}

// Load a Lua "chunk" from a binary resource (for execution). Similar to
// luaL_loadbuffer(), but this function knows how to handle (gzip-)decompression.
int load_decompressed_buffer(lua_State *L, const char *data, size_t len,
		const char *name)
{
	char chunkname[PATH_MAX];
	// explicitly prefix chunk name with '=', so Lua doesn't tamper with it
	// (see e.g. http://lua.2524044.n2.nabble.com/Error-reporting-the-chunk-name-td4034634.html)
	snprintf(chunkname, sizeof(chunkname), "=%s", strip_pwd(name)); // (but strip PWD first)

	if (!is_gzipped(data)) {
		// this should be a plain(text) buffer, so we pass it to luaL_loadbuffer() directly
		return luaL_loadbuffer(L, data, len, chunkname);
	}
	// gzipped data, use decompressor before loading it
	size_t decompressed_size;
	char *decompressed = gzip_decompress(data, len, &decompressed_size);
	if (decompressed) {
		// decompression successful, continue with loading
		int result = luaL_loadbuffer(L, decompressed, decompressed_size, chunkname);
		free(decompressed);
		return result;
	}
	return luaL_error(L, "%s(%s): decompression of gzipped resource FAILED",
					  __func__, name);
}

// Lua "dofile" function override that dynamically 'falls back' to using a
// compiled-in resource if no matching .lua script is found for a given filename
LUA_CFUNC(symbol_dofile_C) {
	int basepointer = lua_gettop(L);
	const char *filename = luaL_checkstring(L, 1);
	char symbolname[PATH_MAX + 32];

	size_t len;
	char *data = getBinarySymbol(filename, &len, symbolname, sizeof(symbolname));
	if (!data || file_exists(filename)) {
#if DEBUG_LOADERS
		char *caller = lua_callerPosition(L, 1);
		char *msg = formatmsg("%s: executing dofile('%s') from %s",
			__func__, strip_pwd(filename), strip_pwd(caller));
		//fire_C(L, "DEBUG_LOADERS", 0, msg); // TODO: FIX ME!
		extra(msg);
		free(msg);
		free(caller);
#endif
		// call regular dofile loader (from the backup)
		lua_getglobal(L, DOFILE_BACKUP);
		lua_pushstring(L, filename);
		// execute, non-zero return value indicates error
		if (lua_pcall(L, 1, LUA_MULTRET, 0)) lua_error(L);
		return lua_gettop(L) - basepointer;
	}
#if DEBUG_FALLBACK
	char *msg = formatmsg("using compiled-in '%s' for '%s'",
						  symbolname, strip_pwd(filename));
	warn(msg);
# if DEBUG_LOADERS
	//fire_C(L, "DEBUG_LOADERS", 0, msg); // TODO: FIX ME!
# endif
	free(msg);
#endif

	// load the binary resource as a lua chunk ...
	if (load_decompressed_buffer(L, data, len, filename)) lua_error(L);
	// ... and execute it
	if (lua_pcall(L, 0, LUA_MULTRET, 0)) lua_error(L);

	return lua_gettop(L) - basepointer; // = result count
}

// "loader" function that processes a script resource (via given filename),
// returns compiled Lua "chunk"
// (see http://www.lua.org/manual/5.1/manual.html#pdf-package.loaders)
LUA_CFUNC(dll_symbolLoader_C) {

	// make filename contain <MOD_NAME>.lua
	char filename[PATH_MAX];
	snprintf(filename, sizeof(filename), "%s.lua", luaL_checkstring(L, 1));

	size_t len;
	char *data = getBinarySymbol(filename, &len, NULL, 0);
	if (!data) {
		// note: newline and tab are prepended to keep the output consistent with other package.loaders
		char *message = formatmsg("\n\tdll_symbolLoader_C(): no resource found for '%s'", filename);
		lua_pushstring(L, message);
		free(message);
		return 1;
	}

#if DEBUG_FALLBACK
	char *msg = formatmsg("%s: fallback to compiled-in '%s'", __func__, filename);
	warn(msg);
# if DEBUG_LOADERS
	//fire_C(L, "DEBUG_LOADERS", 0, msg); // TODO: FIX ME!
# endif
	free(msg);
#endif
	// non-zero result indicates an error
	if (load_decompressed_buffer(L, data, len, filename)) lua_error(L);
	return 1;
}

/*
// dynamicBase_C(module) helper to detect relocatable code (flag from PE header)
// module defaults to the target process
LUA_CFUNC(dynamicBase_C) {
	HMODULE module = (HMODULE)lua_topointer(L, 1);
	lua_pushboolean(L, getDynamicBase(module ? module : BASE));
	return 1;
}

// Lua function to retrieve the base address of a module
// you may pass one of the following:
// a numeric value (number) to be used directly
// a module name (string), which will translate into the module's image base
// `nil`, being converted to the default value (current module's image base)
// returns numeric address, or raises error if module name not found
LUA_CFUNC(symbol_getBase_C) {
	switch (lua_type(L, 1)) {
		case LUA_TNONE:
		case LUA_TNIL:
			luautils_pushptr(L, GetModuleHandleA(NULL));
			break;
		case LUA_TSTRING:
			luautils_pushptr(L, GetModuleHandleA(lua_tostring(L, 1)));
			if (lua_isnil(L, -1))
				luaL_error_fname(L, "can't find module \"%s\"",
					lua_tostring(L, 1));
			break;
	}
	luautils_ptrtonumber(L, -1, 0, false);
	return 1;
}
*/

#if DEBUG_LOADERS
// a diagnostic "loader" function that just prints the requested filename
// (and fires an event for debugging purposes)
LUA_CFUNC(debuggingLoader_C) {
	char *caller = lua_callerPosition(L, 2);
	char *msg = formatmsg("%s: require('%s') from %s",
		__func__, lua_tostring(L, 1), strip_pwd(caller));
	//fire_C(L, "DEBUG_LOADERS", 0, msg); // TODO: FIX ME!
	free(msg);
	free(caller);
	return 0; // we don't return anything (nil)
}
#endif

// Register a package loader (usually a function) for Lua 5.1 or 5.2
// This functions does the equivalent of
// "table.insert(package.loaders or package.searchers, value)" or
// "table.insert(package.loaders or package.searchers, pos, value)",
// depending on whether a position (> 0) is given or not.
// The value to store is expected at the top of the Lua stack upon entry
// and gets discarded (popped).
void register_package_loader(lua_State *L, int pos) {
	luautils_getfunction(L, "table", "insert", true); // push function
	lua_getglobal(L, "package");
	if (lua_istable(L, -1)) {
		lua_getfield(L, -1, "loaders");
		if (lua_isnil(L, -1)) {
			// Lua 5.2 uses "package.searchers" instead
			lua_pop(L, 1);
			lua_getfield(L, -1, "searchers");
		}
		lua_replace(L, -2); // remove "package" table
		if (pos > 0) {
			lua_pushinteger(L, pos);
			lua_pushvalue(L, -4); // duplicate "value" (loader function)
			lua_call(L, 3, 0); // "table.insert(loaders, pos, value)"
		} else {
			lua_pushvalue(L, -3); // duplicate "value" (loader function)
			lua_call(L, 2, 0); // "table.insert(loaders, value)"
		}
		lua_pop(L, 1); // (finally discard value from stack)
	}
	else luaL_error_fname(L, "failed to retrieve global 'package' table!");
}

// Lua bindings (initialization)

LUA_CFUNC(luaopen_symbols) {
	/*
	LREG(L, dll_getSymbolCount_C);
	LREG(L, dll_getSymbolName_C);
	LREG(L, dll_getSymbol_C);
	*/
	LREG(L, dll_getBinarySymbol_C);
	//LREG(L, symbol_dofile_C); // kind of redundant, as it gets set as "dofile"!
	/*
	LREG(L, dynamicBase_C);
	LREG(L, symbol_getBase_C);
	*/

	// overwrite global "dofile", to support resources from DLL
	lua_getglobal(L, DOFILE);
	lua_setglobal(L, DOFILE_BACKUP);
	lua_pushcfunction(L, symbol_dofile_C);
	lua_setglobal(L, DOFILE);

	// register package loader (append to table of loader functions),
	// this is to handle "require" properly
	lua_pushcfunction(L, dll_symbolLoader_C);
	register_package_loader(L, 0);

#if DEBUG_LOADERS
	// insert debugging loader (at specific position #2 = _after_ loaders for
	// package.preload and package.path, see Lua documentation on "package.loaders")
	lua_pushcfunction(L, debuggingLoader_C);
	register_package_loader(L, 2);
#endif
	return 0;
}
