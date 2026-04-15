#include "config.h"
#include "serialib.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

// #include <stdint.h>
// #include <stdio.h>
#include <unistd.h>

int askSimulationLength() {
  int len = 0;
  std::cout << "Give simulation length as an integer: ";
  std::cin >> len;
  return len;
}

bool openConnection(serialib &serial) {
  char err = serial.openDevice(SERIAL_PORT, BAUD_RATE);
  if (err != 1) {
    std::cout << "No connection to " << SERIAL_PORT << " possible.\n";
    return false;
  }
  std::cout << "Successful connection to " << SERIAL_PORT << "\n";
  sleep(1);
  serial.flushReceiver();
  sleep(1);
  return true;
}

void saveToFile(const std::vector<Sample> &data, const std::string &filename) {
  std::ofstream file(filename);
  if (!file.is_open()) {
    std::cout << "Unable to open file: " << filename << "\n";
    return;
  }

  // Metadata rows prefixed for  -> df = pd.read_csv("run_XXX.csv", comment='#')
  file << "# CTRL_MODE: " << static_cast<int>(CTRL_MODE) << "\n";
  file << "# K_motor1: " << K[0][0] << "," << K[0][1] << "," << K[0][2] << ","
       << K[0][3] << "," << K[0][4] << "," << K[0][5] << "\n";
  file << "# K_motor2: " << K[1][0] << "," << K[1][1] << "," << K[1][2] << ","
       << K[1][3] << "," << K[1][4] << "," << K[1][5] << "\n";
  file << "# samples: " << data.size() << "\n";

  // CSV header row
  file << "pos1,pos2,strain1,strain2,"
       << "strain1div,strain2div,cycle_time_ms,out1,out2\n";

  for (const Sample &s : data) {
    file << s.pos1 << "," << s.pos2 << "," << s.strain1 << "," << s.strain2
         << "," << s.strain1div << "," << s.strain2div << "," << s.cycle_time_ms
         << "," << s.out1 << "," << s.out2 << "\n";
  }
}

//  Helper for runControlLoop
// Extracts one int16 value from two consecutive bytes in a buffer
// const uint8_t* buf  — read-only pointer to the buffer (no copy)
// int offset          — which byte position to start reading from
// float scale         — divide by this to recover the original float
float extractField(const uint8_t *buf, int offset, float scale) {
  return static_cast<float>(
             static_cast<int16_t>(buf[offset] | buf[offset + 1] << 8)) /
         scale;
}

