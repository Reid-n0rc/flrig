// Unit tests U1-U10 and P5 (tcp-fixes/patch7-plan.md, section 6) and T1-T5
// (tcp-fixes/patch8-plan.md) for the REAL
// src/support/socket_io.cxx, built against the fakes in fakes/ and fakes.cxx.
// One test per process: unit_socketio <TEST> [label]. Prints one line
//   <TEST> PASS|FAIL <numbers>
// and exits 0 (PASS) or 1 (FAIL). Only functions present in old and new code
// are called: connect_to_remote, disconnect_from_remote, send_to_remote, and
// the receive thread that connect_to_remote starts.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <unistd.h>
#include <pthread.h>
#include <FL/Fl.H>
#include "fake_ctl.h"
#include "socket_io.h"
#include "rigpanel.h"
#include "status.h"

static const char *cname(Fl_Color c)
{
	switch (c) {
	case FL_GREEN: return "green";
	case FL_YELLOW: return "yellow";
	case FL_LIGHT1: return "light";
	case FL_BACKGROUND2_COLOR: return "bg2";
	default: { static char b[16]; snprintf(b, sizeof b, "%u", (unsigned)c); return b; }
	}
}

static long now_ms()
{
	struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000L + t.tv_nsec / 1000000L;
}

// main-thread "event loop": drain the fake Fl::awake queue every 200 us until
// pred() is true or timeout; returns false on timeout
template <class P> static bool run_until(P pred, long timeout_ms = 15000)
{
	long t0 = now_ms();
	for (;;) {
		fake_drain();
		if (pred()) return true;
		if (now_ms() - t0 > timeout_ms) return false;
		usleep(200);
	}
}
static void run_for(long ms) { long t0 = now_ms(); run_until([&] { return now_ms() - t0 >= ms; }); }

struct Res { bool ok = true; std::string why; void need(bool c, const char *w) { if (!c) { ok = false; if (!why.empty()) why += ","; why += w; } } };

static long off_main() { return fake_ctl.w_poll + fake_ctl.w_other; }

static void finish(const char *t, Res &r, const std::string &extra = "")
{
	printf("%s %s off_main=%ld (poll=%ld rcv=%ld) main=%ld posts=%ld awake_fail=%ld drained=%ld recv=%ld "
	       "tcpip_connect=%s xcvr=%s menu=%s shown(main=%ld off=%ld)%s%s%s\n",
	       t, r.ok ? "PASS" : "FAIL", off_main(), fake_ctl.w_poll.load(), fake_ctl.w_other.load(),
	       fake_ctl.w_main.load(), fake_ctl.posts.load(), fake_ctl.awake_failed.load(),
	       fake_ctl.drained.load(), fake_ctl.recv_calls.load(),
	       cname(box_tcpip_connect->color()), cname(box_xcvr_connect->color()),
	       cname(tcpip_menu_box->color()), fake_ctl.show_main.load(), fake_ctl.show_off.load(),
	       extra.empty() ? "" : " ", extra.c_str(), r.ok ? "" : (" why=" + r.why).c_str());
	fflush(stdout);
	_exit(r.ok ? 0 : 1);   // no teardown: the receive thread may still run
}

static bool all_colour(Fl_Color c)
{
	return box_tcpip_connect->color() == c && box_xcvr_connect->color() == c && tcpip_menu_box->color() == c;
}

// V2 common: fakes exercised, and "0 off-main" only counts with main calls > 0
static void common(Res &r, long min_recv)
{
	r.need(fake_ctl.recv_calls >= min_recv, "recv_not_exercised");
	r.need(fake_ctl.w_main > 0, "no_main_widget_calls");
	r.need(off_main() == 0, "widgets_touched_off_main");
	r.need(fake_ctl.posts >= 1, "no_awake_post");
	r.need(fake_ctl.drained >= 1, "awake_queue_not_drained");
}

static bool try_connect()
{
	try { connect_to_remote(); return true; } catch (...) { return false; }
}

struct PollThread {
	pthread_t t; void (*fn)();
	static void *run(void *p) { fake_set_role("poll"); ((PollThread *)p)->fn(); return 0; }
	void start(void (*f)()) { fn = f; pthread_create(&t, 0, run, this); }
	void join() { pthread_join(t, 0); }
};

