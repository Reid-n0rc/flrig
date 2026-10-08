// fake threads.h: guard_lock = plain pthread lock (same contract as flrig's)
#pragma once
#include <pthread.h>
#include <string>
class guard_lock {
public:
	guard_lock(pthread_mutex_t *m, std::string = "", long = 0) : mutex(m) { pthread_mutex_lock(mutex); }
	~guard_lock() { pthread_mutex_unlock(mutex); }
private:
	pthread_mutex_t *mutex;
};
