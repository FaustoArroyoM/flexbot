# Contributing to FlexBot

FlexBot is a two-repo hardware project (`flexbot` — this repo, PC host —
and [`flexbot_firmware`](https://github.com/FaustoArroyoM/flexbot_firmware)
— ESP32 firmware). Most of what's useful here doesn't require the physical
arm.

## What you can do without the hardware

- Build both repos and confirm they compile (see [Quick start](README.md#quick-start)).
- Fix host-side bugs: `serialcomm_c/src/serialcomm.cpp`, CSV logging,
  `data/analyze_timing_data.py`.
- Improve docs, CI, tooling, error messages.
- Review/extend the protocol logic in `com.cpp`/`serialcomm.cpp` — you can
  run the PC host against a bare ESP32 DevKitC with no arm attached and
  exercise the full start/stop/control wire protocol; see
  [`docs/hardware.md`](docs/hardware.md) for what that looks like.

## What needs hardware

Anything touching control tuning, encoder/strain-gauge behavior, or
motor safety limits needs verification on the physical rig. Items
currently marked `NEEDS HARDWARE VERIFICATION` are tracked in
[`docs/open-issues.md`](docs/open-issues.md) — check there before
assuming a fix is confirmed working.

## Rules of the codebase

These are non-negotiable invariants, not style preferences:

1. **`CTRL_MODE` must be identical in both repos' `config.h`.** Recompile
   and reflash both sides after any change — a mismatch desyncs the wire
   protocol.
2. **All mode branching uses `if constexpr`, never a runtime flag.** This
   is what lets the compiler delete unused control-mode code entirely and
   keeps the protocol from silently drifting.
3. **`com.cpp` (firmware) and the serial I/O path (PC) stay mode-agnostic.**
   Never push `CTRL_MODE`-specific logic into the transport layer.
4. **`serialib.{h,cpp}` is third-party and read-only.** Do not modify it —
   see [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md). If you need
   different serial behavior, wrap it, don't edit it.

## Doc maintenance

This project keeps an engineering log, not just a README. If your PR
changes behavior, update the relevant doc in the same PR:

| Doc | Update when |
|---|---|
| [`docs/change-log.md`](docs/change-log.md) | Any fix or intentional design change. Never delete old entries — mark superseded ones `REVISED`/`OVERRULED` with the reason, don't erase them. |
| [`docs/open-issues.md`](docs/open-issues.md) | Tick off items only once fixed **and** hardware-verified. Add new `[ ]` entries for anything you find, even if undiagnosed. |
| [`docs/architecture.md`](docs/architecture.md) | Any protocol constant, packet format, task timing, or `RobotState` field change. |

If a fix is applied but not yet confirmed on hardware, mark it explicitly:
`[ ] Item — FIX APPLIED, NEEDS HARDWARE VERIFICATION`, and leave it open
until someone confirms on the real arm.

## PR expectations

- Builds cleanly on Linux (`cmake -S serialcomm_c -B build && cmake --build build`) — CI checks this.
- Doc updates per the table above, where applicable.
- Keep `CTRL_MODE`/protocol changes to both repos in sync — call out in
  the PR description if a change here requires a corresponding
  `flexbot_firmware` change.
- Small, focused PRs over large ones — this is a two-person-hobbyist-scale
  project, not a place for sweeping refactors without discussion first.
