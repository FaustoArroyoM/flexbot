<!-- AI assistant memory file (Claude Code). Not part of the build. -->
# FlexBot — Claude Code Project Memory

> Keep this file lean. Detailed reference lives in `docs/` — pull it in
> on-demand with `@docs/filename.md`. Only load what the task needs.

---

## Doc maintenance (non-negotiable)

After every session where code changes, tests run, or design decisions are
made — update the docs before ending. Do not wait to be asked.

| Doc | Update when |
|-----|-------------|
| `change-log.md` | Any fix or intentional design change. Never delete entries — mark superseded ones REVISED/OVERRULED with reason. |
| `open-issues.md` | Tick [x] items fixed AND hardware-verified. Add [ ] for every new bug or TODO found, even undiagnosed. Move resolved items to ## Verified working. |
| `architecture.md` | Any protocol constant, packet format, task timing, or RobotState field changes. |

**Intermediate state tag** — if a fix was applied but hardware verification
is still needed, mark it:
```
[ ] Item — FIX APPLIED, NEEDS HARDWARE VERIFICATION
```
Leave it open until the user confirms on hardware.

**The rule:** if a future session reads only the four .md files and has the
wrong mental model, the session that made the change is at fault.

---

## Project

Two-joint robot arm over USB serial.
Hardware: ESP32-D0WD (dual-core, 240 MHz) ↔ Linux PC (Ubuntu 22.04)
via `/dev/ttyUSB0` @ 921600 baud.

## Session startup (run this first, every session)

\```bash
claude --add-dir ~/Desktop/flexbot_firmware
\```

Do not start answering tasks until both repos are in context.

### Directories

```
~/Desktop/flexbot/
├── serialcomm_c/
│   ├── include/config.h       ← CTRL_MODE (must match ESP32)
│   ├── include/serialib.h     ← third-party, never modify
│   ├── src/serialcomm.cpp
│   └── CMakeLists.txt
├── build/                     ← CMake output
├── data/                      ← CSV experiment logs
└── docs/

~/Desktop/flexbot_firmware/
├── include/config.h           ← CTRL_MODE (must match PC)
├── include/FlexBot.h
└── src/{main,com,encoder,analog,hall,filter,control}.cpp
```

### Build & flash

```bash
# PC — from ~/Desktop/flexbot
cmake -S serialcomm_c -B build && cmake --build build

# ESP32 — from ~/Desktop/flexbot_firmware
pio run
pio run -t upload
pio device monitor
```

### Critical invariants

1. `CTRL_MODE` identical in both `config.h` files. Recompile both on change.
2. All mode branching uses `if constexpr`. No runtime mode flags.
3. `com.cpp` is mode-agnostic by design — never push mode logic into it.
4. `serialib.{h,cpp}` is third-party — never modify.

---

## Docs (load on demand only)

- `@docs/architecture.md` — control modes, packet format, RobotState,
  FreeRTOS layout. Load when working on control flow or scheduling.
- `@docs/change-log.md` — every fix vs original code with rationale.
  Load when something seems weird and you want to know "why is it like this."
- `@docs/open-issues.md` — active bugs and untested items.
  Load before every debugging session.
