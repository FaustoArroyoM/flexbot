# FlexBot Change Log

> On-demand reference. Load with `@docs/change-log.md` when something
> seems weird and you want to know "why is it like this." Each entry
> documents a fix and the bug it addressed, so the rationale survives
> across sessions.

## 1. `write_control` moved outside the if/else in `cycle()`

**Bug:** Was only called inside the `else (!stop)` branch. On boot with
`stop=true`, the DAC was never written to neutral, causing random
voltages and a crash on startup.

**Fix:** `write_control` now always executes at the bottom of every
cycle tick, unconditionally.

## 2. `reset_pos_enc` changed from continuous to edge-triggered

**Bug:** `reset_pos_enc` was being called every 1 ms inside the `stop`
branch, constantly wiping the hardware PCNT counter. Even when the arm
was running, the encoder read 0 because Core 1 could set `stop=false`
while Core 0 still had one more reset tick queued.

**Fix:** Added `prev_stop` local variable to `cycle()`. `reset_pos_enc`
only fires once on the running→stopped transition:

```cpp
bool prev_stop = true;
// ...
const bool currently_stopped = state.stop;
if (currently_stopped) {
    state.out1_buffer = OUT_NEUTRAL_1;
    state.out2_buffer = OUT_NEUTRAL_2;
    if (!prev_stop) {                 // ← only on the falling edge
        reset_pos_enc(p1, p2);
        state.pos1 = p1;
        state.pos2 = p2;
    }
}
prev_stop = currently_stopped;
```

## 3. Double `vTaskDelayUntil` in `serialcomm()` removed

**Bug:** A refactor accidentally duplicated both the time measurement
and the `vTaskDelayUntil` call at the bottom of `serialcomm()`. The task
was sleeping 10 ms instead of 5 ms, and `communication_time` was being
overwritten after the sleep.

**Fix:** Deleted the duplicate block. Only one measurement + one delay
remains.

## 4. LOW_LEVEL `enable_transmit` deadlock fixed

**Bug:** In LOW_LEVEL, `enable_transmit` starts `false`. The ESP32 only
transmitted once (after receiving the start packet) then stopped forever
because the PC never sends another packet, so `enable_transmit` never
became `true` again.

**Fix:** In LOW_LEVEL's `serialcomm` branch, the `enable_transmit` gate
was removed entirely. ESP32 streams autonomously every tick while
`!stop`.


## 5. HYBRID mode design — UNDER REVISION

Original implementation used K·x as an additive feedforward voltage offset
on top of the ESP32 PI loop (NOT cascade). This design is being replaced.

Intended design: true cascade control.
  - Outer loop (PC, ~10ms): K·x computes a POSITION REFERENCE, not a voltage
  - Inner loop (ESP32 PI, ~1ms): tracks that position reference
  - PC sends ref1/ref2 as position targets, not DAC-scaled voltage corrections

Status: not yet implemented. Current serialcomm.cpp HYBRID branch still
sends voltage offsets. main.cpp HYBRID branch still adds ref to PI output.
Both need to change.

## 6. `reset_pos_enc` moved from falling to rising edge — OVERRULED 2026-04-30

**Original change (kept for context):** §2's falling-edge reset was
moved to the rising edge (stopped→running) so each run starts at
encoder zero regardless of inter-run drift. Hardware-verified
2026-04-30 (Test A passed).

**Why overruled:** Per user request — the rising-edge detector with
`prev_stop` tracking is more code than the original pattern, and the
original "continuously reset while stopped" pattern from `ESP_serial.ino`
achieves the same end result with simpler code. The race condition that
§2 was nominally guarding against (Core 1 setting `stop=false` while
Core 0 has a queued reset tick) only produces a 1-tick window where the
encoder might get reset one extra time at the moment of transition,
which is harmless: the next tick reads encoder normally, accumulating
from 0.

**Replaced by:** the original-style continuous reset, restored in the
`if (currently_stopped)` branch:

```cpp
if (currently_stopped) {
    state.out1_buffer = OUT_NEUTRAL_1;
    state.out2_buffer = OUT_NEUTRAL_2;
    float p1_reset = 0.0F, p2_reset = 0.0F;
    reset_pos_enc(p1_reset, p2_reset);
    state.pos1 = p1_reset;
    state.pos2 = p2_reset;
}
```

`prev_stop` tracking removed. Behavior: as long as `state.stop == true`
(boot state, between runs, after Hall trip), `cnt1`/`cnt2` are zeroed
every 1 ms tick. The first tick where serialcomm sets `stop=false`,
this branch stops firing and the encoder begins accumulating from 0.
**Note:** This continuous reset was later removed in §12.

## 7. `cycle_time_ms` row 0 zeroed on every START