// --------------------------------------------------------------------------
static void U1()
{
	Res r; fake_ctl.sleep_us_per_ms = 200;
	r.need(try_connect(), "connect_failed");
	r.need(run_until([] { return fake_ctl.recv_calls >= 20; }), "timeout");
	run_for(20);
	common(r, 20);
	r.need(all_colour(FL_GREEN), "not_green");
	r.need(tcpip_box->visible(), "tcpip_box_not_shown");
	finish("U1", r);
}

static void U2()
{
	Res r; fake_ctl.sleep_us_per_ms = 50;
	r.need(try_connect(), "connect_failed");
	r.need(run_until([] { return fake_ctl.recv_calls >= 1000; }), "timeout");
	run_for(20);
	common(r, 1000);
	r.need(fake_ctl.posts == 1, "posts_not_exactly_1");
	r.need(all_colour(FL_GREEN), "not_green");
	finish("U2", r);
}

static void U3()
{
	Res r; fake_ctl.sleep_us_per_ms = 200;
	fake_ctl.recv_script = [](long n) { return (n > 30 && n <= 60) ? RECV_THROW : RECV_DATA; };
	r.need(try_connect(), "connect_failed");
	r.need(run_until([] { return fake_ctl.recv_calls >= 100; }), "timeout");
	run_for(20);
	common(r, 100);
	std::vector<Fl_Color> h = fake_colour_history();
	std::string hs;
	for (size_t i = 0; i < h.size(); i++) { if (i) hs += ">"; hs += cname(h[i]); }
	r.need(h.size() == 3 && h[0] == FL_GREEN && h[1] == FL_YELLOW && h[2] == FL_GREEN, "history_not_green>yellow>green");
	r.need(fake_ctl.posts >= 3, "fewer_than_3_posts");
	r.need(all_colour(FL_GREEN), "final_not_green");
	finish("U3", r, "history=" + hs);
}

static void U4()
{
	Res r; fake_ctl.sleep_us_per_ms = 200;
	PollThread p; p.start([] { try { connect_to_remote(); } catch (...) {} }); p.join();
	r.need(fake_ctl.connect_calls >= 1, "connect_not_called");
	r.need(run_until([] { return fake_ctl.recv_calls >= 20; }), "timeout");
	run_for(20);
	common(r, 20);
	r.need(fake_ctl.show_main >= 1 && fake_ctl.show_off == 0, "tcpip_box_show_not_on_main");
	r.need(all_colour(FL_GREEN), "not_green");
	finish("U4", r);
}

static void U5()
{
	Res r;
	fake_preset(FL_GREEN);                 // display still green from before the drop
	tcpip = new Socket(-1);                // socket left closed by a dropped link
	fake_ctl.connect_addr_throws = true;   // remote refuses the reconnect
	PollThread p; p.start([] { send_to_remote("FA;"); }); p.join();
	run_for(30);
	r.need(fake_ctl.connect_calls >= 1, "connect_not_called");
	common(r, 0);
	r.need(all_colour(FL_LIGHT1), "final_not_light");
	finish("U5", r);
}

static void U6()
{
	Res r; fake_ctl.sleep_us_per_ms = 200;
	r.need(try_connect(), "connect_failed");
	r.need(run_until([] { return fake_ctl.recv_calls >= 20; }), "timeout");
	PollThread p; p.start([] { disconnect_from_remote(); }); p.join();
	long rc = fake_ctl.recv_calls;
	run_for(30);
	r.need(fake_ctl.recv_calls == rc, "receive_thread_still_running");
	common(r, 20);
	r.need(all_colour(FL_LIGHT1), "final_not_light");
	finish("U6", r);
}

static void U7()
{
	const int K = 3;
	Res r; fake_ctl.sleep_us_per_ms = 200; fake_ctl.awake_fail_first = K;
	r.need(try_connect(), "connect_failed");
	r.need(run_until([] { return fake_ctl.recv_calls >= 50; }), "timeout");
	run_for(20);
	common(r, 50);
	r.need(fake_ctl.awake_failed == K, "awake_minus1_not_hit_K_times");
	r.need(all_colour(FL_GREEN), "final_not_green");
	char b[64]; snprintf(b, sizeof b, "K=%d attempts=%ld", K, fake_ctl.awake_attempts.load());
	finish("U7", r, b);
}

