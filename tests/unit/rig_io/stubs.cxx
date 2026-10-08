// Trivial stubs so the real src/support/rig_io.cxx links without the rest of flrig.
// Only the TCP/IP path of readResponse()/sendCommand() is exercised.
#include <ctime>
#include <cstdio>
#include <cstddef>
#include <new>
#include <string>
#include <FL/Fl.H>
#include "rig.h"
#include "support.h"
#include "util.h"
#include "debug.h"
#include "status.h"
#include "rigbase.h"
#include "tod_clock.h"
#include "threads.h"
#include "tci_io.h"
#include "xmlrpc_rig.h"

status progStatus;                       // test_main sets use_tcpip, tcpip_ping_delay, xmlrpc_rig
Cserial *RigSerial = 0, *AuxSerial = 0, *SepSerial = 0;   // never dereferenced on the TCP path
bool bypass_serial_thread_loop = false;
pthread_mutex_t mutex_replystr = PTHREAD_MUTEX_INITIALIZER;
const char *dialog_warning_48_icon[] = { 0 };

// selrig: only selrig->replystr is touched (assignReplyStr). Build just that member
// in raw storage instead of constructing a whole rigbase (vtable, rigbase.cxx).
#pragma clang diagnostic ignored "-Winvalid-offsetof"
alignas(rigbase) static unsigned char selrig_mem[sizeof(rigbase)];
rigbase *selrig = (new (selrig_mem + offsetof(rigbase, replystr)) std::string,
                   reinterpret_cast<rigbase *>(selrig_mem));

// same as flrig util.cxx on POSIX
void MilliSleep(long msecs) {
	struct timespec tv = { msecs / 1000, (msecs % 1000) * 1000000L };
	nanosleep(&tv, NULL);
}
ullint zmsec() { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000ULL + t.tv_nsec / 1000000; }
const char *str2hex(const char *, size_t) { return ""; }   // LOG_DEBUG is off anyway

bool tci_running() { return false; }
void tci_send(std::string) {}
std::string xml_cat_string(std::string) { return ""; }
void xml_cmd_string(std::string) {}

// debug.h: logging off (level 0, mask 0); log/slog never called
debug::level_e debug::level = debug::QUIET_LEVEL;
uint32_t debug::mask = 0;
void debug::log(level_e, const char*, const char*, int, const char*, ...) {}
void debug::slog(level_e, const char*, const char*, int, const char*, ...) {}
void icons::set_message_icon(const char **) {}

guard_lock::guard_lock(pthread_mutex_t *m, std::string h, long) : mutex(m), how(h), start_time(0), time_out(0) { pthread_mutex_lock(mutex); }
guard_lock::~guard_lock() { pthread_mutex_unlock(mutex); }

// Cserial: link-only; serial path is not tested
bool Cserial::OpenPort() { return false; }
void Cserial::set_attributes() {}
int  Cserial::ReadBuffer(std::string &b, int, std::string, std::string) { b.clear(); return 0; }
int  Cserial::WriteBuffer(const char *, int n) { return n; }
void Cserial::FlushBuffer() {}
// Fl::awake / fl_alert come from the FLTK library (Fl::awake is a no-op without Fl::lock)
