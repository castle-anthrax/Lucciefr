// Linux implementation of process functions

#include <dirent.h>

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

static void fmt_pid_prefix(char *buffer, size_t size,
		pid_t pid, const char* suffix)
{
	if (suffix)
		snprintf(buffer, size, "/proc/%u/%s", pid, suffix);
	else
		snprintf(buffer, size, "/proc/%u/", pid);
}

// TODO: test non-ASCII names like "krötensoße"
char *get_pid_exe(pid_t pid, char *buffer, size_t size) {
	char *result = buffer;
	if (!pid) pid = getpid();
	ssize_t rc;

	char proc[32]; // symlink path: /proc/<pid>/exe
	fmt_pid_prefix(proc, sizeof(proc), pid, "exe");

	if (result) {
		rc = readlink(proc, result, size);
		if (rc < 0) return NULL;
	} else {
		// use dynamically sized buffer
		size = 128;
		while (true) {
			result = realloc(result, size);
			if (!result)
				return NULL;
			rc = readlink(proc, result, size);
			if (rc < 0) {
				free(result);
				return NULL;
			}
			if (rc < size)
				break;
			/* possibly truncated readlink() result, double size and retry */
			size *= 2;
		}
		// Note: YOU must call free() on the return value later!
	}
	if (rc < size)
		result[rc] = '\0'; // make sure the result is NUL-terminated
	return result;
}

static char *get_pid_stat_name(pid_t pid) {
	char *result = NULL;

	char proc[32]; // path: /proc/<pid>/stat
	fmt_pid_prefix(proc, sizeof(proc), pid, "stat");

	FILE *stat = fopen(proc, "r");
	if (stat) {
		fscanf(stat, "%*u (%ms", &result);
		fclose(stat);
		result[strlen(result) - 1] = '\0'; // remove trailing ')' character
	}
	return result; // make sure you free() it again later!
}

LUA_CFUNC(process_get_module_name_C) {
	pid_t pid = luaL_optint(L, 1, 0);
	char path[PATH_MAX];
	char *result = get_pid_exe(pid, path, sizeof(path));
	if (!result) {
		int err = errno;
		lua_pushnil(L);
		if (err != ENOENT)
			luautils_push_syserrorno(L, err, "get_pid_exe()");
		else {
			char *name = get_pid_stat_name(pid);
			lua_pushfstring(L, "can't dereference exe symlink for [%s]", name);
			free(name);
		}
		return 2;
	}
	lua_pushstring(L, path);
	return 1;
}

// Lua bindings
LUA_CFUNC(luaopen_process) {
	LREG(L, process_get_pids_C);
	LREG(L, process_get_module_name_C);
	return 0;
}
