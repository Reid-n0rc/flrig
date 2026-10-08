// Test control for the fakes (fakes.cxx). Everything here is thread-safe so
// that ThreadSanitizer reports only races in socket_io.cxx itself.
#pragma once
#include <pthread.h>
#include <atomic>
#include <functional>
#include <string>
#include <vector>
#include <utility>
#include <FL/Enumerations.H>

// Recording widget double: color()/redraw()/show()/hide() record the thread
// role of the caller (main / poll / other = receive thread) and the colour.
class FakeWidget {
public:
	FakeWidget(const char *n, Fl_Color c) : name(n), col(c), shown(false) {}
	void color(Fl_Color c);
	Fl_Color color();
	void redraw();
	void show();
	void hide();
	int visible();
	const char *name;
	Fl_Color col; bool shown;
};

enum RecvAct { RECV_DATA, RECV_EMPTY, RECV_THROW };

struct FakeCtl {
	// --- script ---
	std::function<RecvAct(long callno)> recv_script;   // default: RECV_DATA
	std::atomic<int> recv_block_ms{0};                // sleep inside recv (P5)
	std::atomic<bool> connect_throws{false};          // Socket::connect()
	std::atomic<bool> connect_addr_throws{false};     // Socket::connect(Address)
	std::atomic<int> awake_fail_first{0};             // Fl::awake(cb) returns -1 this many times
	std::atomic<int> sleep_us_per_ms{1000};           // MilliSleep scale
	// --- counters ---
	std::atomic<long> recv_calls{0}, recv_entered{0}, recv_returned{0};
	std::atomic<long> awake_attempts{0}, awake_failed{0}, posts{0}, drained{0};
	std::atomic<long> w_main{0}, w_poll{0}, w_other{0};   // widget calls by role
	std::atomic<long> show_main{0}, show_off{0};
	std::atomic<long> connect_calls{0};
	// --- timed socket (patch 8, T1-T5) ---
	// timed = true: recv() returns only what has "arrived" (fake_socket_inject
	// or replies scheduled by send()); send() looks up reply_script.
	std::atomic<bool> timed{false};
	// reply to the n-th send (1-based): {reply, delay_ms}; delay < 0 = no reply.
	// Replies are delivered in order (FIFO, like a radio's serial line).
	std::function<std::pair<std::string, int>(const std::string &cmd, long n)> reply_script;
	std::atomic<long> rcv_hold_until_sends{0};   // receive thread's recv() gets nothing until sends >= this
	std::atomic<bool> recv_throw_on_sender{false};// recv() on a non-receive thread throws (T5)
	std::atomic<long> sends{0}, replies_scheduled{0}, chunks_delivered{0};
	std::atomic<long> recv_calls_sender{0};      // recv() calls from a thread other than the receive thread
	std::atomic<long> recv_held{0};              // receive-thread recv() calls held back with data waiting
	std::atomic<long> recv_throws{0};
	std::atomic<long> log_discard{0}, log_total{0};   // fake LOG_*: messages, and those mentioning "discard"
};
extern FakeCtl fake_ctl;

// thread roles
void fake_set_main_thread();           // call first thing in main()
void fake_set_role(const char *role);  // e.g. "poll" or "B" (thread_local)
const char *fake_role();
bool fake_is_main();

// fd() returns -1 when called from a thread whose role is this string (U8)
void fake_fd_minus_one_for_role(const char *role);

// timed socket: data arrives in the fake socket after delay_ms (FIFO order)
void fake_socket_inject(const std::string &data, int delay_ms);
size_t fake_socket_pending();          // bytes in flight or arrived, not yet recv'd
// fake LOG_* sink (debug.h)
void fake_log(const char *fmt, ...);

// main-thread drain of the fake Fl::awake queue; returns callbacks run
int fake_drain();
// distinct colours applied to box_tcpip_connect, in order, by any thread
std::vector<Fl_Color> fake_colour_history();
// set all four widgets' colour without recording a call (test precondition)
void fake_preset(Fl_Color c);
// lock used by the fakes (exposed so the test can read widget state safely)
void fake_lock();
void fake_unlock();
