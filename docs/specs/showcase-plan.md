# FlexBot Showcase Plan — polish, adoption, licensing, collaboration

> Spec written 2026-07-06. Scope: **this repo only** (`flexbot/`, the PC host).
> The firmware repo is a separate later task (see §7). Nothing here is
> implemented yet — this is the work plan.
>
> Decisions already locked (do not re-open):
> - License: **MIT** (LICENSE already in place, © 2026 Fausto Arroyo Mantero).
> - Repo structure: **stays a separate repo** — no monorepo merge.
> - serialib attribution: **done** (`THIRD_PARTY_NOTICES.md` + README credits
>   + verbatim header notice). serialib is the **sole third-party code** in
>   this repo (verified by include audit).

---

## 0. Current state (verified, not assumed)

What's already good — the baseline is stronger than a typical hobby repo:

- README is thorough on *operation*: build steps for Ubuntu + Windows, serial
  port setup, all three control modes explained with data-flow diagrams,
  CSV format table, timing instrumentation docs, troubleshooting table.
- `LICENSE` (MIT) and `THIRD_PARTY_NOTICES.md` exist and are correct.
- `docs/architecture.md`, `docs/change-log.md` (718 lines, with
  REVISED/OVERRULED discipline), `docs/open-issues.md` — genuinely unusual
  engineering-log quality.
- `.clang-tidy`, `.clangd`, VS Code configs exist locally.

What needs work (found during the audit for this plan):

- **~35 MB of vendor PDFs and Windows driver zips are committed** under
  `docs/ESP32/` and `docs/Robot_Hardware/` (largest: 17 MB user manual,
  7 MB DAQ manual, two proprietary driver `.zip`s). This is the single
  biggest hygiene problem: clone weight, and a redistribution-rights
  question (vendor datasheets and driver installers are copyrighted;
  most don't grant redistribution).
- README has **no hero**: no photo/GIF of the arm, no top-level architecture
  diagram, no badges, and the one-line pitch is buried under a repo-layout
  table.
- **Doc inconsistency:** `CLAUDE.md` says 115200 baud; README and
  `docs/architecture.md` say 921600. One of them is stale — resolve.
- `.gitignore` is functional but stale: references a `firmware/` directory
  that doesn't exist in this repo; `serialcomm_c/serialcomm.exe` sits
  untracked in the working tree (caught by `*.exe`, so not a git problem —
  just delete it locally).
- `CLAUDE.md` is currently **untracked**. Decide: commit it (it documents the
  doc-maintenance discipline — arguably a showcase asset) or ignore it.
- No CI, no CONTRIBUTING.md, no issue/PR templates, no CODE_OF_CONDUCT, no
  tagged release.
- GitHub repo metadata (description, topics, social-preview image) —
  not auditable locally; assume unset.

---

## 1. Recruiter-facing polish

A recruiter gives a repo ~30 seconds. Everything in this section optimizes
what's visible without scrolling: hero image, pitch, badges, diagram.

### 1.1 README hero rewrite (top ~40 lines only)

**What:** Restructure the top of the README:

1. One-sentence pitch: *"Real-time cascade control of a flexible two-joint
   robot arm — C++17 host + ESP32 firmware talking over a custom 921600-baud
   serial protocol at 1 kHz."*
2. Photo or GIF of the arm moving (see 1.2) directly under the title.
3. Badge row: `C++17`, `CMake`, `Linux | Windows`, `ESP32`, `MIT`, and the
   CI badge once §4.3 lands.
4. A compact architecture diagram (see 1.3).
5. "Highlights" bullet list — 5 bullets max, each an engineering decision
   (see 1.4).
6. Then the existing content (Two-Repo System, modes, build…) follows —
   it's already good; don't rewrite it, just demote it below the hero.

**Why:** The current README opens with "PC-side control and data-logging
software" — accurate but undersells a real-time control-systems project.
The depth is already there; it's the first screen that's missing.
**Effort:** 2–3 h (excluding media capture). **Priority: DO FIRST.**

### 1.2 Photo / GIF of the arm working

**What:** Capture (a) one clean still photo of the hardware, (b) a short
screen-or-phone GIF of a HIGH_LEVEL or HYBRID run — arm disturbed by hand,
regulating back to neutral. Keep the GIF under ~10 MB; prefer a compressed
`.gif` or an embedded `.mp4`-to-GIF conversion. Store in `docs/media/`
(add the folder), reference from README.
Optionally add a plot rendered from a real CSV run (there's already
`data/analyze_timing_data.py` — a matplotlib figure of position + control
output over a run makes the "1 kHz control" claim tangible).

**Why:** Hardware projects live or die on proof the hardware exists and
works. This is the single highest-leverage recruiter item.
**Effort:** 1–2 h with hardware access. **Priority: DO FIRST** (needs you +
the physical arm; nothing else blocks on it, so schedule it whenever the
hardware is out).

### 1.3 Architecture diagram

**What:** One diagram showing: PC (control loop ~200 Hz, CSV logger) ↔
USB serial 921600 baud ↔ ESP32 (dual-core: 1 kHz control task / comm task) ↔
motors + encoders + strain gauges. Annotate the three CTRL_MODEs as "who
closes the loop." Use Mermaid in the README (renders natively on GitHub,
diffable, zero binary assets) with the option of a hand-drawn SVG later.

