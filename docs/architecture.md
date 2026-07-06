# FlexBot Architecture Reference

> On-demand reference. Load with `@docs/architecture.md` when working on
> control flow, packet parsing, or FreeRTOS scheduling.

## Control architecture

Three modes, selected by a single compile-time constant in **both**
`include/config.h` files:

```cpp
enum class CtrlMode { HIGH_LEVEL, LOW_LEVEL, HYBRID };
constexpr CtrlMode CTRL_MODE = CtrlMode::HIGH_LEVEL;
```

| Mode         | Who computes output                  | PC role                          | Inner-loop latency |
| ------------ | ------------------------------------ | -------------------------------- | ------------------ |
| `HIGH_LEVEL` | PC computes K·x → sends DAC bytes    | Full state-feedback controller   | ~2 ms (COMM_TIME_MS=1) |
| `LOW_LEVEL`  | ESP32 runs PI locally                | Passive data logger only         | ~1 ms              |
| `HYBRID`     | ESP32 PI tracks PC ref               | Sends position references         | ~1 ms inner loop   |

All mode branching uses `if constexpr`. No runtime mode flag in packets.

## Serial protocol

```cpp
// PC config.h
constexpr int PACKET_WRITE_SIZE = 3;   // PC→ESP32: FLAG + 2 data bytes (all modes)
constexpr int PACKET_READ_SIZE  = 16;  // ESP32→PC: always 16 bytes

constexpr uint8_t FLAG_STARTSTOP = 120;
constexpr uint8_t FLAG_CONTROL   = 99;
constexpr uint8_t FLAG_SEND      = 109;
constexpr uint8_t FLAG_DUMP      = 77;   // PC→ESP32 end-of-run timing dump
constexpr uint8_t DUMP_MAGIC_0   = 0xA5; // dump framing marker (must match both repos)
constexpr uint8_t DUMP_MAGIC_1   = 0x5A;
constexpr uint8_t DUMP_MAGIC_2   = 0xC3;
constexpr uint8_t DUMP_MAGIC_3   = 0x3C;

// PC-side position references used by HIGH_LEVEL/HYBRID:
constexpr float REF_POS1 = 0.0F;
constexpr float REF_POS2 = 0.0F;
```

| Packet               | Direction  | Size     | Payload                                                                                          |
| -------------------- | ---------- | -------- | ------------------------------------------------------------------------------------------------ |
| Sensor data          | ESP32 → PC | 16 bytes | FLAG_SEND + pos1 + pos2 + strain1 + strain2 + strain1div + strain2div + time (all int16, scaled) |
| Control (HIGH_LEVEL) | PC → ESP32 | 3 bytes  | FLAG_CONTROL + out1_uint8 + out2_uint8                                                           |
| Reference (HYBRID)   | PC → ESP32 | 3 bytes  | FLAG_CONTROL + ref1_byte + ref2_byte (byte 127 = zero reference)                                 |
| Start                | PC → ESP32 | 3 bytes  | FLAG_STARTSTOP + (len>>8) + (len&0xFF) — experiment length as big-endian uint16                  |
| Stop                 | PC → ESP32 | 3 bytes  | FLAG_STARTSTOP + 0xFF + 0xFF — sentinel; can't collide with valid len (len=0 is invalid)         |
| Timing dump request  | PC → ESP32 | 3 bytes  | FLAG_DUMP + 0 + 0 (sent once after STOP at end of run)                                           |
| Timing dump payload  | ESP32 → PC | 6 + 4·N  | 4-byte magic + uint16 N (LE) + N × (int16 comp_us, int16 comm_us). See change-log §27/§28.      |

## ESP32 FreeRTOS task layout

```
Core 0                          Core 1
────────────────────            ──────────────────────────────
cycle()  @ 1 ms                 serialcomm()  @ 5 ms
  read encoders                   HIGH_LEVEL: transmit → wait for PC → repeat
  read strain gauges              LOW_LEVEL:  autonomous stream, drain RX
  lowpass filter                  HYBRID:     autonomous stream, receive ref
  compute output (mode)
  write DAC always                debug_msg() @ disabled
```

Shared state lives in a single `RobotState` struct. **All fields are
`volatile`** because two cores touch them.

Startup behavior: on stop→run transition, `cycle()` forces one neutral-DAC
tick before resuming mode-specific control. LOW/HYBRID PI integrator state is
reset on STOP and on the stop→run edge.

## RobotState struct (ESP32 `config.h`)

```cpp
struct RobotState {
    volatile bool  stop             = true;
    volatile bool  enable_transmit  = false;
    volatile bool  enable_debug     = false;
    volatile float pos1             = 0.0F;
    volatile float pos2             = 0.0F;
    volatile float strain1          = 0.0F;
    volatile float strain2          = 0.0F;
    volatile float strain1_filt     = 0.0F;
    volatile float strain2_filt     = 0.0F;
    volatile float strain1div_filt  = 0.0F;
    volatile float strain2div_filt  = 0.0F;
    volatile float out1_buffer      = 127.0F;
    volatile float out2_buffer      = 125.0F;
    volatile int64_t init_time      = 0;
    volatile int64_t end_time       = 0;
    volatile int   computation_time = 0;
    volatile int   communication_time = 0;
    volatile float load             = 0.0F;
    volatile float ref1             = 0.0F;   // HYBRID: feedforward from PC
    volatile float ref2             = 0.0F;
};
```

## HYBRID mode — cascade control design

HYBRID splits control across two loops with very different update rates:

```text
PC outer loop (~200 Hz)           ESP32 inner loop (1 kHz)
─────────────────────────         ──────────────────────────────────
Read sensors from packet          Read sensors from hardware
Compute  ref = -SCALE * K*x       Decode byte → ref_rad:
Clamp to ±HYBRID_REF_MAX (rad)      ref = ((byte-127)/127)*HYBRID_REF_MAX
EMA smooth reference              err = pos - ref_rad
Encode: byte=127+round(127*ref/MAX)  v = PI(err, strain, straindot)
Send FLAG_CONTROL+ref1b+ref2b     dac = DAC_SCALE*v + OUT_NEUTRAL
                                  clamp dac to [0,255], write DAC
```

**Encode/decode contract:** `HYBRID_REF_MAX_1/2` must be identical in
both `config.h` files. A mismatch makes the ESP32 see a reference
`PC_MAX/ESP32_MAX` times larger than intended (was 7.5× before §23 fix).

**Tuning knobs** (PC `serialcomm_c/include/config.h`):

| Knob | Effect | Start value |
|------|--------|-------------|
| `HYBRID_OUTER_K_SCALE` | Scales outer K·x contribution. 0 = inner PI only (baseline). | 0.05–0.10 |
| `HYBRID_REF_MAX_1/2` | Clips reference to ±MAX rad. Must match ESP32. | 0.2 rad |
| `HYBRID_REF_SMOOTH_ALPHA` | EMA on reference. 1.0 = no smoothing, ~0.3 = slow. | 0.6 |
| `HYBRID_FORCE_NEUTRAL_REF` | Forces ref=0; inner PI acts like LOW_LEVEL. | true first |

**CSV columns in HYBRID mode:**

- `out1/out2`: raw K·x voltage from outer loop (before clamp/encode)
- `ref1/ref2`: decoded position reference actually sent to ESP32 (rad)
