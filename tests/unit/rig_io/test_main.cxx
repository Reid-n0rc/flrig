// Unit test of rig_io.cxx readResponse()/sendCommand() over TCP/IP, with a fake
// remote in place of socket_io.cxx. No radio, no emulator, no sockets.
// Prints "Tn PASS|FAIL ..." per test and exits 1 if any test fails.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>
#include "status.h"
#include "socket_io.h"
#include "rig_io.h"

extern status progStatus;
extern std::string respstr;

// ---- fake remote ---------------------------------------------------------
// Each chunk becomes available either at the Nth read_from_remote() call
// (by_call) or N ms after the last send_to_remote() (never before a send).
struct Chunk { bool by_call; long at; std::string data; bool done; };
static std::vector<Chunk> script;
static int reads = 0, sends = 0;
static std::chrono::steady_clock::time_point t_ref;
static long ms_since(std::chrono::steady_clock::time_point t) {
	return (long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t).count();
}
static void set_script(std::vector<Chunk> s) { script = s; reads = sends = 0; t_ref = std::chrono::steady_clock::now(); }

// same contract as socket_io.cxx: str = what arrived since the last call; buffer cleared
int read_from_remote(std::string &str) {
	str.clear();
	++reads;
	for (auto &c : script) {
		if (c.done) continue;
		if (c.by_call ? reads >= c.at : (sends > 0 && ms_since(t_ref) >= c.at)) { str += c.data; c.done = true; }
	}
	return (int)str.length();
}
void send_to_remote(std::string) { ++sends; t_ref = std::chrono::steady_clock::now(); }

// ---- helpers -------------------------------------------------------------
static std::string show(const std::string &s) {
	std::string o = "\"";
	for (unsigned char c : s) { char b[8]; if (c < 32 || c > 126) { snprintf(b, sizeof b, "\\x%02X", c); o += b; } else o += (char)c; }
	return o + "\"";
}
template <class F> static long timed(F f) { auto t = std::chrono::steady_clock::now(); f(); return ms_since(t); }

static int failed = 0;
static void verdict(const char *t, bool ok, const char *why) {
	printf("%s %s%s%s\n", t, ok ? "PASS" : "FAIL", ok ? "" : " - ", ok ? "" : why);
	if (!ok) failed++;
}

int main(int argc, char **argv) {
	const char *label = argc > 1 ? argv[1] : "rig_io";
	progStatus.use_tcpip = true;
	progStatus.tcpip_ping_delay = 50;
	progStatus.xmlrpc_rig = false;
	int N = argc > 2 ? atoi(argv[2]) : 20;
	const std::string FA = "FA007100000;";

	printf("== %s ==\n", label);

	// 1. set command, nread 0: median wall time of N calls.
	// 2.0.12.05 reads 100 x 10 ms (about 1.2 s) before every set command.
	{
		std::vector<long> t;
		for (int i = 0; i < N; i++) { set_script({}); t.push_back(timed([] { sendCommand("FA00714000;", 0); })); }
		std::sort(t.begin(), t.end());
		long med = t[t.size() / 2];
		printf("   sendCommand(set,0)          median %ld ms (min %ld, max %ld, n=%d), reads/call %d\n",
			med, t.front(), t.back(), N, reads);
		verdict("T1", med < 500, "a set command over TCP/IP waits for a reply that never comes");
	}
	// 2. readResponse(), no terminator, reply already waiting
	{
		set_script({ {true, 1, FA, false} });
		int n = 0; long ms = timed([&] { n = readResponse(); });
		printf("   readResponse() reply ready  %ld ms, ret %d, respstr %s, reads %d\n", ms, n, show(respstr).c_str(), reads);
		verdict("T2", n == 12 && respstr == FA && ms < 500, "reply lost or late");
	}
	// 3. split reply: "FA0071" on read 1, "00000;" on read 3
	{
		set_script({ {true, 1, "FA0071", false}, {true, 3, "00000;", false} });
		int n = 0; long ms = timed([&] { n = readResponse(";", "\xFD"); });
		printf("   readResponse(;,FD) split    %ld ms, ret %d, respstr %s, reads %d\n", ms, n, show(respstr).c_str(), reads);
		verdict("T3", n == 12 && respstr == FA, "first piece of a split reply lost");
	}
	// 4. terminator given, nothing arrives: times out and returns 0
	{
		set_script({});
		int n = 0; long ms = timed([&] { n = readResponse(";", "\xFD"); });
		printf("   readResponse(;,FD) silent   %ld ms, ret %d, respstr %s, reads %d\n", ms, n, show(respstr).c_str(), reads);
		verdict("T4", n == 0 && respstr.empty(), "silent remote returned data");
	}
	// 5. query, nread 10: reply arrives 40 ms after the send (inside the 50 ms ping delay)
	{
		set_script({ {false, 40, FA, false} });
		int n = 0; long ms = timed([&] { n = sendCommand("FA;", 10); });
		printf("   sendCommand(query,10)       %ld ms, ret %d, respstr %s, reads %d, sends %d\n", ms, n, show(respstr).c_str(), reads, sends);
		verdict("T5", n == 12 && respstr == FA && ms < 1000, "early reply lost");
	}
	return failed ? 1 : 0;
}