**Why:** The two-repo + three-mode + two-rate structure is the core
intellectual content of the project and currently exists only as prose
and ASCII tables.
**Effort:** 1–2 h. **Priority: DO FIRST.**

### 1.4 "Design decisions" section (framing the engineering)

**What:** A short README section (or `docs/design-decisions.md` linked from
the README) with 4–6 entries, each: decision → alternative rejected → why.
Material already exists in `docs/change-log.md`; this is a distillation, not
new writing. Strong candidates:

- **Compile-time mode dispatch** — `if constexpr` over runtime flags: dead
  code eliminated, protocol can't desync silently at runtime, `com.cpp`
  stays mode-agnostic.
- **Cascade (HYBRID) architecture** — why the inner 1 kHz PI on the ESP32 +
  outer ~200 Hz state feedback on the PC, i.e. decoupling control bandwidth
  from USB latency.
- **Timing instrumentation with zero-cost off switch** — `ENABLE_TIMING`
  post-run FLAG_DUMP design: no measurement overhead during the run,
  per-packet µs data merged into the CSV afterwards.
- **Crash-safe experiment logging** — SIGINT-driven atomic flag, partial-run
  CSV save with `# stopped:` reason header.
- **Encoder zeroing anchored to the held pose** — including the fact that a
  "fix" was tried, overruled, and reverted with reasons (change-log §12).
  Mentioning a documented reversal is a maturity signal, not a weakness.

**Why:** This is the section a technical interviewer reads. It converts
"student project" into "engineer who can justify trade-offs" and gives you
ready-made interview narrative.
**Effort:** 2–3 h. **Priority: DO FIRST.**

### 1.5 GitHub repo metadata

**What:** On GitHub: set the repo description (same one-liner as README),
add topics (`robotics`, `control-systems`, `esp32`, `embedded`, `cpp17`,
`real-time`, `serial-communication`, `freertos`), upload a social-preview
image (the arm photo from 1.2).

**Why:** Description + topics are what shows in search and on your profile
pin; the social preview is what renders when you paste the link to a
recruiter in an email/DM.
**Effort:** 15 min. **Priority: DO FIRST** (cheapest item on the list).

---

## 2. Ease of adoption (stranger clones → runs)

The README's build/run coverage is already ~80% there. Gaps:

### 2.1 Quick-start block

**What:** A copy-pasteable "from zero to first experiment" block near the
top: clone (both repos), apt install line, cmake build, flash firmware
(pointer to firmware README), dialout group, run. Explicitly state the
happy path is Ubuntu 22.04 + ESP32 DevKitC.

