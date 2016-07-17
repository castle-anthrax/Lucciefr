/*
 * process.c
 */

#include <windows.h>
#include <psapi.h>

#include "process.h"


LUA_CFUNC(get_processes_C) {
	DWORD aProcesses[1024], cbNeeded, cProcesses;
	unsigned i;
	if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded)) {
		return 0;
	}
	cProcesses = cbNeeded / sizeof(DWORD);
	lua_newtable(L);
	for(i = 0; i < cProcesses; i++) {
		if(aProcesses[i] != 0) {
			lua_pushnumber(L, i);
			lua_pushnumber(L, aProcesses[i]);
			lua_rawset(L, -3);
		}
	}

	return 1;
}

LUA_CFUNC(get_process_name_C) {
	if (!lua_isnumber(L, -1)) { 
		return 0;
	}
	DWORD pid = lua_tonumber(L, -1);
	HANDLE Handle = OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
        FALSE,
				pid);
  if (Handle) {
		TCHAR Buffer[MAX_PATH];
		if (GetModuleFileNameEx(Handle, 0, Buffer, MAX_PATH)) {
			lua_pushstring(L, Buffer);
		} else {
			lua_pushnil(L);
		}
		CloseHandle(Handle);
  }
	return 1;
}

LUA_CFUNC(luaopen_process) {
	LREG(L, get_processes_C);
	LREG(L, get_process_name_C);
	return 0;
}
