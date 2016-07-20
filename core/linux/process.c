// Linux implementation of process functions

#include <dirent.h>

LUA_CFUNC(get_processes_C) {
	lua_getglobal(L, "print");
	lua_pushlstring(L, "Hallo from linux", 17);
	lua_call(L, 1, 1);
	return 1;
}

/*
 * The Linux version of this function will simply iterate the /procs directory
 * looking for process-related directories. Directory names that represent a
 * numerical value != 0 are considered to be PIDs.
 */
LUA_CFUNC(process_get_pids_C) {
	struct dirent *entry;
	DIR *dir = opendir("/proc");
	if (!dir) {
		lua_pushnil(L);
		luautils_push_syserror(L, "%s open()", __func__);
		return 2;
	}

	lua_newtable(L);
	unsigned int count = 0;
	while ((entry = readdir(dir)))
		// consider only entries that are directories and evaluate as number
		if (entry->d_type & DT_DIR) {
			int pid = atoi(entry->d_name);
			if (pid != 0) {
				//printf("PID %d\n", pid);
				lua_pushinteger(L, pid);
				lua_rawseti(L, -2, ++count); // t[#t + 1] = pid
			}
		}
	closedir(dir);

	//printf("%u entries\n", count);
	return 1; // returns table of pid entries
}

// Lua bindings
LUA_CFUNC(luaopen_process) {
	LREG(L, get_processes_C);
	LREG(L, process_get_pids_C);
	return 0;
}
