// Concurrency test for debug::log(): N threads log at once (no window shown).
#include <pthread.h>
#include <cstdio>
#include <cstdlib>
#include <FL/Fl.H>
#include "debug.h"

static int ITER = 2000;

static void *worker(void *arg)
{
	long id = (long)arg;
	for (int i = 0; i < ITER; i++) {
		if (i & 1) LOG_WARN("thread %ld iteration %d some text to append", id, i);
		else debug::log(debug::ERROR_LEVEL, __func__, __FILE__, __LINE__,
				"thread %ld direct %d", id, i);
	}
	return 0;
}

int main(int argc, char **argv)
{
	int nthreads = argc > 1 ? atoi(argv[1]) : 4;
	if (argc > 2) ITER = atoi(argv[2]);
	const char *logfile = argc > 3 ? argv[3] : "debug_test.log";
	debug::level = debug::INFO_LEVEL;
	debug::start(logfile);      // creates (never shows) the Event log window
	pthread_t t[16];
	for (long i = 0; i < nthreads; i++) pthread_create(&t[i], 0, worker, (void *)i);
	for (int i = 0; i < nthreads; i++) pthread_join(t[i], 0);
	printf("done: %d threads x %d logs\n", nthreads, ITER);
	return 0;
}
