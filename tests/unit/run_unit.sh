#!/bin/bash
# Unit tests: each one builds one REAL flrig source file with stubs or fakes
# and runs it. No radio, emulator, socket or flrig window is used.
#
# usage: tests/unit/run_unit.sh [-r SRC_ROOT] [-o OUT] [SUITE ...]
#   SRC_ROOT  tree whose src/ is tested (default: this checkout)
#   SUITE     rig_io socket_io socket_io_asan socket_io_tsan debug_log
#             debug_log_tsan (default: every suite this platform supports)
# env: CXX (default clang++, else g++), FLTK_CONFIG (default fltk-config),
#      REPS (runs of each socket_io / debug_log test, default 3),
#      EXTRA_CXXFLAGS (added last, e.g. "-O0 --coverage")
# Exit status: number of failed suites (0 = all passed).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
OUT=""
while getopts "r:o:" o; do
	case $o in r) ROOT=$(cd "$OPTARG" && pwd);; o) OUT=$OPTARG;; *) exit 64;; esac
done
shift $((OPTIND - 1))
OUT=${OUT:-$HERE/out}
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd)
FLTK_CONFIG=${FLTK_CONFIG:-fltk-config}
REPS=${REPS:-3}
if [ -z "${CXX:-}" ]; then
	if command -v clang++ > /dev/null; then CXX=clang++; else CXX=g++; fi
fi
OS=$(uname -s)
case $OS in MINGW*|MSYS*|CYGWIN*) WIN=1;; *) WIN=0;; esac

if [ $# -eq 0 ]; then
	set -- rig_io socket_io debug_log
	# sanitizers: not with MinGW
	[ $WIN = 0 ] && set -- "$@" socket_io_asan socket_io_tsan debug_log_tsan
fi

SRC="$ROOT/src"
FLTK_CXX=$($FLTK_CONFIG --cxxflags)
FLTK_LD=$($FLTK_CONFIG --ldflags)
CFG="$OUT/cfg"; mkdir -p "$CFG"
printf '#define PACKAGE "flrig"\n#define VERSION "unit-test"\n' > "$CFG/config.h"
REAL_INC="-I$CFG -I$SRC/include -I$SRC/xmlrpcpp $FLTK_CXX"
LIBS="-lpthread"; [ $WIN = 1 ] && LIBS="$LIBS -lws2_32"

FAILED=0
result() {	# suite PASS|FAIL detail
	printf '%-16s %s  %s\n' "$1" "$2" "$3" | tee -a "$OUT/summary.txt"
	[ "$2" = PASS ] || FAILED=$((FAILED + 1))
}

build() {	# out_binary log flags sources...
	local bin=$1 log=$2 flags=$3; shift 3
	if ! $CXX -std=gnu++11 -g -w $flags ${EXTRA_CXXFLAGS:-} "$@" -o "$bin" > "$log" 2>&1; then
		echo "--- build failed: $log"; cat "$log"; return 1
	fi
}

# rig_io.cxx readResponse()/sendCommand() over TCP/IP (T1-T5)
suite_rig_io() {
	local d="$OUT/rig_io"; mkdir -p "$d"
	build "$d/unit_rigio" "$d/build.log" "-O1 $REAL_INC" "$SRC/support/rig_io.cxx" \
		"$HERE/rig_io/stubs.cxx" "$HERE/rig_io/test_main.cxx" $FLTK_LD $LIBS \
		|| { result rig_io FAIL "build failed"; return; }
	"$d/unit_rigio" rig_io 20 > "$d/result.txt" 2>&1
	local rc=$?
	cat "$d/result.txt"
	local bad; bad=$(grep -E '^T[0-9]+ FAIL' "$d/result.txt" | cut -d' ' -f1 | tr '\n' ' ')
	[ $rc -eq 0 ] && result rig_io PASS "T1-T5" || result rig_io FAIL "exit $rc ${bad}"
}

# socket_io.cxx with fake sockets and widgets (U1-U10, T1-T5, P5)
SIO_TESTS="U1 U2 U3 U4 U5 U6 U7 U8 U9 U10 T1 T2 T3 T4 T5"
suite_socket_io_kind() {	# plain|asan|tsan
	local kind=$1 d="$OUT/socket_io_$1" f="-O1" tests=$SIO_TESTS env=""
	case $kind in
		asan) f="-O1 -fsanitize=address -fno-omit-frame-pointer"; tests="P5"
		      env="ASAN_OPTIONS=detect_leaks=0";;
		tsan) f="-O1 -fsanitize=thread"; tests="U8"
		      env="TSAN_OPTIONS=halt_on_error=0 report_signal_unsafe=0";;
	esac
	mkdir -p "$d"
	build "$d/unit" "$d/build.log" "$f -I$HERE/socket_io/fakes $FLTK_CXX" \
		"$SRC/support/socket_io.cxx" "$HERE/socket_io/fakes.cxx" \
		"$HERE/socket_io/test_main.cxx" $LIBS \
		|| { result "socket_io_$kind" FAIL "build failed"; return; }
	local t i log rc bad=""
	for t in $tests; do
		for i in $(seq 1 "$REPS"); do
			log="$d/${t}_$i.log"
			env $env "$d/unit" "$t" > "$log" 2>&1; rc=$?
			if [ $rc -ne 0 ] || ! grep -qE "^$t PASS" "$log" \
			   || grep -qE "ERROR: AddressSanitizer|WARNING: ThreadSanitizer" "$log"; then
				bad="$bad $t#$i"
				echo "--- $t run $i failed (exit $rc):"; tail -20 "$log"
			fi
		done
	done
	[ -z "$bad" ] && result "socket_io_$kind" PASS "$(echo $tests) x$REPS" \
		|| result "socket_io_$kind" FAIL "failed:$bad"
}
suite_socket_io()      { suite_socket_io_kind plain; }
suite_socket_io_asan() { suite_socket_io_kind asan; }
suite_socket_io_tsan() { suite_socket_io_kind tsan; }

