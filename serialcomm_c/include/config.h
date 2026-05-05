#pragma once // modern alternative to #ifndef guards
#include <cstdint>

// =====================================================================
// CONTROL MODE — VALUES MUST MATCH the other config.h at flexbot_firmware/include/config.h
// HIGH_LEVEL : PC computes K*x → sends raw DAC values to ESP32
// LOW_LEVEL  : ESP32 runs PI locally, PC only logs sensor data
// HYBRID     : ESP32 fast PI tracks reference sent by PC
// =====================================================================
enum class CtrlMode { HIGH_LEVEL, LOW_LEVEL, HYBRID };
constexpr CtrlMode CTRL_MODE = CtrlMode::HYBRID;


// =====================================================================
// HIGH LEVEL CONTROLLER gains
// Gain Matrix K [2 motors][6 states]
// Each row: {K_pos, K_vel, K_strain, K_straindot, 0, 0}
// =====================================================================
constexpr float K[2][6] = {
    {-2.0F, 0.0F, 0.10F, 0.00F, 0.0F, 0.0F}, // motor 1
    {0.0F, -3.0F, 0.00F, 0.05F, 0.0F, 0.0F}  // motor 2
};

// const float K[2][6] = {0, 0, 0.1, 0, 0, 0, 0, 0, 0, 0.1, 0, 0};

// const float K[2][6] = {-0.0356, -1.1374, 0.018,  0.1872, 0, 0,
//    -0.0362, -1.2301, 0.0177, 0.1939, 0, 0};



// SERIAL_PORT when switching between Linux and Windows
// =====================================================================
#ifdef _WIN32
constexpr const char *SERIAL_PORT = "COM3";
#else
constexpr const char *SERIAL_PORT = "/dev/ttyUSB0";
#endif

constexpr unsigned int BAUD_RATE = 115200;

// =====================================================================
// PROTOCOL FLAGS --- must match ESP32 flexbot_firmware/src/com.cpp exactly!
// =====================================================================

constexpr uint8_t FLAG_STARTSTOP = 120;
constexpr uint8_t FLAG_CONTROL = 99;
constexpr uint8_t FLAG_SEND = 109;

constexpr int PACKET_WRITE_SIZE = 3; // PC→ESP32: flag + two outputs
constexpr int PACKET_READ_SIZE = 16; // ESP32→PC: pos, strain, time

constexpr int OUT_NEUTRAL_1 = 127;
constexpr int OUT_NEUTRAL_2 = 125;
constexpr int OUT_MAX = 255;
constexpr int OUT_MIN = 0;

// =====================================================================
// HIGH_LEVEL/HYBRID position references (radians) used in u = -K(x - x_ref)
// =====================================================================
constexpr float REF_POS1 = 0.0F;
constexpr float REF_POS2 = 0.0F;


// =================================================================================================
// HYBRID CONTROL MODE  ONLY : HYBRID cascade tuning (PC outer loop -> reference bytes for ESP PI)
// =================================================================================================

// Scales outer-loop K*x contribution before converting to reference position.
constexpr float HYBRID_OUTER_K_SCALE = 0.10F;

// Maximum absolute position reference (rad) sent for each joint.
constexpr float HYBRID_REF_MAX_1 = 0.2F;
constexpr float HYBRID_REF_MAX_2 = 0.2F;

// [0,1], 1.0 = no smoothing, lower = smoother/slower ref updates.
constexpr float HYBRID_REF_SMOOTH_ALPHA = 0.6F;

// true = send neutral references only (PI-only behavior through HYBRID path)
constexpr bool HYBRID_FORCE_NEUTRAL_REF = false;

constexpr float DAC_SCALE = 256.0F / 3.3F; // voltage → uint8 scaling
constexpr int SERIAL_TIMEOUT_MS = 2000;    // readBytes timeout

// HYBRID mode: maximum encoder position range used to scale references into
// uint8. Tune these to match the physical travel range of each joint.


// =====================================================================
// SAVE TO CSV
// =====================================================================
struct Sample {
  float pos1 = 0.0f;
  float pos2 = 0.0f;
  float strain1 = 0.0f;
  float strain2 = 0.0f;
  float strain1div = 0.0f;
  float strain2div = 0.0f;
  float cycle_time_ms = 0.0f;
  float out1 = 0.0f;   // HIGH_LEVEL: K*x voltage. HYBRID: K*x voltage (before ref clamp)
  float out2 = 0.0f;
  float ref1 = 0.0f;   // HYBRID: decoded position reference sent to ESP32 (rad). 0 for other modes.
  float ref2 = 0.0f;   // HYBRID: decoded position reference sent to ESP32 (rad). 0 for other modes.
};