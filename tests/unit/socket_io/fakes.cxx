// Fakes for unit-testing the real src/support/socket_io.cxx without FLTK
// widgets, sockets or the rest of flrig. See README section in RESULTS.md.
#include <ctime>
#include <cstdio>
#include <deque>
#include <utility>
#include <cstdarg>
#include <cstring>
#include <strings.h>
#include <FL/Fl.H>
#include "fake_ctl.h"
#include "socket.h"
#include "status.h"
#include "util.h"

FakeCtl fake_ctl;
status progStatus;

static pthread_mutex_t fk = PTHREAD_MUTEX_INITIALIZER;   // guards widgets, queue, history
void fake_lock() { pthread_mutex_lock(&fk); }
void fake_unlock() { pthread_mutex_unlock(&fk); }

// ---- thread roles ------------------------------------------------------
static pthread_t main_tid;
static thread_local const char *role = "other";
static std::string fd_minus_role;
void fake_set_main_thread() { main_tid = pthread_self(); role = "main"; }
void fake_set_role(const char *r) { role = r; }
const char *fake_role() { return role; }
bool fake_is_main() { return pthread_equal(pthread_self(), main_tid); }
void fake_fd_minus_one_for_role(const char *r) { fd_minus_role = r; }

static void count_call()
{
	if (fake_is_main()) fake_ctl.w_main++;
	else if (std::string(role) == "other") fake_ctl.w_other++;
	else fake_ctl.w_poll++;
}

// ---- widgets -----------------------------------------------------------
static FakeWidget w_tcpip_connect("box_tcpip_connect", FL_LIGHT1);
static FakeWidget w_xcvr_connect("box_xcvr_connect", FL_LIGHT1);
static FakeWidget w_menu_box("tcpip_menu_box", FL_LIGHT1);
static FakeWidget w_tcpip_box("tcpip_box", FL_LIGHT1);
FakeWidget *box_tcpip_connect = &w_tcpip_connect;
FakeWidget *box_xcvr_connect = &w_xcvr_connect;
FakeWidget *tcpip_menu_box = &w_menu_box;
FakeWidget *tcpip_box = &w_tcpip_box;

static std::vector<Fl_Color> history;

void FakeWidget::color(Fl_Color c)
{
	fake_lock();
	count_call();
	col = c;
	if (this == &w_tcpip_connect && (history.empty() || history.back() != c)) history.push_back(c);
	fake_unlock();
}
Fl_Color FakeWidget::color() { fake_lock(); Fl_Color c = col; fake_unlock(); return c; }
void FakeWidget::redraw() { fake_lock(); count_call(); fake_unlock(); }
void FakeWidget::show()
{
	fake_lock(); count_call(); shown = true;
	if (fake_is_main()) fake_ctl.show_main++; else fake_ctl.show_off++;
	fake_unlock();
}
void FakeWidget::hide() { fake_lock(); count_call(); shown = false; fake_unlock(); }
int FakeWidget::visible() { fake_lock(); int v = shown; fake_unlock(); return v; }

void fake_preset(Fl_Color c)
{
	fake_lock();
	w_tcpip_connect.col = w_xcvr_connect.col = w_menu_box.col = w_tcpip_box.col = c;
	fake_unlock();
}

std::vector<Fl_Color> fake_colour_history() { fake_lock(); std::vector<Fl_Color> h = history; fake_unlock(); return h; }

// ---- Fl::awake (FLTK's own definitions are not linked) -----------------
static std::deque<std::pair<Fl_Awake_Handler, void *> > awake_q;
int Fl::awake(Fl_Awake_Handler cb, void *msg)
{
	fake_ctl.awake_attempts++;
	if (fake_ctl.awake_fail_first.load() > 0) {
		fake_ctl.awake_fail_first--;
		fake_ctl.awake_failed++;
		return -1;              // FLTK: ring buffer full
	}
	fake_lock();
	awake_q.push_back(std::make_pair(cb, msg));
	fake_unlock();
	fake_ctl.posts++;
	return 0;
}
void Fl::awake(void *) {}       // no-op, as flrig uses it only to wake the loop

int fake_drain()
{
	int n = 0;
	for (;;) {
		fake_lock();
		if (awake_q.empty()) { fake_unlock(); break; }
		std::pair<Fl_Awake_Handler, void *> e = awake_q.front();
		awake_q.pop_front();
		fake_unlock();
		e.first(e.second);      // run on the calling (main) thread, unlocked
		n++;
		fake_ctl.drained++;
	}
	return n;
}

// ---- MilliSleep (scaled) -----------------------------------------------
void MilliSleep(long msecs)
{
	long us = msecs * fake_ctl.sleep_us_per_ms.load();
	struct timespec tv = { us / 1000000, (us % 1000000) * 1000L };
	nanosleep(&tv, NULL);
}