static std::atomic<bool> b_done{false};
static void U8()
{
	const int N = 10000;
	Res r; fake_ctl.sleep_us_per_ms = 10;
	fake_ctl.recv_script = [](long) { return RECV_THROW; };   // receive thread: yellow
	fake_fd_minus_one_for_role("B");                          // thread B: reconnects, green
	r.need(try_connect(), "connect_failed");
	struct B { static void *run(void *) {
		fake_set_role("B");
		for (int i = 0; i < 10000; i++) { try { connect_to_remote(); } catch (...) {} }
		b_done = true; return 0; } };
	pthread_t tb; pthread_create(&tb, 0, B::run, 0);
	r.need(run_until([&] { return b_done && fake_ctl.recv_calls >= N; }, 120000), "timeout");
	pthread_join(tb, 0);
	long rc = fake_ctl.recv_calls;     // phase 2: only the receive thread (yellow) runs
	r.need(run_until([&] { return fake_ctl.recv_calls >= rc + 200; }), "timeout2");
	run_for(20);
	common(r, N);
	r.need(fake_ctl.posts >= 2, "fewer_than_2_posts");
	r.need(all_colour(FL_YELLOW), "final_not_last_state_yellow");
	char b[64]; snprintf(b, sizeof b, "B_calls=%d connect_calls=%ld", N, fake_ctl.connect_calls.load());
	finish("U8", r, b);
}

static void U9()
{
	Res r; fake_ctl.sleep_us_per_ms = 200;
	r.need(try_connect(), "connect_failed");
	r.need(run_until([] { return fake_ctl.recv_calls >= 20; }), "timeout");
	run_for(10);
	disconnect_from_remote();                       // main-thread caller, no drain yet
	long pending = fake_ctl.posts - fake_ctl.drained;
	Fl_Color before = box_tcpip_connect->color();
	run_for(10);                                    // next loop pass
	common(r, 20);
	r.need(pending >= 1, "no_post_pending_after_call");
	r.need(before == FL_GREEN, "applied_before_drain");
	r.need(all_colour(FL_LIGHT1), "final_not_light");
	char b[64]; snprintf(b, sizeof b, "pending_after_call=%ld before_drain=%s", pending, cname(before));
	finish("U9", r, b);
}

static void U10()
{
	Res r; fake_ctl.sleep_us_per_ms = 200;
	r.need(try_connect(), "connect_failed");
	r.need(run_until([] { return fake_ctl.recv_calls >= 20; }), "timeout");
	run_for(10);
	box_xcvr_connect->color(FL_BACKGROUND2_COLOR);   // serial_failed(): "Transceiver not responding"
	long rc = fake_ctl.recv_calls;
	r.need(run_until([&] { return fake_ctl.recv_calls >= rc + 50; }), "timeout2");
	run_for(10);
	common(r, 70);
	r.need(box_xcvr_connect->color() == FL_BACKGROUND2_COLOR, "alert_colour_repainted");
	char b[48]; snprintf(b, sizeof b, "passes_after_alert=%ld", fake_ctl.recv_calls - rc);
	finish("U10", r, b);
}

// P5: disconnect while the receive thread is inside recv() (50 ms). The verdict
// on use-after-free comes from AddressSanitizer (run_tests.sh asan); this
// binary checks that the scenario really happened.
static void P5()
{
	Res r; fake_ctl.sleep_us_per_ms = 200; fake_ctl.recv_block_ms = 50;
	r.need(try_connect(), "connect_failed");
	r.need(run_until([] { return fake_ctl.recv_entered > fake_ctl.recv_returned; }), "timeout");
	bool in_recv = fake_ctl.recv_entered > fake_ctl.recv_returned;
	long t0 = now_ms();
	disconnect_from_remote();
	long dt = now_ms() - t0;
	long rc = fake_ctl.recv_calls;
	usleep(150000);
	r.need(in_recv, "not_inside_recv_at_disconnect");
	r.need(fake_ctl.recv_calls == rc && fake_ctl.recv_entered == fake_ctl.recv_returned, "receive_thread_not_stopped");
	char b[80]; snprintf(b, sizeof b, "in_recv_at_disconnect=%d disconnect_ms=%ld", (int)in_recv, dt);
	printf("P5 %s %s%s%s\n", r.ok ? "PASS" : "FAIL", b, r.ok ? "" : " why=", r.why.c_str());
	fflush(stdout);
	_exit(r.ok ? 0 : 1);
}