**Bug:** §6 fixed encoders for every run, but the round-trip latency
field was still wrong on row 0. Initially we used a `bool first_receive`
local — that only zeroed `state.end_time` once after ESP32 boot.
Subsequent runs without a power-cycle reused stale `state.init_time`
from the previous run, producing arbitrary row 0 values.

**Fix:** Replaced the one-shot flag with rising-edge detection on
`state.stop` inside the receive branch of `serialcomm()`. Whenever a
START packet flips stop true→false, we reset both timing fields:

```cpp
const bool was_stopped = stp;     // before receive_control mutates
receive_control(out1, out2, stp);
state.stop = stp;
if (was_stopped && !stp) {        // rising edge = START packet
    state.end_time  = 0;
    state.init_time = 0;
} else {
    state.end_time = esp_timer_get_time() - state.init_time;
}
```

Same patch applied to the HYBRID receive branch. LOW_LEVEL never
updates `end_time` so it didn't need the patch.

## 8. PC-side homing phase — OVERRULED 2026-04-30

**Original change (kept for context):** Added a 5-phase
`runControlLoop`: START → hold neutral DAC for 200 samples → STOP+START
to retrigger encoder reset → main K·x loop → STOP.

**Why overruled:** Per user request — the homing approach didn't work
in practice (arm didn't visibly settle, double-START handshake was
fragile) and added significant complexity to a function that the user
wants to keep simple. The desire for "arm at gravity-rest = (0, 0)"
is deferred; for now, K·x is logged from whatever pose the arm is in
when START is sent. If the user wants to start runs from a specific
pose, the workflow is now: physically place the arm where you want
"(0, 0)" to be, then start the run.

**Replaced by:** Removed all homing phases. `runControlLoop()` is back
to a single linear flow: START → main K·x loop → STOP. No
`HOMING_SAMPLES`, no double-START handshake.

## 9. Graceful partial-CSV save with stop_reason

**Bug:** When the ESP32 stopped transmitting (Hall safety trip,
encoder out-of-bounds, or any timeout), `runControlLoop` `break`d
out of its loop and `saveToFile` wrote a CSV padded with zero-rows
up to `len`. Ctrl-C lost everything because `saveToFile` never ran.
No log entry told the user what went wrong.

**Fix:**
  - `runControlLoop` signature: `std::vector<Sample> runControlLoop(
    serialib&, int len, std::string& stop_reason)`. The vector is
    built with `push_back` (no pre-sized vector → no zero padding);
    `stop_reason` is an out-parameter the caller declares locally.
    No wrapper struct.
  - SIGINT handler installed in `main()`. Sets a `std::atomic<bool>`
    that the loop polls every iteration. Ctrl-C now flushes the
    partial CSV cleanly with `stop_reason = "user interrupt..."`.
  - `readBytes` short-read or wrong flag → loop breaks with
    `stop_reason = "ESP32 stopped transmitting at sample N (likely
    Hall safety trip or out-of-bounds)"`.
  - `saveToFile` writes `# stopped: <reason>` as a CSV header
    comment, alongside `# samples: N`. pandas with `comment='#'`
    strips both.

**Note (revised 2026-04-30):** Original implementation used a
`RunResult { data, stop_reason }` struct. Replaced by the
out-parameter form above, per user feedback that the wrapper struct
added complexity without benefit.

## 10. Timestamp format — seconds + readable

**Cosmetic:** `output_20260430_1354.csv` → `output_2026-04-30_13-54-12.csv`.
Sortable, human-readable, distinguishable runs within the same minute.
One-line `strftime` format change in `serialcomm.cpp`.

## 11. START-edge encoder reset across cores — OVERRULED 2026-05-04

**Bug:** After refactoring, the continuous reset while stopped no longer
guaranteed `pos1/pos2 == 0` on the very first running tick. If encoder
counts incremented between the last stopped tick and the first running
tick, the first sample could start non-zero.

**Fix:** Added a `state.reset_encoders` flag that is set in `serialcomm()`
on the rising edge of START (all modes). The `cycle()` task consumes that
flag on the first running tick, calls `reset_pos_enc`, writes `pos1/pos2`,
and clears the flag. Continuous reset while `state.stop == true` remains.

**Why overruled:** User clarified the controller should NOT treat the start
pose as the rest pose. Encoder zeroing on stop/start was removed so K·x
references a fixed zero instead of the start pose.

## 12. Encoder zeroing while stopped removed — OVERRULED 2026-05-04

**Bug:** The controller treated the start pose as the equilibrium because
encoders were forcibly zeroed when `state.stop == true`. Each run redefined
zero, so K·x would hold whatever pose the arm was in at START.

**Fix:** Removed encoder zeroing while stopped (and the START-edge reset).
While stopped, DAC outputs remain neutral and `pos1/pos2` are updated from
the encoders so the start pose is preserved. K·x now references the fixed
encoder zero instead of the start pose.

