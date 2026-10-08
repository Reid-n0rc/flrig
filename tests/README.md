# flrig tests

No radio is needed. `.github/workflows/ci.yml` runs all of this on Linux,
macOS and Windows for every push and pull request.

## Unit tests (`tests/unit`)

Each test builds one real flrig source file with stubs or fakes in place of
the rest of flrig, and runs it:

| Suite | Source | What it checks |
|---|---|---|
| `rig_io` | `src/support/rig_io.cxx` | TCP/IP set commands don't wait 1 s for a reply; split and early replies are kept |
| `socket_io` | `src/support/socket_io.cxx` | widgets only touched on the main thread; old input discarded before a send |
| `socket_io_asan` | same | the receive thread is stopped before the socket is deleted (AddressSanitizer) |
| `socket_io_tsan` | same | no data races in connect/disconnect (ThreadSanitizer) |
| `debug_log` | `src/support/debug.cxx` | 4 threads logging at once don't crash |
| `debug_log_tsan` | same | and don't race (ThreadSanitizer) |

    FLTK_CONFIG=/path/to/fltk-config tests/unit/run_unit.sh [-r TREE] [SUITE ...]

`-r TREE` tests another tree's `src/` (CI uses it to run the tests on a pull
request's base branch, where a test for a new fix is expected to fail).
Sanitizer suites are skipped on Windows (MinGW).

## Emulator tests (`tests/emu`)

`ft710_emu.py` (timing measured on a real FT-710) and `ftx1_emu.py` (strict,
from the FTX-1 CAT manual 2508-C; answers `?;` to anything else) stand in for
the radios. `ci_emu.py` starts the emulator first, then flrig with its own
config directory, drives it over XML-RPC and stops it with `rig.shutdown`:

    python3 tests/emu/ci_emu.py path/to/flrig [SCENARIO ...]

A run fails if flrig doesn't come online or exit in time, crashes, sends a
command the emulator rejects, reads another command's reply, or reads 0 from
the power or SWR meter. Serial scenarios need a pseudo-terminal (Linux,
macOS); TCP/IP scenarios run everywhere. Logs: `tests/emu/runs/`.

`harness.py` (FT-710) and `ftx1_harness.py` can also be run by hand for
measurements; see their docstrings.

## Build helpers (`tests/ci`)

`build_fltk.sh PREFIX` builds FLTK 1.4.4; `build_mxe.sh DIR COMMIT` builds the
MXE toolchain flrig's Windows installer is made with (`scripts/buildmxe.sh`).
