# FlexBot Open Issues / TODOs

> On-demand reference. Load with `@docs/open-issues.md` before starting
> a debugging session.
>
> Use `[ ]` checkboxes — Claude Code can tick them off as items get
> resolved. When you finish one, ask "tick off the encoder verification
> item in `@docs/open-issues.md`."

## Verified working

- [x] pos1/pos2 encoder reads non-zero hardware values (verified 2026-04-29)
- [x] K·x HIGH_LEVEL drives arm to zero in 300-sample run (verified 2026-04-29)
- [x] **ISSUE A — encoder zeroed at experiment START** (verified 2026-04-30;
      overrule reverted 2026-05-04)
      Continuous `reset_pos_enc` while `state.stop == true` (original-style,
      restored from `ESP_serial.ino`). Each run starts with `pos1=pos2=0`,
      and the encoder zero tracks whatever pose the user is physically
      holding the arm in. The 2026-05-04 overrule (anchoring pos=0 to ESP32
      boot pose so K·x referenced a "fixed" zero) was itself overruled later
      that day: the arm regulated to boot pose instead of any meaningful
      mechanical middle, and `OUT_NEUTRAL_1/2` cannot stand in as a pose
      target since DAC neutral is "no torque," not "go to middle." Restored
      per change-log §12 OVERRULED. **Workflow:** physically hold the arm
      at the desired neutral pose before pressing START.
- [x] **ISSUE B — first CSV row cycle_time_ms is 0** (verified 2026-04-30)
      Rising-edge detection on `state.stop` in serialcomm() receive branch
      zeroes `state.end_time` on every START packet. User confirmed
      "cycle_time_ms logging looks normal now". See change-log §7.
- [x] **ISSUE D — timestamp has seconds, sortable** (verified 2026-04-30)
      `output_2026-04-30_HH-MM-SS.csv`. See change-log §10.

## Active bugs (fix in order)

- [ ] **ISSUE E — one-shot neutral output on START** —
      FIX APPLIED, NEEDS HARDWARE VERIFICATION
      `cycle()` now forces a single neutral output on the first running tick
      after any stopped period, then normal control resumes. PI integrators are
      reset on STOP and on START edge.
      Firmware now emits tagged debug logs for cycle/serial/receive_control/
      write_control/Hall events to support hardware trace debugging.
      PC side now saves `*_debug.log` per run without polluting UART protocol.
      **Verify:** start from a non-neutral pose; the first running tick should
      command neutral outputs (127/125), then K·x takes over on the next tick.
      Also verify changing `REF_POS1/REF_POS2` in PC `config.h` moves the
      equilibrium accordingly (confirming target is from `x_ref`, not DAC neutral).

- [ ] **ISSUE C — graceful save on out-of-bounds / Ctrl-C** —
      FIX APPLIED, NEEDS HARDWARE VERIFICATION
      `runControlLoop()` uses `push_back` (no zero padding), polls a
      SIGINT-driven atomic flag, and reports any short-read / wrong
      flag from ESP32 via `stop_reason` (out parameter, no wrapper
      struct). `saveToFile` writes `# stopped: <reason>` to the CSV
      header. See change-log §9.
      **Verify:** force a Hall trip mid-run (push a joint past its
      limit). The CSV should still save, contain only the actually
      collected rows, and have a `# stopped: ESP32 stopped
      transmitting...` line in the header. Also try Ctrl-C mid-run.

## Deferred / out of scope

- **Physical homing to gravity-rest before K·x** — overruled
  2026-04-30. The 5-phase START → homing → STOP+START → K·x → STOP
  flow didn't work reliably on hardware and added complexity. For now,
  the run starts wherever the arm is when the user presses go.
  See change-log §8 (overruled). Revisit later if a real homing
  reference (e.g., Hall sensor as zero) is added.

## HYBRID mode redesign (separate from bug fixes)

- [ ] **Redesign HYBRID as true cascade control**
      Current (wrong): K·x → voltage offset added to PI output
      Target: K·x → position reference sent to ESP32 inner PI loop
      Outer loop (PC, ~10ms): computes ref1/ref2 as position targets
      Inner loop (ESP32 PI, ~1ms): tracks those position targets
      Files to change: serialcomm.cpp HYBRID branch, main.cpp HYBRID branch,
      both config.h files (packet format for ref may change),
      architecture.md and change-log.md after implementation.
      FIX APPLIED, NEEDS HARDWARE VERIFICATION
      Implemented cascade: PC now sends position references; ESP32 decodes
      reference bytes and PI tracks position error. Added new tuning knobs
      (`HYBRID_OUTER_K_SCALE`, `HYBRID_REF_SMOOTH_ALPHA`,
      `HYBRID_FORCE_NEUTRAL_REF`) and kept saturation counters for diagnosis.
      **Verify:** in HYBRID, `HYBRID_FORCE_NEUTRAL_REF=true` behaves like
      PI-only baseline, and increasing `HYBRID_OUTER_K_SCALE` improves tracking
      until onset of oscillation.

## Intentional non-changes

- com.cpp is mode-agnostic by design — never push mode logic into it
- serialib.{h,cpp} is third-party — never modify
- All mode branching uses if constexpr — no runtime flags

## Latency campaign

- [ ] **Verify COMM_TIME_MS=1 on hardware** — FIX APPLIED, NEEDS HARDWARE VERIFICATION
      Measurements showed `esp_comm_us` max=145 µs and `esp_comp_us` max=274 µs,
      both well within 1 ms. COMM_TIME_MS lowered 5→1 (§29). Expected: `pc_loop_us`
      drops from ~10 000 µs to ~2 000 µs. ENABLE_TIMING flag added (§29) — both
      `constexpr bool ENABLE_TIMING` in both config.h files must match.
      **Verify:** flash and run ≥1000 samples in HIGH_LEVEL with ENABLE_TIMING=true.
      Check `mean(pc_loop_us) ≈ 2000`, no `# stopped: ESP32 stopped transmitting`
      errors, no corrupted packets. If stable, try ENABLE_TIMING=false to confirm
      no-overhead path works too.

## Untested on hardware

- LOW_LEVEL mode: tested on hardware 2026-05-05 — behaved like HIGH_LEVEL (encoder reset + regulation working). Gains may still need tuning but the control path is confirmed functional.
- HYBRID mode: cascade redesign implemented (§22). Inner loop (FORCE_NEUTRAL_REF=true) confirmed stable. Outer loop (K_SCALE>0) needs careful gain tuning — start ≤ 0.10.