**Why overruled:** With encoders no longer zeroed at START, `pos = 0`
became anchored to ESP32 boot pose (struct init `cnt1 = cnt2 = 0`). K·x
then drove the arm back to that boot pose on every run, regardless of
where the user was holding the arm. Writing `OUT_NEUTRAL_1/2` to the DAC
does not move the arm to any "middle" pose either — DAC neutral and
encoder neutral are independent: 127/125 means "no torque," not "go to
middle." After comparing to `ESP_serial.ino`, the older approach is
strictly better for the actual workflow: continuously reset `cnt1/cnt2`
while `state.stop == true`, so the encoder zero "tracks" wherever the
user is holding the arm. When START fires, `pos = 0` corresponds to the
held pose, and K·x regulates back to that pose. To make the arm
regulate to the mechanical middle, the user simply holds the arm in
the middle before pressing START. See change-log §6 for the same
restoration earlier in the project history.

**Replaced by:** restoration of the §6 pattern in `cycle()`:

```cpp
if (state.stop) {
    state.out1_buffer = OUT_NEUTRAL_1;
    state.out2_buffer = OUT_NEUTRAL_2;
    float p1 = 0.0F, p2 = 0.0F;
    reset_pos_enc(p1, p2);
    state.pos1 = p1;
    state.pos2 = p2;
}
```

**Knock-on note for §14:** The PC-side `REF_POS1/REF_POS2` mechanism is
now redundant for HIGH_LEVEL — with the encoder zero following the held
pose, the natural reference is `pos = 0` and the `s.pos - REF_POS`
subtraction in `serialcomm.cpp` is already commented out. Keeping
`REF_POS*` constants in `config.h` is harmless; revisit when wiring
HYBRID's true cascade design.

## 13. One-shot neutral output on START

**Bug:** Immediately after START, the controller could switch to normal
outputs before the “neutral” command had ever been applied in a running
cycle. The desired behavior is a single neutral output on the first running
cycle, then normal control thereafter.

**Fix:** Implemented one-shot neutral directly in `cycle()` with a local
`first_running_tick` flag. While stopped, the flag is armed; on the first
running tick, `out1/out2` are forced to neutral once, then normal control
resumes. `RobotState` was kept clean (no extra cross-task state flag).

## 14. Explicit position reference in PC high-level controller

**Bug:** `OUT_NEUTRAL_1/2` were being interpreted as if they were position
targets. They are DAC neutral offsets, not pose references. This made target
intent unclear during debugging.

**Fix:** Added explicit position references in PC config:
`REF_POS1`, `REF_POS2`. HIGH_LEVEL/HYBRID now compute
`u = -K * (x - x_ref)` for the position states (`pos1`, `pos2`) and keep
`OUT_NEUTRAL_1/2` only as DAC centering offsets when converting voltage to
uint8.

## 15. One-tick neutral gate + PI reset on START/STOP (firmware)

**Bug:** Across runs, controller state and first-tick command ordering could
make startup behavior inconsistent between modes.

**Fix:** In `cycle()`:
- detect stop→run and force exactly one running tick with neutral DAC outputs;
- reset PI integrator state on stop and on stop→run transition via
  `reset_controller_state()`;
- keep encoder positions readable while stopped (`get_pos_enc`, no forced reset).

## 16. Expanded serial debug instrumentation (firmware)

**Goal:** Make start/stop transitions, DAC commands, and packet parsing visible
in terminal logs during hardware debugging.

**Changes:**
- Enabled `state.enable_debug = true` in `setup()` for active sessions.
- Added periodic tagged logs in `cycle()` and `serialcomm()`.
- Added detailed packet logs in `receive_control()`.
- Added requested-vs-saturated DAC logs in `write_control()`.
- Added Hall trip flagging (`state.hall_trip`) in ISR and deferred print in
  `cycle()` (safe logging outside ISR).

## 17. PC-side post-run debug log file

**Bug:** Debug prints on ESP32 UART corrupt the binary protocol, so runtime
inspection could not happen during normal PC↔ESP runs.

**Fix:** Added a PC-side logger in `serialcomm.cpp` that writes
`*_debug.log` after each run. It records start/stop events, short-read/bad-ID
errors, sampled telemetry snapshots, and control DAC values sent by the PC.

## 18. Split run outputs into `data/runs` and `data/logs`

**Change:** Default output paths now separate CSV and debug logs:
- CSV runs: `data/runs/output_<timestamp>.csv`
- Debug logs: `data/logs/output_<timestamp>_debug.log`

## 19. LOW_LEVEL mode finally functional (three latent bugs fixed)

