# FlexBot — PC Software (`serialcomm_c`)

PC-side control and data logging software for the FlexBot 2-DOF robot arm.  
Communicates with the ESP32 firmware over USB serial. Written in **C++17**, built with **CMake**, linted with **clangd**.

---

## Project Structure

```
serialcomm_c/
├── include/
│   ├── config.h        ← All tuning parameters and control mode switch
│   └── serialib.h      ← Third-party serial library (do not modify)
├── src/
│   ├── serialcomm.cpp  ← Main program: control loop, logging, serial I/O
│   └── serialib.cpp    ← Third-party serial library (do not modify)
├── CMakeLists.txt
└── README.md
```

Output data is saved to:
```
flexbot/data/output_YYYYMMDD_HHMM.csv
```

---

## Requirements

### Ubuntu 22.04

```bash
sudo apt update
sudo apt install build-essential cmake clangd
```

### Windows

- [CMake](https://cmake.org/download/) (add to PATH during install)
- [Visual Studio Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/) with "Desktop development with C++"
- [LLVM/clangd](https://github.com/clangd/clangd/releases) (optional, for VS Code IntelliSense)

---

## Build & Run

### Linux (Ubuntu)

```bash
cd ~/Desktop/flexbot
mkdir -p build && cd build
cmake ../serialcomm_c
make -j$(nproc)
./serialcomm        # uses default output path under ../data/
# or with explicit output path:
./serialcomm /path/to/output.csv
```

### Windows

```powershell
cd C:\path\to\flexbot
mkdir build && cd build
cmake ..\serialcomm_c -G "Visual Studio 17 2022"
cmake --build . --config Release
.\Release\serialcomm.exe
```

---

## Serial Port Configuration

The serial port is auto-selected by platform in `include/config.h`:

```cpp
#ifdef _WIN32
    constexpr const char* SERIAL_PORT = "COM3";   // ← change to your port
#else
    constexpr const char* SERIAL_PORT = "/dev/ttyUSB0";  // ← change if needed
#endif
```

**Linux:** Check which port the ESP32 appears on:
```bash
ls /dev/ttyUSB*
# or watch for it when you plug in:
dmesg | tail -n 10
```

**Linux USB permissions** (required once per user):
```bash
sudo usermod -aG dialout $USER
# then log out and back in
```

**Windows:** Open Device Manager → Ports (COM & LPT) to find the COM number.

---

## Changing the Control Mode

Open `include/config.h` and change **one line**, then recompile:

```cpp
constexpr CtrlMode CTRL_MODE = CtrlMode::HIGH_LEVEL;  // ← change this
```

| Mode | What the PC does | What the ESP32 does |
|---|---|---|
| `HIGH_LEVEL` | Computes full K·x, sends DAC value each cycle | Applies output directly to DAC — no local control |
| `LOW_LEVEL` | Passive logger only — reads and saves data | Runs PI controller autonomously at 1 ms |
| `HYBRID` | Computes K·x, sends as setpoint each cycle | Runs PI that tracks the PC's setpoint at 1 ms |

> **Important:** You must set the **same mode** in both `serialcomm_c/include/config.h` **and** `flexbot_firmware/include/config.h`, then recompile and reflash both.

---

## Tuning the Gain Matrix (HIGH_LEVEL / HYBRID)

In `include/config.h`:

```cpp
// Gain Matrix K [2 motors][6 states]
// Each row: { K_pos, K_vel, K_strain, K_straindot, 0, 0 }
constexpr float K[2][6] = {
    {-2.0F, 0.0F, 0.10F, 0.00F, 0.0F, 0.0F},  // motor 1
    { 0.0F,-3.0F, 0.00F, 0.05F, 0.0F, 0.0F}   // motor 2
};
```

Change values and recompile. The gain matrix is written as a comment in every CSV output file so you always know which gains produced which data.

---

## Serial Protocol

All values must match the ESP32 firmware exactly (they are defined identically in both `config.h` files).

| Constant | Value | Meaning |
|---|---|---|
| `BAUD_RATE` | 115200 | Serial baud rate |
| `FLAG_STARTSTOP` | 120 | Start (`byte[1]=0`) or Stop (`byte[1]=1`) command |
| `FLAG_CONTROL` | 99 | PC → ESP32 control/reference packet |
| `FLAG_SEND` | 109 | ESP32 → PC sensor data packet |
| `PACKET_WRITE_SIZE` | 3 bytes | PC → ESP32: `[flag, out1, out2]` |
| `PACKET_READ_SIZE` | 16 bytes | ESP32 → PC: `[flag, pad, pos1×2, pos2×2, s1×2, s2×2, s1d×2, s2d×2, time×2]` |

---

## CSV Output Format

Each run produces a timestamped CSV with metadata comment lines:

```
# CTRL_MODE: 0
# K_motor1: -2.0,0.0,0.1,0.0,0.0,0.0
# K_motor2: 0.0,-3.0,0.0,0.05,0.0,0.0
# samples: 1000
pos1,pos2,strain1,strain2,strain1div,strain2div,cycle_time_ms,out1,out2
0.12,0.08,...
```

Read in Python, skipping comment lines:
```python
import pandas as pd
df = pd.read_csv("output_20260415_1430.csv", comment='#')
```

---

## VS Code Setup (clangd IntelliSense)

1. Install the [clangd extension](https://marketplace.visualstudio.com/items?itemName=llvm-vs-code-extensions.vscode-clangd)
2. Build once with CMake — this generates `build/compile_commands.json`
3. Add to `.vscode/settings.json`:
```json
{
    "clangd.arguments": [
        "--compile-commands-dir=${workspaceFolder}/build"
    ]
}
```
clangd will now provide full type-checking and autocomplete.

---

## Troubleshooting

| Problem | Fix |
|---|---|
| `No connection to /dev/ttyUSB0` | Check cable, run `ls /dev/ttyUSB*`, verify permissions (`dialout` group) |
| `Unexpected packet identifier` | Both repos must have the same `CTRL_MODE`. Reflash ESP32 after changing. |
| Build fails on Windows with `filesystem` errors | Requires MSVC 19.14+ or GCC 8+. Update Visual Studio Build Tools. |
| clangd shows errors but build works | Regenerate `compile_commands.json`: delete `build/` and run CMake again. |