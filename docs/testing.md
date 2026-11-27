# Tests

`build/launchertests` runs the launcher's tests; they need no game files. `MW3_TEST=a,b` runs only the
tests whose names contain one of those, `MW3_SKIP` leaves some out.

## The test ledger

A test that passed is not run again on the same code (2026-11-27).
`tools/test_ledger.py run build/launchertests --tests a,b --programs mw3launcher`
builds nothing. It fingerprints the code the test binary and the programs it starts are made from:
- every source, header and library Ninja lists for them;
- the build commands, without the version define;
- the compiler's version.
It skips each named test that already passed on that fingerprint, runs the rest in one process (`MW3_TEST`,
under `tests/differential/run_capped.py`), and adds each `ok` or `FAIL` to the ledger. Without `--tests` it covers
all of the binary's tests. `status` shows what passed on the current code. `--force` runs them anyway, and its
lines are marked forced. The ledger is `$MW3_TEST_LEDGER`, else the machine's shared one (`KEEP_LEDGER` in the
script), else `test-ledger.jsonl` in the build folder. The data the tests read is in the fingerprint too: every file
under `assets/`, always, and under each `--data` path. The game install (`game/`) is not. It reads Ninja builds
only (configure with `cmake -S . -B build -G Ninja`), and covers the test binaries that take `MW3_TEST`, not the tests ctest runs as CMake scripts.
`tools/test_ledger.py selftest` checks the tool itself.
