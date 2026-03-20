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

// using namespace std;

int main() {
  int userlen = 0;

  std::cout << "Give simulation length as an integer: ";
  std::cin >> userlen;

  const int len = userlen;

  std::vector<Sample> data(len);
  // float nums[len][7] = {0};
  // float outs[len][2] = {0};

  // int out1d = 0;
  // int out2d = 0;
  // float times[len] = {0};
  unsigned char buffer_write[3] = {0};
  unsigned char buffer_read[16] = {0};
  int identifier_read = 0;

  // float randwalk1 = 0;
  // float randwalk2 = 0;

  // Serial object
  serialib serial;

  // Connection to serial port
  char errorOpening = serial.openDevice(SERIAL_PORT, BAUD_RATE);

  // If connection fails, return the error code otherwise, display a success
  // message
  if (errorOpening != 1) {
    std::cout << "No connection to " << SERIAL_PORT << " possible.."
              << std::endl;
    sleep(1);
    return errorOpening;
  }
  std::cout << "Successful connection to " << SERIAL_PORT << std::endl;

  sleep(1);
  serial.flushReceiver();
  sleep(1);

  // Write the string on the serial device
  buffer_write[0] = FLAG_STARTSTOP;
  buffer_write[1] = 0;
  buffer_write[2] = 0;
  serial.writeBytes(buffer_write, 3);

  for (int i = 0; i < len; i++) {
    // Read the string:
    serial.readBytes(buffer_read, 16, 2000);

    identifier_read = int16_t(buffer_read[0] | buffer_read[1] << 8);
    if (identifier_read == FLAG_SEND) {
      // Store data in preallocated dynamic array:
      data[i].pos1 =
          float(int16_t(buffer_read[2] | buffer_read[3] << 8) / 100.0);
      data[i].pos2 =
          float(int16_t(buffer_read[4] | buffer_read[5] << 8) / 100.0);
      data[i].strain1 =
          float(int16_t(buffer_read[6] | buffer_read[7] << 8) / 1000.0);
      data[i].strain2 =
          float(int16_t(buffer_read[8] | buffer_read[9] << 8) / 1000.0);
      data[i].strain1div =
          float(int16_t(buffer_read[10] | buffer_read[11] << 8) / 100.0);
      data[i].strain2div =
          float(int16_t(buffer_read[12] | buffer_read[13] << 8) / 100.0);
      data[i].cycle_time_ms =
          float(int16_t(buffer_read[14] | buffer_read[15] << 8) / 100.0);

      // std::cout << "Cycle in ms: " << data[i].cycle_time_ms << std::endl;

      // Creates current iteration state array
      const std::array<float, 6> state = {
          data[i].pos1,    data[i].pos2,       data[i].strain1,
          data[i].strain2, data[i].strain1div, data[i].strain2div};

      float out1_raw = 0.0f;
      float out2_raw = 0.0f;

      for (int k = 0; k < 6; k++) {
        out1_raw += K[0][k] * state[k]; // motor 1
        out2_raw += K[1][k] * state[k]; // motor 2
      }

      // // compute controls:
      // for (int j = 0; j < 2; j++) {
      //   for (int k = 0; k < 6; k++) {
      //     outs[i][j] += (K[j][k] * nums[i][k]);
      //   }
      // }

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

      // out1d = int(256 / 3.3 * outs[i][0] + 127);
      // out2d = int(256 / 3.3 * outs[i][1] + 127 - 2);
      // if (out1d > 255) {
      //   out1d = 255;
      // }
      // if (out1d < 0) {
      //   out1d = 0;
      // }
      // if (out2d > 255) {
      //   out2d = 255;
      // }
      // if (out2d < 0) {
      //   out2d = 0;
      // }

      // buffer_write[0] = FLAG_CONTROL;
      // buffer_write[1] = out1d;
      // buffer_write[2] = out2d;
      // serial.writeBytes(buffer_write, 3);

    } else {
      std::cout << "Error!" << std::endl;
      break;
    }
  }

  buffer_write[0] = FLAG_STARTSTOP;
  buffer_write[1] = 1;
  buffer_write[2] = 1;
  serial.writeBytes(buffer_write, 3);
  // Close the serial device
  sleep(1);
  serial.closeDevice();
  std::cout << "Done!" << std::endl;

  // for(int i = 0; i < len; i++){
  //     std::cout << "Cycle in ms: " << nums[i][6] << std::endl;
  // }

  // Store data in text file:
  std::ofstream file("data.txt");
  if (file.is_open()) {
    for (int i = 0; i < len; i++) {
      file << data[i].pos1 << " " << data[i].pos2 << " " << data[i].strain1
           << " " << data[i].strain2 << " " << data[i].strain1div << " "
           << data[i].strain2div << " " << data[i].cycle_time_ms << " "
           << data[i].out1 << " " << data[i].out2 << "\n";
    }

    // for (int i = 0; i < len; i++) {
    //   for (int j = 0; j <= 6; j++) {
    //     file << double(nums[i][j]) << " ";
    //   }
    //   for (int j = 0; j <= 1; j++) {
    //     file << double(outs[i][j]) << " ";
    //   }
    //   file << "\n";
    // }

    file.close();

  } else {
    std::cout << "Unable to open file\n";
  }
  return 0;
}