**Bug:** LOW_LEVEL mode appeared to "work" only in the sense that the
firmware compiled and ran. On hardware the arm thrashed: the apparent
target seemed to drift every iteration and the regulator never settled.
The cause was three stacked bugs along the LOW_LEVEL data path. None
of them ever bit production because the original `ESP_serial.ino`
commented out `out1 = controller1(...)` and used `out1 = out1_buffer`
instead — i.e. it ran HIGH_LEVEL exclusively and never exercised the
PI controllers.

**Bug 19a — `T = 0` from integer division** in `control.cpp`:
```cpp
const float T = CYCLE_TIME_MS/1000;   // CYCLE_TIME_MS=1 (int), 1/1000 = 0
```
`pos1_int += T*pos1` therefore added 0 every tick and the integral
term `K_POSINT*pos_int` was permanently zero. Inherited verbatim from
`ESP_serial.ino`. Fix: `static_cast<float>(CYCLE_TIME_MS) / 1000.0F`.

**Bug 19b — controller voltage written to the DAC as if it were a
byte.** `controller{1,2}` return state-feedback voltages (small floats,
roughly `[-3, +3]` V). `write_control` in `analog.cpp` does
`int val1 = int(out1)` and clamps to `[0, 255]`, expecting a uint8 DAC
byte. So a voltage of `-0.5` became `0` (max negative motor drive); a
voltage of `+0.5` also became `0`; the arm got slammed one direction
permanently and the apparent "target" wobbled as `pos`/`strain`
reacted. HIGH_LEVEL never hit this because the PC pre-converts to a
uint8 DAC byte (`DAC_SCALE * volts + OUT_NEUTRAL`) before sending. Fix
applied in `cycle()`'s LOW_LEVEL branch — same conversion as the PC,
inlined:
```cpp
constexpr float DAC_SCALE = 256.0F / 3.3F;
const float v1 = controller1(p1, s1f, s1d);
const float d1 = DAC_SCALE * v1 + OUT_NEUTRAL_1;
state.out1_buffer = std::min(std::max(d1, 0.0F), 255.0F);
```

**Bug 19c — PI integrators carried wind-up across runs.**
`reset_controller_state()` exists in `control.cpp` but was not called
when `state.stop == true`. Combined with bug 19a it was hidden (T=0 →
integrator never accumulated anyway), but bug 19a's fix makes 19c
surface. Fix: call `reset_controller_state()` in the stopped branch
of `cycle()` alongside the encoder reset and neutral DAC outputs.
HIGH_LEVEL is unaffected (its integrators are never used).

**Note:** Verifies on hardware against this workflow: select
`CtrlMode::LOW_LEVEL` in BOTH `config.h` files, recompile both repos,
hold the arm at the desired neutral pose, press START. The encoder
reset (§12 OVERRULED) anchors `pos = 0` to the held pose; the on-ESP32
PI then regulates back to that pose autonomously.

## 20. HYBRID stabilization patch (pre-cascade redesign)

**Bug:** HYBRID reused LOW_LEVEL PI outputs but wrote them as raw values to
`write_control`, which expects DAC-space bytes. This mirrored LOW_LEVEL bug
19b and caused unstable behavior on hardware.

**Fix:** In `cycle()` HYBRID branch:
- treat `state.ref1/ref2` as feedforward DAC bytes from PC;
- convert feedforward bytes back to voltage offsets;
- add feedforward voltage to PI voltage;
- convert combined voltage back to DAC bytes (`DAC_SCALE * v + neutral`) and
  clamp before `write_control`.

**Also fixed:** HYBRID transmit path now streams autonomously while `!stop`
like LOW_LEVEL (removed `enable_transmit` gate from HYBRID branch) to avoid
stalled/bursty measurement flow during async receive.

**Scope note:** This is a stabilization patch for the current additive
feedforward design. The planned cascade redesign in `open-issues.md` remains
separate and still pending.

## 21. HYBRID tunable feedforward + saturation counters

**Goal:** Make oscillation diagnosis and supervisor review easier without
rewriting HYBRID to cascade yet.

**Changes:**
- PC `serialcomm_c/include/config.h`:
  - `HYBRID_K_SCALE` (feedforward gain scaling),
  - `HYBRID_CMD_SMOOTH_ALPHA` (EMA smoothing on outgoing DAC bytes),
  - `HYBRID_FORCE_NEUTRAL` (PI-only test in HYBRID).
- PC HYBRID branch applies those knobs before sending `FLAG_CONTROL`.
- Firmware `analog.cpp` counts near-rail saturation events for both motors
  (total/high/low) using `SAT_MARGIN`.
- Firmware debug output prints saturation counters for quick diagnosis.

## 22. HYBRID migrated to true cascade control

**Change:** Implemented the redesign from `open-issues.md`.

