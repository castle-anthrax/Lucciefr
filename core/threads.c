#include "threads.h"

#include "log.h"
#include "timing.h"

#if _WINDOWS
pthread_t thread_start(THREAD_FUNC(*start_routine)(void *), void *attr,
		void *arg)
{
	unsigned int stacksize = 0;
	if (attr)
		stacksize = *(unsigned int *)attr;
	return (pthread_t)_beginthread(start_routine, stacksize, arg);
	// TODO: error handling
}

int thread_stop(pthread_t thread, unsigned int exit_code) {
	if (TerminateThread(thread, exit_code))
		return 0; // = ERROR_SUCCESS
	else
		return GetLastError();
}

int thread_wait(pthread_t thread, unsigned int timeout_ms) {
	return WaitForSingleObject(thread, timeout_ms);
}

#else
pthread_t thread_start(THREAD_FUNC(*start_routine)(void *), void *attr,
		void *arg)
{
	pthread_t result;
	int err = pthread_create(&result, attr, start_routine, arg);
	if (err) {
		error("pthread_create(%p, %p, %p): %s", strerror(err));
		return 0;
	}
	return result;
}

int thread_stop(pthread_t thread, unsigned int exit_code) {
	return pthread_cancel(thread);
}

int thread_wait(pthread_t thread, unsigned int timeout_ms) {
	double seconds, timeout = get_elapsed_ms() + timeout_ms;
	timeout /= 1000;
	timeout = modf(timeout, &seconds);
	struct timespec ts = { .tv_sec = seconds, .tv_nsec = timeout * 1e6 };
	return pthread_timedjoin_np(thread, NULL, &ts);
}

#endif
