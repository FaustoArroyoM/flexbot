#include "config.h"
#include "serialib.h"
#include <algorithm>
#include <array>
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
  for (const Sample &s : data) {
    file << s.pos1 << " " << s.pos2 << " " << s.strain1 << " " << s.strain2
         << " " << s.strain1div << " " << s.strain2div << " " << s.cycle_time_ms
         << " " << s.out1 << " " << s.out2 << "\n";
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
    // Read the string:
    serial.readBytes(buffer_read, PACKET_READ_SIZE, SERIAL_TIMEOUT_MS);

    const int identifier_read =
        static_cast<int16_t>(buffer_read[0] | buffer_read[1] << 8);

    if (identifier_read == FLAG_SEND) {
      data[i].pos1 = extractField(buffer_read, 2, 100.0F);
      data[i].pos2 = extractField(buffer_read, 4, 100.0F);
      data[i].strain1 = extractField(buffer_read, 6, 1000.0F);
      data[i].strain2 = extractField(buffer_read, 8, 1000.0F);
      data[i].strain1div = extractField(buffer_read, 10, 100.0F);
      data[i].strain2div = extractField(buffer_read, 12, 100.0F);
      data[i].cycle_time_ms = extractField(buffer_read, 14, 100.0F);

      // std::cout << "Cycle in ms: " << data[i].cycle_time_ms << std::endl;

      // Compute Control Output
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
      int out1d = static_cast<int>(256.0f / 3.3f * out1_raw + OUT_NEUTRAL_1);
      int out2d = static_cast<int>(256.0f / 3.3f * out2_raw + OUT_NEUTRAL_2);

      // Clamp to valid uint8 range using std::clamp (C++17)
      out1d = std::clamp(out1d, OUT_MIN, OUT_MAX);
      out2d = std::clamp(out2d, OUT_MIN, OUT_MAX);

      // --- Stage 3: pack into the 3-byte packet and send ---
      buffer_write[0] = FLAG_CONTROL;
      buffer_write[1] = static_cast<unsigned char>(out1d);
      buffer_write[2] = static_cast<unsigned char>(out2d);
      serial.writeBytes(buffer_write, PACKET_WRITE_SIZE);

    } else {
      std::cout << "Error! Unexpected packet identifier.\n";
      break;
    }
  }

  buffer_write[0] = FLAG_STARTSTOP;
  buffer_write[1] = 1;
  buffer_write[2] = 1;
  serial.writeBytes(buffer_write, PACKET_WRITE_SIZE);

  return data;
};

int main() {
  const int len = askSimulationLength();

  serialib serial;
  if (!openConnection(serial))
    return 1;

  const std::vector<Sample> data = runControlLoop(serial, len);

  // Close the serial device
  sleep(1);
  serial.closeDevice();
  std::cout << "Done!";

  saveToFile(data, "data.txt");
  return 0;
}