# Design Decisions

Six engineering decisions from this project, each as decision → alternative
rejected → why. Distilled from [`docs/change-log.md`](change-log.md), which
has the full rationale and code for each — section references below point
there.

## 1. Compile-time mode dispatch, not runtime flags

**Decision:** All three control architectures (`HIGH_LEVEL`, `LOW_LEVEL`,
`HYBRID`) are selected by one `constexpr CtrlMode CTRL_MODE` in `config.h`,
and every branch on it uses `if constexpr`.

**Alternative rejected:** A runtime flag (read from a config file or CLI
arg) so the mode could be switched without recompiling.

**Why:** `if constexpr` deletes the other two modes' code paths entirely at
compile time — no dead branches shipped, no way for the protocol to
desync silently because a runtime flag drifted from what the firmware
actually compiled with. The cost is a mandatory recompile-and-reflash on
every mode change; the project accepts that cost explicitly (`README.md`,
`CLAUDE.md`) rather than build a runtime-safe mode switch for a two-repo
system where the two sides can already fall out of sync in a dozen other
ways. `com.cpp` stays mode-agnostic by construction, not by convention.

## 2. HYBRID as cascade control, not feedforward offset

**Decision:** In `HYBRID` mode, the PC's outer loop (K·x, ~200 Hz) computes
a **position reference** that the ESP32's inner PI loop (1 kHz) tracks —
true cascade control.

**Alternative rejected (and shipped first):** K·x as an additive
feedforward voltage added directly on top of the ESP32 PI output
(change-log §5, §20, §21 — "HYBRID mode design — UNDER REVISION" through
two stabilization patches).

**Why:** The feedforward design coupled outer-loop compute directly into
motor voltage, so any outer-loop jitter or USB latency injected directly
into torque. Cascade control decouples the two: the inner loop rejects
disturbances and tracks the reference at 1 kHz regardless of what the USB
link is doing, and the outer loop only has to update as fast as it can,
not as fast as the motor needs. The migration (§22) meant redefining the
3-byte wire packet's payload as a reference rather than a voltage
correction, encode/decode symmetric around byte 127 — and it exposed a
real bug in the process (§23: a `HYBRID_REF_MAX` mismatch between the two
`config.h` files caused a 7.5× reference amplification).

## 3. Timing instrumentation with a zero-cost off switch

**Decision:** `ENABLE_TIMING` (both `config.h` files, must match like
`CTRL_MODE`) gates all latency measurement — PC-side `pc_loop/proc/wait_us`
and ESP32-side per-packet `esp_comp/comm_us`, buffered and dumped via
`FLAG_DUMP` after STOP (change-log §25, §27, §29).

**Alternative rejected:** Always-on instrumentation, or inlining timing
fields into every 16-byte sensor packet.

**Why:** Extending the hot-path packet would slow every iteration during
the run just to carry diagnostic data most runs don't need. Buffering
on-device and dumping once after STOP keeps the runtime path unchanged;
wrapping the buffers and capture code in `if constexpr (ENABLE_TIMING)`
means `false` costs nothing — no allocation, no branch, no extra columns —
verified by the RAM delta itself (§27: +40 KB static when timing is
compiled in, ~5%→19% of SRAM).

## 4. Crash-safe experiment logging

**Decision:** `runControlLoop()` builds the sample vector with `push_back`
(no pre-sized zero-padding), installs a SIGINT handler that sets an
`std::atomic<bool>` polled every iteration, and always calls `saveToFile`
on exit — writing `# stopped: <reason>` into the CSV header (change-log
§9).

**Alternative rejected:** A pre-sized vector filled in and saved only on
clean completion (the original behavior) — and, briefly, a `RunResult`
wrapper struct around `{data, stop_reason}` (§9's "Note (revised)").

**Why:** The original design lost the entire run on Ctrl-C or a Hall
safety trip — a pre-sized, zero-padded vector plus no save-on-abort path
meant an interrupted run wrote nothing, or worse, wrote zero rows the
experimenter could mistake for real data. An out-parameter `stop_reason`
was chosen over the wrapper struct after the first version shipped,
specifically because the struct added indirection with no benefit for a
single-caller function — a small reversal, kept in the log rather than
silently redone.

## 5. Encoder zero anchored to the held pose

**Decision:** While the arm is stopped, encoders are continuously reset to
zero. Whatever pose the user is physically holding the arm in at the
moment `START` fires becomes `pos = 0`, and K·x regulates back to that
pose (change-log §12, restoring the pattern from §6/§2).

**Alternatives rejected — and tried on hardware:** (a) zero the encoders
once on the stop→run edge rather than continuously (§11), and (b) stop
zeroing altogether so `pos = 0` anchors to the ESP32's boot pose, a fixed
mechanical zero independent of the run (§12's original attempt).

**Why this is worth reading, not just the outcome:** Both alternatives
were implemented, hardware-tested, and reverted — twice, on the same day
(2026-05-04). Anchoring to boot pose sounded more "correct" (a fixed
reference instead of a moving one) but failed on the actual rig: DAC
neutral (`OUT_NEUTRAL_1/2` = "no torque") isn't the same as encoder zero
("mechanical middle"), so K·x drove the arm back to wherever it happened
to be at power-on, not any meaningful rest pose. The continuously-reset,
held-pose-tracking version is more code-simple and matches how the
original `ESP_serial.ino` behaved, and it's the version that actually
works for the lab's workflow: hold the arm where you want zero, then
press start. Documenting a reverted "fix" honestly, with the reasoning
for reverting it, is the point — see `docs/change-log.md` §11/§12 for the
full trace.

## 6. Dump-framing magic bytes against a real race condition

**Decision:** The end-of-run timing dump is framed with a 4-byte marker
(`DUMP_MAGIC_0..3 = A5 5A C3 3C`) before the `uint16` sample count, and the
PC scans for that marker before trusting the count (change-log §28).

**Alternative rejected:** Read the 2-byte count immediately after
requesting the dump, trusting that no stale sensor bytes are still
in-flight on the wire.

**Why:** That trust broke on hardware. A still-in-transit 16-byte sensor
packet could arrive between the PC's dump request and its count read, and
the sensor flag byte — `109`, i.e. `FLAG_SEND` — was misread as a dump
count of 109, corrupting the first ~109 rows with garbage timing values.
The fix hardens the wire format rather than trying to eliminate the race
by timing (which would just move the race, not remove it): the PC now
scans the stream for an unambiguous 4-byte marker no legitimate sensor
packet can produce, so any stale bytes are skipped rather than
misinterpreted.
