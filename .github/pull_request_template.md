## What does this PR do?



## Checklist

- [ ] Builds on Linux (`cmake -S serialcomm_c -B build && cmake --build build`)
- [ ] If `CTRL_MODE`, packet format, or another protocol constant changed: the
      corresponding change was made (or a linked PR was opened) in
      [`flexbot_firmware`](https://github.com/FaustoArroyoM/flexbot_firmware),
      and both `config.h` files still match
- [ ] `docs/change-log.md` updated if this is a fix or an intentional design change
- [ ] `docs/open-issues.md` updated — new issues added, or fixed items marked
      `NEEDS HARDWARE VERIFICATION` (not ticked off) until confirmed on the arm
- [ ] `docs/architecture.md` updated if a protocol constant, packet format, task
      timing, or `RobotState` field changed

## Hardware verification

- [ ] Verified on the physical arm
- [ ] Not verified on hardware — host/firmware logic only, or needs a maintainer with the rig