# debug.cxx: 4 threads logging at once must neither crash nor race
suite_debug_log_kind() {	# plain|tsan
	local kind=$1 d="$OUT/debug_log_$1" f="-O2" extra="" env=""
	if [ "$kind" = tsan ]; then
		f="-O1 -fsanitize=thread"
		env="TSAN_OPTIONS=halt_on_error=0 abort_on_error=0 report_signal_unsafe=0"
		# macOS TSan does not intercept vsnprintf; the shims make the writes visible
		[ "$OS" = Darwin ] && extra="$HERE/debug_log/tsan_shims.cxx"
	fi
	mkdir -p "$d"
	build "$d/unit" "$d/build.log" "$f $REAL_INC" "$SRC/support/debug.cxx" \
		"$HERE/debug_log/stubs.cxx" "$HERE/debug_log/test_main.cxx" $extra \
		$FLTK_LD $LIBS || { result "debug_log_$kind" FAIL "build failed"; return; }
	local i rc bad="" n=$REPS
	[ "$kind" = plain ] && n=$((REPS * 5))
	for i in $(seq 1 "$n"); do
		env $env "$d/unit" 4 2000 "$d/debug_test_$i.log" > "$d/run_$i.txt" 2>&1; rc=$?
		if [ $rc -ne 0 ] || grep -q "WARNING: ThreadSanitizer" "$d/run_$i.txt"; then
			bad="$bad #$i"
			echo "--- run $i failed (exit $rc):"; grep -m3 -A12 "WARNING: ThreadSanitizer" "$d/run_$i.txt" || tail -5 "$d/run_$i.txt"
		fi
	done
	[ -z "$bad" ] && result "debug_log_$kind" PASS "4 threads x 2000 logs, $n runs" \
		|| result "debug_log_$kind" FAIL "failed runs:$bad"
}
suite_debug_log()      { suite_debug_log_kind plain; }
suite_debug_log_tsan() { suite_debug_log_kind tsan; }

: > "$OUT/summary.txt"
echo "== unit tests on $SRC ($OS, $($CXX --version | head -1), FLTK $($FLTK_CONFIG --version))"
for s in "$@"; do
	echo "== $s"
	"suite_$s"
done
echo "== summary"
cat "$OUT/summary.txt"
exit $FAILED