// ==========================================================================
// Patch 8 (tcp-fixes/patch8-plan.md): clear old TCP/IP input before each send.
// The fake socket is "timed" (fake_ctl.timed): send() schedules the reply that
// reply_script gives, delivered into the socket after delay_ms in FIFO order;
// recv() returns only what has arrived. The receive thread started by
// connect_to_remote() runs unscaled (MilliSleep(5) = 5 ms).

// MIRROR of rigbase::wait_char() (src/rigs/rigbase.cxx, TCP/IP branch), NOT the
// real rigbase: send_to_remote(cmd), then read_from_remote() every 1 ms,
// appending, until the reply holds ';' or the timeout passes (the timeout is
// pushed out by serial_timeout whenever data comes in, as in wait_char).
static std::string wait_reply(const std::string &cmd, int timeout_ms = 100, int serial_timeout_ms = 50)
{
	std::string reply;
	send_to_remote(cmd);
	long tout = now_ms() + timeout_ms;
	do {
		std::string tmp;
		int n = read_from_remote(tmp);
		if (n) { reply += tmp; tout = now_ms() + serial_timeout_ms; }
		if (reply.find(';') != std::string::npos) break;
		usleep(1000);
	} while (now_ms() < tout);
	return reply;
}

static void t_setup()
{
	fake_ctl.timed = true;
	fake_ctl.sleep_us_per_ms = 1000;      // real time: the receive thread polls every 5 ms
}

// V2 for the T tests: the fake socket was really used
static void t_common(Res &r, long sends, long delivered)
{
	r.need(fake_ctl.sends >= sends, "sends_not_exercised");
	r.need(fake_ctl.replies_scheduled >= 1, "no_reply_scheduled");
	r.need(fake_ctl.chunks_delivered >= delivered, "replies_not_delivered");
	r.need(fake_ctl.recv_calls >= 2, "recv_not_exercised");
}

static std::string t_numbers(const std::string &reply)
{
	char b[256];
	snprintf(b, sizeof b, "reply=\"%s\" sends=%ld scheduled=%ld delivered=%ld recv=%ld recv_by_sender=%ld held=%ld "
	         "throws=%ld log_discard=%ld",
	         reply.c_str(), fake_ctl.sends.load(), fake_ctl.replies_scheduled.load(), fake_ctl.chunks_delivered.load(),
	         fake_ctl.recv_calls.load(), fake_ctl.recv_calls_sender.load(), fake_ctl.recv_held.load(),
	         fake_ctl.recv_throws.load(), fake_ctl.log_discard.load());
	return b;
}

static const char *STALE = "FA00007000000;";
static const char *FRESH = "FA00014070000;";

// T1: stale reply already in rxbuffer when a command is sent
static void T1()
{
	Res r; t_setup();
	fake_ctl.reply_script = [](const std::string &, long) { return std::make_pair(std::string(FRESH), 0); };
	r.need(try_connect(), "connect_failed");
	fake_socket_inject(STALE, 0);
	r.need(run_until([] { return fake_ctl.chunks_delivered >= 1; }, 2000), "stale_not_received");  // now in rxbuffer
	usleep(20000);
	std::string reply = wait_reply("FA;");
	t_common(r, 1, 1);
	r.need(reply == FRESH, "reply_not_only_new");
	finish("T1", r, t_numbers(reply));
}

// T2: stale reply still in the socket (the receive thread has not taken it)
// when a command is sent. The receive thread's recv() is held back until the
// first send; any other thread's recv() (a flush in send_to_remote) is not.
static void T2()
{
	Res r; t_setup();
	fake_ctl.reply_script = [](const std::string &, long) { return std::make_pair(std::string(FRESH), 0); };
	fake_ctl.rcv_hold_until_sends = 1;
	r.need(try_connect(), "connect_failed");
	fake_socket_inject(STALE, 0);
	r.need(run_until([] { return fake_ctl.recv_held >= 3; }, 2000), "receive_thread_not_held");
	std::string reply = wait_reply("FA;");
	t_common(r, 1, 1);
	r.need(fake_ctl.recv_held >= 3, "stale_not_waiting_in_socket");
	r.need(reply == FRESH, "reply_not_only_new");
	finish("T2", r, t_numbers(reply));
}