**PC outer loop (`serialcomm_c`)**
- HYBRID now maps K·x output to position references (rad), clamps to
  `HYBRID_REF_MAX_{1,2}`, smooths with `HYBRID_REF_SMOOTH_ALPHA`, and encodes
  to bytes around 127.
- Added knobs:
  - `HYBRID_OUTER_K_SCALE`
  - `HYBRID_REF_SMOOTH_ALPHA`
  - `HYBRID_FORCE_NEUTRAL_REF`

**ESP32 inner loop (`flexbot_firmware`)**
- HYBRID now decodes reference bytes to radians using `HYBRID_REF_MAX_{1,2}`.
- Forms error `err = pos - ref` and runs PI on error.
- Converts PI voltage to DAC byte (`DAC_SCALE * v + OUT_NEUTRAL`) before
  `write_control`.

**Protocol note:** Packet size remains 3 bytes (`FLAG_CONTROL + ref1b + ref2b`)
so `com.cpp` stays mode-agnostic.

## 23. HYBRID_REF_MAX mismatch fixed + ref CSV columns + supervisor comments

**Bug:** `HYBRID_REF_MAX_1/2` was `1.5F` in the ESP32 `config.h` but `0.2F`
in the PC `config.h`. The PC encodes references using its `0.2` scale; the
ESP32 decodes with `1.5`. A 0.1 rad reference from the PC arrived as ~0.75 rad
on the ESP32 — 7.5× amplification. This is likely the dominant cause of
instability at outer-loop gains above 0.05.

**Fix:** Changed ESP32 `HYBRID_REF_MAX_1/2` from `1.5F` → `0.2F` to match
the PC. Both sides now use the same scale so encode/decode cancels correctly.

**CSV diagnostic columns added:**

- `Sample` struct in PC `config.h` now has `ref1`, `ref2` fields (always
  present, 0.0 for non-HYBRID modes).
- `saveToFile` writes `ref1,ref2` columns after `out1,out2`.
- HYBRID branch: `s.out1/out2` now stores the raw K·x voltage (before clamp),
  and `s.ref1/ref2` stores the decoded reference actually sent to ESP32 (rad).
  Previously `out1/out2` was being overwritten with the decoded ref, losing
  the raw K·x signal.

**Supervisor-facing comments added:**

- `cycle()` HYBRID block (ESP32): documents the four-step inner loop (decode
  → error → PI → DAC conversion).
- `serialcomm()` HYBRID block (ESP32): documents the packet format and why
  `state.ref1/ref2` stores raw bytes (not radians).
- `runControlLoop()` HYBRID block (PC): documents the five-step outer loop
  (K·x → clamp → smooth → encode → transmit) with tuning guidance.

**Architecture doc updated:**

- HYBRID packet table corrected from 4 bytes to 3 bytes.
- HYBRID design section rewritten to show the full encode/decode loop,
  the encode/decode contract, tuning-knob table, and CSV column meanings.

## 24. HIGH_LEVEL and LOW_LEVEL control-flow comments expanded

**Change:** Added step-by-step supervisor-facing comments for HIGH_LEVEL and
LOW_LEVEL in both loops where behavior matters:

- `flexbot_firmware/src/main.cpp`
  - `cycle()` compute section now documents per-tick data/control flow for
    HIGH_LEVEL, LOW_LEVEL, and HYBRID from the ESP32 perspective.
  - `serialcomm()` HIGH_LEVEL and LOW_LEVEL branches now document their
    per-iteration communication flow and coupling/decoupling behavior.
- `serialcomm_c/src/serialcomm.cpp`
  - `runControlLoop()` header updated so HYBRID is described as reference
    cascade (not feedforward offset).
  - HIGH_LEVEL and LOW_LEVEL branches now include explicit iteration steps.

**Behavior impact:** none (comments/documentation only). Control logic and
packet format were not changed.

## 25. Latency campaign — Phase 1 instrumentation + Phase 3 baud bump

**Goal:** Measure and reduce PC↔ESP32 round-trip latency in HIGH_LEVEL mode.
Testing performed on a bare ESP32-WROOM-32 (no actuator); timing pipeline is
identical to production.

**Phase 1 — PC-side latency columns added to CSV:**

Three new `int32_t` fields added to `Sample` (PC `serialcomm_c/include/config.h`):

| Column | Measures |
|--------|----------|
| `pc_wait_us` | How long `readBytes` blocked waiting for the ESP32 sensor packet |
| `pc_proc_us` | From `readBytes` return to end of iteration body (K·x compute + `writeBytes`) |
| `pc_loop_us` | Full iteration cadence (prev `readBytes` return → this one); 0 on sample 0 |

