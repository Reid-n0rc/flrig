// Trivial stubs so the real src/support/debug.cxx links without the rest of flrig.
#include <cstdio>
#include <fcntl.h>
#include "status.h"
status progStatus;                                   // zero-init: debugtrace == false
void trace(int, ...) {}                              // not called (debugtrace false)
char *ztime() { static char t[] = "00:00:00.000"; return t; } // constant string, no shared writes
int set_cloexec(int fd, unsigned char v) { return fcntl(fd, F_SETFD, v ? FD_CLOEXEC : 0); }
// guard_lock (threads.h): plain lock/unlock, for patched debug.cxx versions that use it.
#include "threads.h"
guard_lock::guard_lock(pthread_mutex_t *m, std::string h, long) : mutex(m), how(h), start_time(0), time_out(0) { pthread_mutex_lock(mutex); }
guard_lock::~guard_lock() { pthread_mutex_unlock(mutex); }
