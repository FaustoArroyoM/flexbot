#pragma once // modern alternative to #ifndef guards
#include <cstdint>

struct Sample {
  float pos1 = 0.0f;
  float pos2 = 0.0f;
  float strain1 = 0.0f;
  float strain2 = 0.0f;
  float strain1div = 0.0f;
  float strain2div = 0.0f;
  float cycle_time_ms = 0.0f;
  float out1 = 0.0f;
  float out2 = 0.0f;
};

// Change Gain Matrix here and recompile!
// Gain Matrix K [2 motors][6 states]
// Each row: {K_pos, K_vel, K_strain, K_straindot, 0, 0}

constexpr float K[2][6] = {
    {-2.0F, 0.0F, 0.10F, 0.00F, 0.0F, 0.0F}, // motor 1
    {0.0F, -3.0F, 0.00F, 0.05F, 0.0F, 0.0F}  // motor 2
};

// const float K[2][6] = {0, 0, 0.1, 0, 0, 0, 0, 0, 0, 0.1, 0, 0};

// const float K[2][6] = {-0.0356, -1.1374, 0.018,  0.1872, 0, 0,
//    -0.0362, -1.2301, 0.0177, 0.1939, 0, 0};

// SERIAL_PORT when switching between Linux and Windows
#ifdef _WIN32
constexpr const char *SERIAL_PORT = "COM3";
#else
constexpr const char *SERIAL_PORT = "/dev/ttyUSB0";
#endif

constexpr unsigned int BAUD_RATE = 115200;

// Protocol Flags must match ESP32 com.cpp exactly!
constexpr uint8_t FLAG_STARTSTOP = 120;
constexpr uint8_t FLAG_CONTROL = 99;
constexpr uint8_t FLAG_SEND = 109;

constexpr int PACKET_WRITE_SIZE = 3; // PC→ESP32: flag + two outputs
constexpr int PACKET_READ_SIZE = 16; // ESP32→PC: pos, strain, time

constexpr int OUT_NEUTRAL_1 = 127;
constexpr int OUT_NEUTRAL_2 = 125;
constexpr int OUT_MAX = 255;
constexpr int OUT_MIN = 0;

constexpr float DAC_SCALE = 256.0F / 3.3F; // voltage → uint8 scaling
constexpr int SERIAL_TIMEOUT_MS = 2000;    // readBytes timeout

// constexpr const char* OUTPUT_FILE =
// "/home/fausto/Desktop/flexbot/data/output.csv";