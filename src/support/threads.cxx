// ----------------------------------------------------------------------------
//      threads.cxx
//
// Copyright (C) 2014
//              Stelios Bounanos, M0GLD
//              David Freese, W1HKJ
//
// This file is part of fldigi.
//
// fldigi is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// fldigi is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
// ----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#include <map>

#include "threads.h"
#include "util.h"
#include "support.h"
#include "debug.h"

/// This ensures that a mutex is always unlocked when leaving a function or block.

extern pthread_mutex_t mutex_replystr;
extern pthread_mutex_t command_mutex;
extern pthread_mutex_t mutex_serial;
extern pthread_mutex_t debug_mutex;
extern pthread_mutex_t mutex_rcv_socket;
extern pthread_mutex_t mutex_trace;

/// lock waits longer than this are written to debug_log.txt
static const long LOCK_WAIT_LOG_MSEC = 100;

/// thread and purpose of a guard_lock that holds a mutex
struct lock_owner {
	pthread_t thread;
	std::string how;
};

/// threads that hold a mutex through a guard_lock, so that a nested
/// guard_lock on the same mutex in the same thread neither deadlocks
/// nor releases the outer lock, and a wait can be logged with the
/// holder's purpose
typedef std::map<pthread_mutex_t *, lock_owner> LOCK_OWNER_MAP;
static LOCK_OWNER_MAP lock_owners;
static pthread_mutex_t mutex_lock_owners = PTHREAD_MUTEX_INITIALIZER;

static bool held_by_this_thread(pthread_mutex_t *mutex)
{
	pthread_mutex_lock(&mutex_lock_owners);
	LOCK_OWNER_MAP::iterator it = lock_owners.find(mutex);
	bool held = (it != lock_owners.end()) &&
		pthread_equal(it->second.thread, pthread_self());
	pthread_mutex_unlock(&mutex_lock_owners);
	return held;
}

/// purpose given by the guard_lock that holds mutex, if any
static std::string lock_holder(pthread_mutex_t *mutex)
{
	pthread_mutex_lock(&mutex_lock_owners);
	LOCK_OWNER_MAP::iterator it = lock_owners.find(mutex);
	std::string holder = (it == lock_owners.end()) ? "?" : it->second.how;
	pthread_mutex_unlock(&mutex_lock_owners);
	return holder;
}

static void set_lock_owner(pthread_mutex_t *mutex, const std::string &how)
{
	pthread_mutex_lock(&mutex_lock_owners);
	lock_owner owner;
	owner.thread = pthread_self();
	owner.how = how;
	lock_owners[mutex] = owner;
	pthread_mutex_unlock(&mutex_lock_owners);
}

static void clear_lock_owner(pthread_mutex_t *mutex)
{
	pthread_mutex_lock(&mutex_lock_owners);
	lock_owners.erase(mutex);
	pthread_mutex_unlock(&mutex_lock_owners);
}

guard_lock::guard_lock(pthread_mutex_t* m, std::string h, long tout) :
	mutex(m), m_locked(false) {

	how.clear();
	how = h;
	time_out = tout;
	start_time = zmsec();

	if (held_by_this_thread(mutex)) {
		std::string sznested = name(mutex);
		sznested.append(" nested lock, already held");
		if (!h.empty()) {
			sznested.append(", ").append(h);
		}
		lock_trace(1, sznested.c_str());
		return;
	}

	if (pthread_mutex_trylock(mutex) == 0) {
		set_lock_owner(mutex, h);
		m_locked = true;
		std::string szlock = name(mutex);
		szlock.append(" try lock ");
		if (!h.empty()) {
			szlock.append(", ").append(h);
		}
		lock_trace(1, szlock.c_str());
		return;
	}

/// another thread holds the mutex; wait for it in pthread_mutex_lock,
/// which hands it over as soon as it is unlocked.  Retrying trylock
/// every 50 msec instead let other threads take it first, and PTT
/// waited about a second while XML-RPC clients polled meters.
	std::string holder = lock_holder(mutex);

	pthread_mutex_lock(mutex);
	set_lock_owner(mutex, h);
	m_locked = true;

	long waited = zmsec() - start_time;
	if (waited > LOCK_WAIT_LOG_MSEC) {
		std::string szlock;
		szlock.assign("lock waited ").append(name(mutex));
		if (!h.empty()) {
			szlock.append(", ").append(h);
		}
		failure_trace(1, szlock.c_str());
		LOG_WARN("%s: waited %ld msec for %s, held by %s",
			h.c_str(), waited, name(mutex), holder.c_str());
	}
}

guard_lock::~guard_lock(void) {

	char szlock[200];
	long now = zmsec();
	snprintf(szlock, sizeof(szlock), "%s locked for %lu msec", name(mutex), (now - start_time));
	lock_trace(1, szlock);

	if (now - start_time > time_out) {
		snprintf(szlock, sizeof(szlock), "%s [ %s ] LOCK TIME: %lu", name(mutex), how.c_str(), (now - start_time));
		failure_trace(1, szlock);
	}

	if (!m_locked) {
		return;
	}
	clear_lock_owner(mutex);
	pthread_mutex_unlock(mutex);
}

const char * guard_lock::name(pthread_mutex_t *m) {
	if (m == &mutex_replystr) return "mutex_replystr";
	if (m == &command_mutex) return "command_mutex";
	if (m == &mutex_replystr) return "mutex_replystr";
	if (m == &mutex_serial) return "mutex_serial";
	if (m == &debug_mutex) return "debug_mutex";
	if (m == &mutex_rcv_socket) return "mutex_rcv_socket";
	if (m == &mutex_srvc_reqs) return "mutex_service_requests";
	if (m == &mutex_trace) return "mutex_trace";
	return "";
}

#ifndef __WIN_32_

int nano_sleep(const struct timespec *req, struct timespec *rem)
{
	return nanosleep(req, rem);
}

#else

int nano_sleep(const struct timespec *req, struct timespec *rem)
{
	if (unlikely(req->tv_nsec < 0 || req->tv_nsec < 0L || req->tv_nsec > 999999999L)) {
		errno = EINVAL;
		return -1;
	}
	Sleep(req->tv_sec * 1000 + req->tv_nsec / 1000000L);
	if (unlikely(rem)) {
		rem->tv_sec = 0;
		rem->tv_nsec = 0L;
	}
	return 0;
}

#endif