// =============================================================================
// runControlLoop — behaviour depends on CTRL_MODE (compile-time constant)
//
// HIGH_LEVEL: PC computes K*x, sends DAC values, waits for each ESP32 reply.
//             Tight coupling — latency ~10 ms per sample.
//
// LOW_LEVEL:  PC is a passive logger. Reads sensor stream from ESP32, logs it.
//             Never sends control packets. ESP32 runs PI autonomously.
//
// HYBRID:     PC sends slow reference positions (~10 ms). ESP32 inner PI loop
//             tracks them at 1 ms. PC logs the resulting sensor data.
// =============================================================================
std::vector<Sample> runControlLoop(serialib &serial, int len) {
  std::vector<Sample> data(len);

  unsigned char buffer_write[PACKET_WRITE_SIZE] = {0};
  unsigned char buffer_read[PACKET_READ_SIZE] = {0};

  // float randwalk1 = 0;
  // float randwalk2 = 0;

  // Send START command
  buffer_write[0] = FLAG_STARTSTOP;
  buffer_write[1] = 0;
  buffer_write[2] = 0;
  serial.writeBytes(buffer_write, PACKET_WRITE_SIZE);

  // Start of the Control loop
  for (int i = 0; i < len; i++) {
    // Read sensor packet
    // auto t_read_start = std::chrono::high_resolution_clock::now();
    serial.readBytes(buffer_read, PACKET_READ_SIZE, SERIAL_TIMEOUT_MS);
    // auto t_read_done = std::chrono::high_resolution_clock::now();

    const int identifier_read =
        static_cast<int16_t>(buffer_read[0] | buffer_read[1] << 8);

    if (identifier_read != FLAG_SEND) {
      std::cout << "Error! Unexpected packet identifier at sample " << i
                << "\n";
      break;
    }

    // Decode sensor fields
    data[i].pos1 = extractField(buffer_read, 2, 100.0F);
    data[i].pos2 = extractField(buffer_read, 4, 100.0F);
    data[i].strain1 = extractField(buffer_read, 6, 1000.0F);
    data[i].strain2 = extractField(buffer_read, 8, 1000.0F);
    data[i].strain1div = extractField(buffer_read, 10, 100.0F);
    data[i].strain2div = extractField(buffer_read, 12, 100.0F);
    data[i].cycle_time_ms = extractField(buffer_read, 14, 100.0F);

    // std::cout << "Cycle in ms: " << data[i].cycle_time_ms << std::endl;

    // ── Step 2: Compute and send response depending on mode ────────────

    if constexpr (CTRL_MODE == CtrlMode::HIGH_LEVEL) {
      const std::array<float, 6> state = {
          data[i].pos1,    data[i].pos2,       data[i].strain1,
          data[i].strain2, data[i].strain1div, data[i].strain2div};

      float out1_raw = 0.0f;
      float out2_raw = 0.0f;

      for (int k = 0; k < 6; k++) {
        out1_raw += K[0][k] * state[k]; // motor 1
        out2_raw += K[1][k] * state[k]; // motor 2
      }

      // random walk for PE:
      // randwalk1 += 0.08*(float(rand())/RAND_MAX - 0.5);
      // randwalk2 += 0.04*(float(rand())/RAND_MAX - 0.5);
      // outs[i][0] += randwalk1;
      // outs[i][1] += randwalk2;

      // Store the raw float outputs in the Sample for later logging
      data[i].out1 = out1_raw;
      data[i].out2 = out2_raw;

      // --- Stage 2: convert float → int, clamped to [0, 255]
      int out1d = static_cast<int>(DAC_SCALE * out1_raw + OUT_NEUTRAL_1);
      int out2d = static_cast<int>(DAC_SCALE * out2_raw + OUT_NEUTRAL_2);

      // Clamp to valid uint8 range using std::clamp (C++17)
      out1d = std::clamp(out1d, OUT_MIN, OUT_MAX);
      out2d = std::clamp(out2d, OUT_MIN, OUT_MAX);

      // --- Stage 3: pack into the 3-byte packet and send ---
      buffer_write[0] = FLAG_CONTROL;
      buffer_write[1] = static_cast<unsigned char>(out1d);
      buffer_write[2] = static_cast<unsigned char>(out2d);

      serial.writeBytes(buffer_write, PACKET_WRITE_SIZE);
      // auto t_write_done = std::chrono::high_resolution_clock::now();

    } else if constexpr (CTRL_MODE == CtrlMode::LOW_LEVEL) {
      // PC does NOT send any control packet.
      // The ESP32 is running its own PI loop; we are just an observer.
      // Log what the ESP32 decided (out1/out2 fields will be 0 — that's fine,
      // because we don't know the ESP32's internal output from this side).
      data[i].out1 = 0.0f;
      data[i].out2 = 0.0f;
      // TODO No serial.writeBytes() here — intentional. maybe send 0's if bugs

    } else if constexpr (CTRL_MODE == CtrlMode::HYBRID) {
      // K*x gives a voltage command — same computation as HIGH_LEVEL.
      // We send it as a DAC uint8, same packet, same format.
      // On the ESP32 side, cycle() will use this as a *reference* for the PI,
      // not write it directly to the DAC. That distinction lives only in
      // main.cpp.
      const std::array<float, 6> x = {data[i].pos1,       data[i].pos2,
                                      data[i].strain1,    data[i].strain2,
                                      data[i].strain1div, data[i].strain2div};
      float out1_raw = 0.0f, out2_raw = 0.0f;
      for (int k = 0; k < 6; k++) {
        out1_raw += K[0][k] * x[k];
        out2_raw += K[1][k] * x[k];
      }
      data[i].out1 = out1_raw;
      data[i].out2 = out2_raw;

      // Same conversion as HIGH_LEVEL — DAC_SCALE already exists in config.h
      int out1d = static_cast<int>(DAC_SCALE * out1_raw + OUT_NEUTRAL_1);
      int out2d = static_cast<int>(DAC_SCALE * out2_raw + OUT_NEUTRAL_2);
      out1d = std::clamp(out1d, OUT_MIN, OUT_MAX);
      out2d = std::clamp(out2d, OUT_MIN, OUT_MAX);

      buffer_write[0] = FLAG_CONTROL;
      buffer_write[1] = static_cast<uint8_t>(out1d);
      buffer_write[2] = static_cast<uint8_t>(out2d);
      serial.writeBytes(buffer_write, PACKET_WRITE_SIZE);
    }

    buffer_write[0] = FLAG_STARTSTOP;
    buffer_write[1] = 1;
    buffer_write[2] = 1;
    serial.writeBytes(buffer_write, PACKET_WRITE_SIZE);

    return data;
  };

  int main(int argc, char *argv[]) {
    const int len = askSimulationLength();

    serialib serial;
    if (!openConnection(serial))
      return 1;

    const std::vector<Sample> data = runControlLoop(serial, len);

    // Close the serial device
    sleep(1);
    serial.closeDevice();
    std::cout << "Done!";

    // Save measurements to desired location
    std::string filepath;
    if (argc > 1) {
      filepath = argv[1];
    } else {
      const std::filesystem::path exe_dir =
          std::filesystem::canonical("/proc/self/exe").parent_path();
      const std::filesystem::path data_dir = exe_dir.parent_path() / "data";
      std::filesystem::create_directories(data_dir);

      const auto now = std::chrono::system_clock::now();
      const std::time_t t = std::chrono::system_clock::to_time_t(now);
      char timestamp[20];
      std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M",
                    std::localtime(&t));
      filepath =
          (data_dir / (std::string("output_") + timestamp + ".csv")).string();
    }

    saveToFile(data, filepath);
    std::cout << "Data saved to: " << filepath << "\n";
    return 0;
  }