Instrumented via `std::chrono::steady_clock` in `runControlLoop()`. ESP32
firmware unchanged. Cross-check: `pc_loop_us` should match ESP32's
`cycle_time_ms × 1000` if PC is the bottleneck; `pc_wait_us` isolates the
serial-read blocking time.

**Phase 3 — Baud rate bumped from 115200 → 921600:**

Changed `BAUD_RATE` in both `config.h` files. Both repos recompiled and
ESP32 reflashed. Protocol floor drops from ~1.8 ms → ~0.22 ms (8×).

Failure mode is explicit: bit errors corrupt `FLAG_SEND`, which
`runControlLoop` already validates → `stop_reason` reports the bad packet
immediately. No silent data corruption.

**CTRL_MODE set to HIGH_LEVEL for timing tests.** Switch back to HYBRID after
latency characterisation is complete (must match in both config files).

**Pending verification:** Run 1000-sample experiment; compare `cycle_time_ms`
and `pc_wait_us` vs the 115200-baud baseline. If RTT is reliably under 2 ms,
Phase 6 (reduce `COMM_TIME_MS`) becomes viable.

## 26. cycle() task documentation expanded (ESP32)

**Change:** Added a supervisor-facing block comment above `cycle()` in
`flexbot_firmware/src/main.cpp` documenting:

- fixed-rate tick timeline (A→E within one loop iteration),
- STOP-path behavior (neutral outputs, integrator reset, encoder-zero policy),
- RUN-path sensing/filtering/compute order,
- per-mode ownership split (HIGH_LEVEL vs LOW_LEVEL vs HYBRID).

Also tightened local inline comments in the stop/run branches so the control
intent is explicit when skimming the function.

**Behavior impact:** none (comment/documentation only).

## 27. ESP32-side per-packet timing dump (FLAG_DUMP)

**Goal:** Measure the actual worst-case execution times of `cycle()` and
`serialcomm()` on the ESP32 so the safety budgets `CYCLE_TIME_MS` and
`COMM_TIME_MS` (firmware `config.h`) can be tightened without missed
deadlines.

**Why a dump rather than per-packet inline:** extending the 16-byte sensor
packet would slow every iteration during the run. Buffering on ESP32 and
dumping after STOP keeps the runtime hot path unchanged.

**Mechanism (firmware):**

- `MAX_TIMING_SAMPLES = 10000` (40 KB). Static `int16_t g_comp_us[]`,
  `int16_t g_comm_us[]`, `volatile uint16_t g_timing_idx` defined in
  `flexbot_firmware/src/main.cpp`, declared `extern` in
  `flexbot_firmware/include/FlexBot.h`.
- `serialcomm()` captures `state.computation_time` and
  `state.communication_time` into the buffers right before each
  `transmit_measurement(...)` call (HIGH_LEVEL, LOW_LEVEL, HYBRID).