// T3: 20 command/reply cycles (timeout 100 ms, 60 ms between cycles, as a poll
// loop); reply 5 arrives 130 ms after its command, i.e. after its timeout and
// before command 6. Replies are FIFO.
static void T3()
{
	const int N = 20, LATE = 5;
	Res r; t_setup();
	fake_ctl.reply_script = [](const std::string &cmd, long n) {
		return std::make_pair(cmd, n == LATE ? 130 : 0); };   // echo "IDnn;"
	r.need(try_connect(), "connect_failed");
	usleep(20000);
	int wrong = 0, off_by_one = 0, first_wrong = 0;
	std::string seq;
	for (int i = 1; i <= N; i++) {
		char c[16], prev[16];
		snprintf(c, sizeof c, "ID%02d;", i);
		snprintf(prev, sizeof prev, "ID%02d;", i - 1);
		std::string reply = wait_reply(c, 100, 50);
		if (reply != c) {
			wrong++; if (!first_wrong) first_wrong = i;
			if (reply == prev) off_by_one++;
			seq += (seq.empty() ? "" : ",") + std::to_string(i) + ":" + (reply.empty() ? "-" : reply.substr(0, 4));
		}
		usleep(60000);
	}
	t_common(r, N, N);
	r.need(wrong <= 1, "more_than_1_wrong_reply");
	char b[96]; snprintf(b, sizeof b, "cycles=%d late=#%d wrong=%d off_by_one=%d first_wrong=%d ", N, LATE, wrong, off_by_one, first_wrong);
	finish("T3", r, b + t_numbers("") + " wrong_list=" + seq);
}

// T4: no stale data: 10 normal command/reply cycles; all correct, nothing
// logged as discarded
static void T4()
{
	const int N = 10;
	Res r; t_setup();
	fake_ctl.reply_script = [](const std::string &cmd, long n) { return std::make_pair(cmd, (int)(n % 2 ? 0 : 3)); };
	r.need(try_connect(), "connect_failed");
	usleep(20000);
	int wrong = 0;
	for (int i = 1; i <= N; i++) {
		char c[16]; snprintf(c, sizeof c, "ID%02d;", i);
		if (wait_reply(c) != c) wrong++;
		usleep(10000);
	}
	t_common(r, N, N);
	r.need(wrong == 0, "wrong_reply");
	r.need(fake_ctl.log_discard == 0, "discard_logged");
	char b[48]; snprintf(b, sizeof b, "cycles=%d wrong=%d ", N, wrong);
	finish("T4", r, b + t_numbers(""));
}

// T5: socket recv() throws when called from the sending thread (the flush):
// the send still happens, the reply is read, the link stays up, no crash.
// recv_by_sender=0 means the code under test does no flush (n/a).
static void T5()
{
	Res r; t_setup();
	fake_ctl.reply_script = [](const std::string &, long) { return std::make_pair(std::string(FRESH), 0); };
	fake_ctl.recv_throw_on_sender = true;
	r.need(try_connect(), "connect_failed");
	usleep(20000);
	std::string reply = wait_reply("FA;");   // an escaping exception aborts = FAIL
	t_common(r, 1, 1);
	r.need(fake_ctl.sends == 1, "send_not_done");
	r.need(reply == FRESH, "reply_wrong");
	r.need(tcpip && tcpip->fd() != -1, "link_dropped");
	finish("T5", r, t_numbers(reply) + (fake_ctl.recv_calls_sender == 0 ? " flush=n/a" : " flush=exercised"));
}

int main(int argc, char **argv)
{
	fake_set_main_thread();
	const char *t = argc > 1 ? argv[1] : "";
	struct { const char *n; void (*f)(); } tests[] = {
		{"U1", U1}, {"U2", U2}, {"U3", U3}, {"U4", U4}, {"U5", U5}, {"U6", U6},
		{"U7", U7}, {"U8", U8}, {"U9", U9}, {"U10", U10}, {"P5", P5},
		{"T1", T1}, {"T2", T2}, {"T3", T3}, {"T4", T4}, {"T5", T5} };
	for (auto &e : tests) if (!strcmp(e.n, t)) { e.f(); return 0; }
	fprintf(stderr, "usage: %s U1..U10|P5|T1..T5\n", argv[0]);
	return 2;
}