**Why:** The info exists but is spread over five sections; a stranger
shouldn't have to assemble the order themselves.
**Effort:** 1 h. **Priority: DO FIRST** (fold into the 1.1 rewrite).

### 2.2 Hardware & wiring documentation

**What:** A `docs/hardware.md` with: bill of materials (ESP32 DevKitC V4,
Maxon RE25/RE35 motors, AMPAQ-L2 amplifiers, E5 encoders, strain gauges,
the 11-2024 PCB), a wiring/pin table (pins are in the firmware `config.h` —
surface them here for people without the firmware repo open), and *links*
to vendor datasheets rather than the committed PDFs (see 5.1).
Honest caveat up front: this runs on a specific lab rig (2-DOF flexible-link
arm + custom PCB); a stranger can build and run the host + firmware against
a bare ESP32 with no arm attached — say what they'll see if they do.

**Why:** Without it, "adoption" stops at compiling. With it, the project is
reproducible in principle, and even non-builders can evaluate the design.
Also the prerequisite for un-committing 35 MB of PDFs.
**Effort:** 3–4 h. **Priority: DO FIRST** (blocks 5.1).

### 2.3 No-hardware mode (stretch)

**What:** Optional: a `--dry-run`/loopback mode or a tiny fake-ESP32 pty
script (Python + `pty`/`socat`) that emits valid 16-byte sensor packets, so
`serialcomm` can be demoed and CI-smoke-tested with no device.

**Why:** Turns "can compile" into "can actually run something" for 100% of
visitors, and gives CI something to exercise beyond compilation. But it's
real work and touches program flow.
**Effort:** 1–2 days. **Priority: NICE TO HAVE** — do only after everything
in "do first" is done.

### 2.4 Doc consistency pass

**What:** Fix the 115200-vs-921600 baud contradiction between `CLAUDE.md`
and README/architecture.md (determine which is current from
`config.h`/firmware and correct the stale one). Align the two build-command
variants (CLAUDE.md builds from `serialcomm_c/`, README from repo root).
Verify README's repo-layout tree matches reality (it currently omits
`docs/specs/`, `THIRD_PARTY_NOTICES.md`, `data/analyze_timing_data.py`).

**Why:** Contradictory docs are worse than missing docs — a stranger can't
tell which to trust, and a recruiter who spots it discounts everything else.
**Effort:** 1 h. **Priority: DO FIRST.**

---

## 3. Third-party code & licensing

Status: **largely complete** (this session). Remaining decisions and
residual items only.

### 3.1 serialib — keep vendored (recommendation, with rationale)

**Audit result:** `serialib.{h,cpp}` v2.0 by Philippe Lucidarme, vendored
unmodified, sole third-party code in the repo. Attribution done in
`THIRD_PARTY_NOTICES.md` (verbatim original notice) and README credits.

**Vendor vs submodule vs package:** **Keep it vendored.**
- It's two files with no dependencies; upstream is not on any package
  manager (no Conan/vcpkg release usable here), so "dependency" isn't a
  real option.
- A git submodule would pin upstream `master`, add a clone-time footgun
  (`--recurse-submodules`) for every stranger, and buy nothing — the code
  is stable (v2.0 is from 2019).
- Vendored-unmodified + NOTICES + "do not modify" invariant is the standard
  and defensible pattern for this size.