- `g_timing_idx = 0` on the START rising edge in all three modes (so
  back-to-back runs don't accumulate).
- New flag `FLAG_DUMP = 77` recognised in `receive_control()`. On match,
  `transmit_timing_dump()` writes `uint16 count` + `count × (int16
  comp_us, int16 comm_us)` to Serial and resets `g_timing_idx`.

**Mechanism (PC):**

- `Sample` gains `int32_t esp_comp_us`, `esp_comm_us`. CSV header and
  per-row writer extended.
- `runControlLoop()` after sending STOP: `flushReceiver()`, send
  `FLAG_DUMP+0+0`, read 2-byte count, read `count*4` payload bytes, merge
  into `data[]` row-for-row. Tolerates count mismatch with a warning.

**Wire impact during the control run:** zero. Packet still 16 bytes.

**RAM impact (firmware):** +40 KB static. ESP32 build went from ~5% RAM
to 19% — well within the 320 KB SRAM budget.

**Use:** run normally; inspect new CSV columns `esp_comp_us`,
`esp_comm_us`. The maxima over a long run set safe lower bounds for
`CYCLE_TIME_MS` and `COMM_TIME_MS`. In HIGH_LEVEL the receive iteration
of `serialcomm()` blocks on `Serial.readBytes`, so `esp_comm_us` will
alternate between fast (transmit-only) and slower (receive) values.

## 29. ENABLE_TIMING compile-time flag + COMM_TIME_MS lowered to 1

**ENABLE_TIMING flag:**

New `constexpr bool ENABLE_TIMING` in **both** `config.h` files (must match,
same invariant as `CTRL_MODE`).

| Value | Effect |
|-------|--------|
| `true` | PC measures `pc_loop/proc/wait_us`, requests timing dump, writes all five columns to CSV. Firmware allocates 40 KB buffers and serves FLAG_DUMP. |
| `false` | None of the above. No buffers, no extra columns, no overhead. For production runs. |

**Implementation details:**

- Firmware `flexbot_firmware/src/main.cpp`: buffer array size declared as
  `ENABLE_TIMING ? MAX_TIMING_SAMPLES : 1` (4 bytes vs 40 KB). All capture
  code and START-edge reset wrapped in `if constexpr (ENABLE_TIMING)`.
- Firmware `flexbot_firmware/src/com.cpp`: FLAG_DUMP branch and
  `transmit_timing_dump()` body guarded by `if constexpr (ENABLE_TIMING)`.
- PC `serialcomm_c/src/serialcomm.cpp`: `pc_*_us` field writes, dump request
  block, and CSV timing columns all wrapped in `if constexpr (ENABLE_TIMING)`.
- CSV header and per-row writer omit the five timing columns when false.

**COMM_TIME_MS lowered 5 → 1:**

Measured worst-case `esp_comm_us` = 145 µs (6.9× headroom to 1000 µs).
At 921 600 baud the PC reply arrives well within the 5 ms window, so the
ESP32's receive iteration finds bytes already in the RX FIFO — no blocking.
The 5 ms was pure wasted sleep. With COMM_TIME_MS=1 the two-iteration
round-trip drops from ~10 ms to ~2 ms. Expected `pc_loop_us` ≈ 2000 µs.

**Expected count=len+1 warning suppressed:**

One "extra" ESP32 transmit happens between the PC sending the last control
output and the ESP32 receiving STOP. This makes `count = data.size()+1`
consistently. The warning is now suppressed for this case; it still fires
for any discrepancy larger than 1.

## 30. Dynamic timing buffers — MAX_TIMING_SAMPLES matches experiment length

**Goal:** Eliminate the manual `MAX_TIMING_SAMPLES` edit. Previously the
firmware allocated a fixed 40 KB static array; if `askSimulationLength()`
returned a number larger than `MAX_TIMING_SAMPLES`, timing data was silently
truncated.

**Root cause:** Static arrays need a compile-time size. The experiment length
is a runtime value on the PC. The fix bridges this gap by encoding the length
inside the START packet and using `std::vector` for heap allocation.

**Protocol change — START/STOP sentinel:**

Packet bytes are indexed as byte0=flag, byte1, byte2.

| Packet | Old byte1 byte2  | New byte1 byte2                                      |
|--------|------------------|------------------------------------------------------|
| START  | `0x00 0x00`      | `(len>>8) (len&0xFF)` — experiment length big-endian |
| STOP   | `0x01 0x01`      | `0xFF 0xFF` — explicit sentinel                      |

`len` is capped to 65534 on the PC side. The ESP32 further caps to
`MAX_TIMING_SAMPLES` (now 30000, ~120 KB heap when ENABLE_TIMING=true).

**Firmware changes:**

- `flexbot_firmware/include/FlexBot.h`: externs changed from `int16_t[]` to
  `std::vector<int16_t>`; added `extern uint16_t g_timing_cap`.
- `flexbot_firmware/src/main.cpp`: static arrays replaced with empty
  `std::vector<int16_t> g_comp_us/g_comm_us`; `g_timing_cap` global added.
  START-edge code in all three modes now calls `g_comp_us.assign(g_timing_cap, 0)`
  to allocate exactly the right number of slots. Capture guard changed from
  `g_timing_idx < MAX_TIMING_SAMPLES` → `g_timing_idx < g_timing_cap`.
- `flexbot_firmware/src/com.cpp`: `receive_control()` now detects STOP via
  `b1==0xFF && b2==0xFF`; on START it extracts `g_timing_cap` from the two
  payload bytes (capped to `MAX_TIMING_SAMPLES`).
- `flexbot_firmware/include/config.h`: `MAX_TIMING_SAMPLES` raised to 30000
  and is now a safety ceiling only, not the actual buffer size.

**PC change:**

- `serialcomm_c/src/serialcomm.cpp`: START packet encodes `len` in byte1/byte2;
  STOP packet uses sentinel `0xFF 0xFF`.

**RAM impact:** When `ENABLE_TIMING=true`, heap is allocated on START and
persists until the next START or power cycle. Peak: `len × 4` bytes (e.g.
4 KB for 1000 samples, 40 KB for 10000). When `ENABLE_TIMING=false`, vectors
remain empty — zero heap use.

## 28. Timing dump framing sync fix (count=109 race)

**Symptom observed on hardware:** PC printed
`Timing dump count 109 != PC sample count 1000` and only the first ~109 CSV
rows got non-zero `esp_comp_us`/`esp_comm_us`, with impossible negative values.

**Root cause:** `109` equals `FLAG_SEND`. After STOP, the PC requested dump and
immediately parsed the next 2 bytes as `count`. A still in-flight 16-byte
sensor packet could arrive first, so the sensor flag byte (`109`) was misread
as dump count.

**Fix (wire format hardening):**

- Added dump framing constants in both repos:
  `DUMP_MAGIC_0..3 = A5 5A C3 3C`.
- Firmware `transmit_timing_dump()` now sends:
  `magic[4] + uint16 count + count*(int16 comp_us, int16 comm_us)`.
- PC `runControlLoop()` now calls `readTimingDumpCount(...)`, which scans the
  serial stream for that 4-byte magic before reading count. This tolerates any
  stale sensor bytes still arriving after STOP.

**Behavior impact during control run:** none. Runtime sensor/control packets
stay unchanged (still 16-byte ESP→PC, 3-byte PC→ESP). Only post-run dump
framing changed.

**Expected post-fix result:** `count` should match the number of transmitted
packets for the run (up to `MAX_TIMING_SAMPLES`) without the recurring fake
`109` value.

## 31. Third-party attribution hygiene (2026-07-06)

**Design/documentation change (no code behavior impact).**

The repo `LICENSE` (MIT, © Fausto Arroyo Mantero) previously implied sole
authorship of the whole repo, but `serialcomm_c/{include,src}/serialib.{h,cpp}`
is third-party — Philippe Lucidarme (University of Angers), v2.0, a
"licence-free" software dedication that is MIT-compatible.

**Changes:**

- Added `THIRD_PARTY_NOTICES.md` at repo root: credits serialib, reproduces its
  original notice verbatim, records that it is vendored unmodified.
- Added a "License & Credits" section to `README.md` stating own code is MIT and
  linking the notices file.

**Rationale:** honest MIT claim (own code only), preserves original author
attribution, and reads as professional diligence for a public/recruiter-facing
repo. License choice itself unchanged — MIT confirmed correct.

**Note:** firmware repo licensing (pre-existing original author + PlatformIO
`lib_deps` third-party libs) is a separate later task, not covered here.

## 32. Repo showcase/hygiene pass (2026-07-07)

**Design/documentation change (no runtime code behavior impact).**
Implements `docs/specs/showcase-plan.md`.

**Doc bugs found and fixed:**

- `CLAUDE.md` claimed 115200 baud; `config.h` (both repos) has been at
  921600 since §25's Phase 3. `CLAUDE.md` corrected; README/architecture.md
  were already accurate.
- README's Build/Running sections told users to run `./build/serialcomm`,
  which has never existed — the CMake target (`serialcomm_c/CMakeLists.txt`)
  is `flexbot_app`, confirmed against an actual local build output. Fixed
  all three occurrences.
- `CLAUDE.md`'s build command built from inside `serialcomm_c/`; README
  builds from the repo root. Aligned `CLAUDE.md` to the repo-root form.

**Added:**

- `docs/hardware.md` — BOM with vendor links (Espressif, AZ-Delivery,
  Quanser, maxon, US Digital, Silicon Labs, WCH), wiring/pin table sourced
  from `flexbot_firmware/include/config.h` and `analog.cpp` (confirms
  motor 1 = shoulder/`DAC1`, motor 2 = elbow/`DAC2`, restricted range),
  and an explicit "runs without the arm" caveat.
- `docs/design-decisions.md` — six decision write-ups distilled from this
  file, verified against the full log rather than assumed from the README.
- `.github/workflows/build.yml` — Linux + Windows CMake build matrix.
  `-Werror` verified clean locally on GCC 11 before enabling; MSVC not
  locally verifiable, so Windows runs `/W4` warn-only until a green CI run
  confirms it's clean too.
- `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, issue/PR templates.
- README hero: pitch, badges, Mermaid architecture diagram, highlights,
  quick-start block (folded from scattered sections).

**Repo hygiene:**

- `.gitignore`: removed a stale `firmware/.pio` block (no `firmware/` dir
  in this repo — leftover from before the two-repo split); un-ignored
  `.clang-tidy`/`.clangd` (previously matched and excluded by their own
  glob patterns, unintentionally); added `.vscode/extensions.json` as a
  tracked exception to the otherwise-ignored `.vscode/`; added
  `docs/ESP32/*.pdf`, `docs/ESP32/*.zip`, `docs/Robot_Hardware/*.pdf` so
  vendor files aren't re-added by accident.
- Deleted local untracked `serialcomm_c/serialcomm.exe` (stale artifact,
  already git-ignore-matched).
- Vendor PDFs/driver zips under `docs/ESP32/` and `docs/Robot_Hardware/`
  (~35 MB, copyrighted, no redistribution rights — see `docs/hardware.md`)
  removed from the working tree and purged from git history via
  `git filter-repo` while the repo had no external clones.

**Not done in this pass (see `docs/specs/showcase-plan.md` for the full
checklist):** media capture (needs hardware), GitHub repo
metadata/topics/social preview, flipping the repos from private to
public, and the `v1.0.0` tag.
