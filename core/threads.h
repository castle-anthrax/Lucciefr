/// @file threads.h

#if _WINDOWS
	#include <process.h>
	#include <windows.h>

	#define THREAD_FUNC		void
	#define pthread_t		HANDLE
	#define thread_exit		ExitThread

#else
	// assume POSIX threads
	#include <math.h>
	#include <pthread.h>
	#include <string.h> // strerror()

	#define THREAD_FUNC		void*
	#define thread_exit		pthread_exit

#endif

pthread_t thread_start(THREAD_FUNC(*start_routine)(void *), void *attr, void *arg);
int thread_stop(pthread_t thread, unsigned int exit_code);
int thread_wait(pthread_t thread, unsigned int timeout_ms);