**Residual to-dos:**
- Add the upstream URL (https://github.com/imabot2/serialib) and the exact
  vendored version/date to `THIRD_PARTY_NOTICES.md` so provenance is
  checkable. *(Effort: 10 min. Priority: DO FIRST.)*
- Note the license caveat honestly in NOTICES: serialib's header says
  "licence-free … can be used by anyone who try to build a better world"
  plus an MIT/X11-style warranty disclaimer. That is not an OSI license;
  the pragmatic reading (author intends free use, attribution preserved)
  should be stated as such rather than silently treated as MIT.
  *(Effort: 15 min. Priority: DO FIRST.)*

### 3.2 Committed vendor PDFs and driver zips are also third-party content

**What:** The datasheets/manuals under `docs/ESP32/` and
`docs/Robot_Hardware/`, and especially `CH34x_Install_Windows_v3_4.zip` /
`CP210x_Universal_Windows_Driver.zip`, are copyrighted vendor material.
Redistributing driver installers from your repo is both a rights problem
and a trust anti-pattern (nobody should fetch drivers from a personal
GitHub repo). Handle via 5.1 (remove + link).

**Why:** Before making the repo a public showcase, this is the one
plausible compliance issue left. **Priority: DO FIRST** (as part of 5.1).

### 3.3 Own-code license — done, one nit

`LICENSE` (MIT, © 2026) is in place and the README License & Credits
section is correct. Optional nit: no per-file license headers needed for a
project this size — the top-level LICENSE + NOTICES is sufficient and
keeping source files clean is fine. No action.

---

## 4. Collaboration enablement

### 4.1 CONTRIBUTING.md

**What:** Short and honest — this is a hardware project, so say what
contributors *can* do without the arm: build both halves, fix host-side
bugs, improve docs/tooling; and what needs hardware (anything in
`open-issues.md` marked NEEDS HARDWARE VERIFICATION — link it). Include:
build instructions pointer, the four critical invariants from CLAUDE.md
(CTRL_MODE sync, `if constexpr` only, `com.cpp` mode-agnostic, serialib
read-only) as "rules of the codebase," the doc-maintenance rule
(change-log/open-issues updates expected with PRs), and PR expectations.

**Why:** The invariants currently live only in `CLAUDE.md` (an AI-tooling
file); contributors need them in a standard location. Also: GitHub shows a
"Contributing" link on the new-issue/new-PR pages automatically.
**Effort:** 1–2 h. **Priority: DO FIRST.**

### 4.2 Issue & PR templates, CODE_OF_CONDUCT

**What:** `.github/ISSUE_TEMPLATE/bug_report.yml` (fields: CTRL_MODE, both
config.h in sync? OS, baud, hardware attached or bare ESP32, CSV header
lines), `feature_request.yml`, `.github/pull_request_template.md`
(checklist: builds on Linux, docs updated per the doc-maintenance rule,
change-log entry added). `CODE_OF_CONDUCT.md` = stock Contributor Covenant
2.1 with your contact email.

**Why:** Cheap, standard, and GitHub's "community standards" checklist
(visible on the Insights tab) goes green — a signal both recruiters and
contributors see.
**Effort:** 1–2 h total. **Priority: DO FIRST** (templates), CODE_OF_CONDUCT
is 10 min.

### 4.3 CI — GitHub Actions build check

**What:** `.github/workflows/build.yml`: matrix over `ubuntu-22.04` and
`windows-latest`, configure + build `serialcomm_c` with CMake, treat
warnings as errors on the CI build (`-Werror` via a CI-only flag, not in
the default CMakeLists). Optional second job: `clang-tidy` using the
existing `.clang-tidy` config (start non-blocking). Add the badge to the
README hero. No hardware tests — CI proves "a stranger's clone compiles,"
which is exactly the adoption promise in §2.

**Why:** The single strongest "this repo is maintained" signal, and it
makes the Windows build claim in the README continuously true instead of
aspirationally true.
**Effort:** 2–4 h (Windows runner quirks). **Priority: DO FIRST.**

### 4.4 Tagged release

**What:** After the polish lands: tag `v1.0.0`, write brief release notes
(modes working, known open issues linked). Because CTRL_MODE/protocol
must match the firmware, state the compatible firmware tag in the notes —
this is the lightweight answer to cross-repo version sync (see 6.1).

**Why:** A release marks the repo as "shipped," not "abandoned mid-flight,"
and gives the firmware repo something concrete to declare compatibility
with. **Effort:** 30 min. **Priority: NICE TO HAVE** (but do it before
sending the link to recruiters).

---

## 5. Repo hygiene

### 5.1 Remove the 35 MB of vendor binaries from git (the big one)

**What:**
1. Create `docs/hardware.md` (2.2) with a link table replacing every PDF:
   component → official vendor datasheet URL. For the two lab-specific PDFs
   with no public URL (`FlexiLinkRobot_Platine_11-2024.pdf`, the Quanser
   flexible-link manuals), either keep just those few (they're the small
   ones) or attach them to a GitHub Release instead of the tree.
2. Drop the driver `.zip`s unconditionally — link to Silicon Labs / WCH
   official download pages instead.
3. `git rm` the removed files; add `docs/**/*.pdf` (or the specific dirs)
   and `*.zip` to `.gitignore` if you keep local copies.
4. **Decide on history rewrite:** removing the files from HEAD still leaves
   ~35 MB in `.git` history, so every clone pays for them forever. Since
   the repo has no external clones yet, this is the one moment a rewrite
   (`git filter-repo` on `docs/ESP32/` zips + the largest PDFs) is cheap
   and victimless. After going public it never is. Recommendation: do it
   now. ⚠️ Destructive to history — your call, coordinate with any other
   checkouts you have.

**Why:** Clone size is a first-impression metric; driver zips are a
rights/trust problem (3.2); and datasheets rot — links with a "retrieved
2026-07" note age better than frozen copies.
**Effort:** 2–3 h including the link hunt. **Priority: DO FIRST.**

### 5.2 .gitignore cleanup

**What:** Remove the stale `firmware/.pio` etc. block (no `firmware/` dir
in this repo). Keep `*.csv` but consider `!data/analyze_timing_data.py`
style comments explaining *why* (experiment logs are local artifacts).
Review the `.vscode/` ignore: currently the four `.vscode` files are
untracked — for a showcase repo, *committing* a minimal
`.vscode/extensions.json` (recommends clangd + PlatformIO) helps strangers;
the rest can stay ignored. Delete the local `serialcomm_c/serialcomm.exe`
(untracked, already ignore-matched — just clutter, and a Windows `.exe`
sitting inside a Linux checkout confuses).

**Why:** A stale .gitignore quietly says "copy-pasted, never read."
**Effort:** 30 min. **Priority: DO FIRST** (trivial, bundle with 5.1's
commit).

### 5.3 CLAUDE.md — commit or ignore, but decide

**What:** It's currently untracked (shows as `??`). Two defensible options:
commit it (it documents real process discipline, and AI-assisted-dev
process is itself a current interview topic) or add it to `.gitignore` as
personal tooling. Recommendation: **commit it** — it references `docs/`
files that are already public and the doc-maintenance table is genuinely
part of how this repo works.

**Why:** An untracked file in the root is a loose end either way.
**Effort:** 5 min. **Priority: DO FIRST.**

### 5.4 Mono vs multi-repo — decided: stay separate

Locked decision, recorded here with the follow-through it implies:
- Each README links prominently to the other repo (this one mentions
  `flexbot_firmware/` but — verify after the firmware repo is published —
  needs an actual GitHub URL, not a local path).
- Cross-repo protocol compatibility is handled by release tags (4.4): each
  release states the firmware tag it pairs with. A shared
  "protocol version" constant is over-engineering at this scale; a
  compatibility line in release notes is enough.
- The stale `firmware/` block in `.gitignore` (5.2) is a leftover from when
  the repos were together — removing it closes that chapter.

### 5.5 What should / shouldn't be committed — summary policy

| Commit | Don't commit |
|---|---|
| Source, CMakeLists, configs | `build/`, `.cache/`, `compile_commands.json` (already ignored) |
| `docs/*.md`, `docs/specs/`, `docs/media/` (small images/GIF) | Experiment CSVs (`*.csv`, already ignored) |
| `.clang-tidy`, `.clangd` — **currently ignored by `*.clang-tidy`/`*.clangd` patterns; un-ignore and commit them** so contributors get the same lint config | Binaries: `*.exe`, driver zips, multi-MB vendor PDFs |
| `.github/` (CI, templates) | Personal editor state (`.vscode/` except `extensions.json`) |
| `CLAUDE.md` (per 5.3) | |

Note the `.clang-tidy`/`.clangd` point: the ignore patterns `*.clang-tidy`
and `*.clangd` currently exclude the project's own lint config from the
repo — almost certainly unintended. *(Effort: 10 min. Priority: DO FIRST,
bundle with 5.2.)*

---

## 6. Explicitly out of scope (tracked so they aren't forgotten)

- **Firmware repo showcase pass** — separate later task. Two known gotchas
  to carry into that session: (a) the firmware has a pre-existing original
  author → joint copyright; the MIT-relicensing question needs care there,
  don't assume this repo's LICENSE pattern transfers; (b) PlatformIO
  `lib_deps` pull in third-party libraries → the firmware needs its own
  license/NOTICES audit; nothing from this repo's audit covers it.
- **Monorepo merge** — rejected, not deferred.
- **No-hardware simulation mode** — listed as 2.3 stretch; treat as
  post-showcase.

---

## 7. Ordered checklist

Work top to bottom. Items in one block are natural single commits.

**Phase 1 — hygiene first (do before anything is publicized; contains the
one destructive-history decision):**
- [ ] 1. Write `docs/hardware.md` with BOM + datasheet link table (2.2)
- [ ] 2. `git rm` vendor PDFs + driver zips; keep/Release-attach the 2–3
      lab-specific PDFs with no public URL (5.1)
- [ ] 3. Decide + (recommended) run `git filter-repo` history rewrite while
      the repo has no clones (5.1.4) ⚠️ destructive, user executes
- [ ] 4. `.gitignore` cleanup: drop stale `firmware/` block; un-ignore and
      commit `.clang-tidy`/`.clangd`; delete local `serialcomm.exe` (5.2, 5.5)
- [ ] 5. Commit `CLAUDE.md` (5.3)
- [ ] 6. Doc consistency pass: fix baud contradiction, align build
      commands, refresh README layout tree (2.4)

**Phase 2 — licensing residuals (quick):**
- [ ] 7. Add upstream URL + version provenance to `THIRD_PARTY_NOTICES.md`;
      state the "licence-free" caveat honestly (3.1)

**Phase 3 — recruiter-facing:**
- [ ] 8. Capture arm photo + working GIF; add `docs/media/`; optional CSV
      plot via `analyze_timing_data.py` (1.2 — needs hardware, schedule
      independently)
- [ ] 9. README hero rewrite: pitch, media, badges, quick-start block
      (1.1 + 2.1)
- [ ] 10. Mermaid architecture diagram in README (1.3)
- [ ] 11. "Design decisions" section distilled from change-log (1.4)
- [ ] 12. GitHub metadata: description, topics, social preview (1.5)

**Phase 4 — collaboration:**
- [ ] 13. CI workflow: Linux + Windows build matrix, README badge (4.3)
- [ ] 14. CONTRIBUTING.md with codebase invariants + hardware-honesty
      section (4.1)
- [ ] 15. Issue/PR templates + CODE_OF_CONDUCT (4.2)

**Phase 5 — ship it:**
- [ ] 16. Tag `v1.0.0` with firmware-compatibility note in release notes
      (4.4)
- [ ] 17. Update `docs/open-issues.md` / `change-log.md` per the
      doc-maintenance rule as each phase lands

**Later / nice to have:**
- [ ] 18. No-hardware fake-serial demo mode + CI smoke test (2.3)
- [ ] 19. Firmware repo showcase pass (separate session; carry the two
      gotchas from §6)

### Effort summary

| Phase | Estimate |
|---|---|
| 1 (hygiene) | ~1 day |
| 2 (licensing) | <1 h |
| 3 (recruiter) | ~1 day + hardware session for media |
| 4 (collaboration) | ~1 day |
| 5 (ship) | ~1 h |

Total: roughly **3–4 focused days** to a repo you can confidently pin on
your profile and link in applications, with the media capture (item 8) as
the only step that requires the physical arm.
