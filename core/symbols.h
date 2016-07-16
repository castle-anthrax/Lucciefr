/// @file symbols.h

#ifndef SYMBOLS_H
#define SYMBOLS_H

#include "luautils.h"

#if _WINDOWS
	#include <windows.h>
#else
	#define HMODULE		void*
#endif

/*
// Note: Passing module == NULL defaults to the mmBBQ DLL itself

// retrieve NT header
IMAGE_NT_HEADERS *getNtHeader(HMODULE module);

// returns the IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE flag
bool getDynamicBase(HMODULE module);

// retrieve section header
IMAGE_SECTION_HEADER *getSectionHeader(HMODULE module, int index);

// retrieve export directory for a given module
IMAGE_EXPORT_DIRECTORY *getExportDir(HMODULE module);

// returns the number of all exported symbols
unsigned int getExportedSymbolCount(HMODULE module);

// returns the name of a symbol identified by its index
char *getExportedSymbolName(HMODULE module, int index);

// returns a data pointer for a symbol identified by its index
void *getExportedSymbol(HMODULE module, int index);
*/

/// returns a data pointer for a symbol identified by its name
void *getExportedSymbolByName(HMODULE module, const char *name);

/*
 * tries to find a binary symbol (statically linked file) within the export table.
 * therefore some regex magic is used to "guess" whether a file is contained.
 *
 * binary_[<PATHs>_]<FILENAME>_<EXT>_[start|end|size] are encoded into the symbol table
 * note: allowed is [a-zA-Z0-9], others are converted to "_"
 *
 * e.g.
 * binary_common_<FNAME>_lua_start
 * binary_common_<FNAME>_lua_end
 * binary_common_<FNAME>_lua_size
 *
 * @return pointer to data (NULL if not found), len will be set to data length, (optional) symbol will receive the symbol name
 */
char *getBinarySymbol(const char *path, size_t *len, char *buffer, size_t size);

// our "dofile" variant that knows about compiled-in scripts
LUA_CFUNC(symbol_dofile_C);

// TODO: maybe implement in far time future
//int getSymbol(int index, char* _name, char* _data);

// Lua bindings
LUA_CFUNC(luaopen_symbols);

#endif // SYMBOLS_H