// ---- Address / Socket --------------------------------------------------
Address::Address(const char *h, int, const char *) : node(h) {}
Address::Address(const char *h, const char *p, const char *) : node(h), service(p) {}
Address::~Address() {}

Socket::Socket(const Address &) : sockfd(3), canary(0x5eed) {}   // real ctor opens a socket
Socket::Socket(int fd_) : sockfd(fd_), canary(0x5eed) {}
Socket::~Socket() { canary = 0; }
void Socket::close(void) { sockfd = -1; }
void Socket::connect(void)
{
	fake_ctl.connect_calls++;
	if (fake_ctl.connect_throws) throw SocketException(61, "fake: connection refused");
	sockfd = 3;
}
void Socket::connect(const Address &)
{
	fake_ctl.connect_calls++;
	if (fake_ctl.connect_addr_throws) throw SocketException(61, "fake: connection refused");
	if (fd_minus_role.empty() || fd_minus_role != role) sockfd = 3;
}
// ---- timed socket (patch 8) ------------------------------------------
static pthread_mutex_t sk = PTHREAD_MUTEX_INITIALIZER;   // guards the in-flight queue
static std::deque<std::pair<long, std::string> > inflight;   // (arrival time us, data)
static long last_arrival_us = 0;
static long now_us()
{
	struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000L + t.tv_nsec / 1000L;
}
void fake_socket_inject(const std::string &data, int delay_ms)
{
	pthread_mutex_lock(&sk);
	long t = now_us() + delay_ms * 1000L;
	if (t < last_arrival_us) t = last_arrival_us;     // FIFO: never overtake earlier data
	last_arrival_us = t;
	inflight.push_back(std::make_pair(t, data));
	pthread_mutex_unlock(&sk);
}
size_t fake_socket_pending()
{
	pthread_mutex_lock(&sk);
	size_t n = 0;
	for (size_t i = 0; i < inflight.size(); i++) n += inflight[i].second.size();
	pthread_mutex_unlock(&sk);
	return n;
}
static bool arrived_any()
{
	pthread_mutex_lock(&sk);
	bool a = !inflight.empty() && inflight.front().first <= now_us();
	pthread_mutex_unlock(&sk);
	return a;
}
static size_t take_arrived(std::string &buf)
{
	size_t n = 0;
	pthread_mutex_lock(&sk);
	long now = now_us();
	while (!inflight.empty() && inflight.front().first <= now) {
		buf += inflight.front().second;
		n += inflight.front().second.size();
		inflight.pop_front();
		fake_ctl.chunks_delivered++;
	}
	pthread_mutex_unlock(&sk);
	return n;
}

// ---- fake LOG_* --------------------------------------------------------
void fake_log(const char *fmt, ...)
{
	char b[1024];
	va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
	fake_ctl.log_total++;
	for (char *p = b; *p; p++) if (!strncasecmp(p, "discard", 7)) { fake_ctl.log_discard++; break; }
}

size_t Socket::send(const std::string s)
{
	long n = ++fake_ctl.sends;
	if (fake_ctl.timed && fake_ctl.reply_script) {
		std::pair<std::string, int> r = fake_ctl.reply_script(s, n);
		if (r.second >= 0) { fake_socket_inject(r.first, r.second); fake_ctl.replies_scheduled++; }
	}
	return s.size();
}
void Socket::set_nonblocking(bool) {}
void Socket::set_timeout(double) {}
int Socket::fd(void)
{
	if (!fd_minus_role.empty() && fd_minus_role == role) return -1;
	return sockfd;
}
size_t Socket::recv(std::string &buf)
{
	long n = ++fake_ctl.recv_calls;
	fake_ctl.recv_entered++;
	int ms = fake_ctl.recv_block_ms.load();
	if (ms > 0) {
		struct timespec tv = { ms / 1000, (ms % 1000) * 1000000L };
		nanosleep(&tv, NULL);
		volatile long c = canary;          // touches *this after the wait (P5)
		(void)c;
	}
	fake_ctl.recv_returned++;
	if (fake_ctl.timed) {
		// receive thread = role "other"; tests and poll threads set their role
		bool rcv_thread = std::string(role) == "other" && !fake_is_main();
		if (!rcv_thread) {
			fake_ctl.recv_calls_sender++;
			if (fake_ctl.recv_throw_on_sender) { fake_ctl.recv_throws++; throw SocketException(54, "fake: connection reset"); }
		} else if (fake_ctl.sends < fake_ctl.rcv_hold_until_sends) {
			if (arrived_any()) fake_ctl.recv_held++;
			return 0;
		}
		return take_arrived(buf);
	}
	RecvAct a = fake_ctl.recv_script ? fake_ctl.recv_script(n) : RECV_DATA;
	if (a == RECV_THROW) throw SocketException(54, "fake: connection reset");
	if (a == RECV_EMPTY) return 0;
	buf += "FA00014070000;";
	return 14;
}
