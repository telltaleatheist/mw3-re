# Running the differential tests at 53 bits

The reference runtime's x87 runs at 53 bits (CLAUDE.md, FPU precision). `MW3_PRECISION=53` runs a
differential test there on both sides, whatever the test's own defaults:

- the emulator: `Original.fpu_control` keeps the precision bits at 53, also for the tests that
  call it with 0x007f in their own code (66 files), and `MW3_X87=53` covers the rest;
- the port: `MW3_FPU53=1`, read by `probe.cpp` when the library loads, so `zfix fpu53` is on.

At exit `original.py` reads both back, prints `precision: emulator [...], port fpu53 N` and exits 3
with `precision: NOT at 53 bits` if either is not there. `test_precision_setting.py` tests that.
It came from `test_pm_collide.py`, which set the emulator only and so compared nothing new.

Without the variable nothing changes: tests keep their own precision.
