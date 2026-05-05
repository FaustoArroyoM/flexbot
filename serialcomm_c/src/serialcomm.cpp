#include "config.h"
#include "serialib.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

// SIGINT handler signals the loop to break and flush a partial CSV.
// Only signal-safe types are touched; the loop polls this every iteration.
std::atomic<bool> stop_requested{false};
void sigint_handler(int /*signum*/) {
  stop_requested.store(true, std::memory_order_relaxed);
}

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

void saveToFile(const std::vector<Sample> &data, const std::string &filename,
                const std::string &stop_reason) {
  std::ofstream file(filename);
  if (!file.is_open()) {
    std::cout << "Unable to open file: " << filename << "\n";
    return;
  }

  // Metadata rows prefixed with '#' so pandas can drop them with comment='#'.
  file << "# CTRL_MODE: " << static_cast<int>(CTRL_MODE) << "\n";
  file << "# K_motor1: " << K[0][0] << "," << K[0][1] << "," << K[0][2] << ","
       << K[0][3] << "," << K[0][4] << "," << K[0][5] << "\n";
  file << "# K_motor2: " << K[1][0] << "," << K[1][1] << "," << K[1][2] << ","
       << K[1][3] << "," << K[1][4] << "," << K[1][5] << "\n";
  file << "# samples: " << data.size() << "\n";
  file << "# stopped: " << stop_reason << "\n";

  // CSV header row
  // out1/out2: K*x voltage output from PC.
  // ref1/ref2: HYBRID only — decoded position reference sent to ESP32 (rad).
  file << "pos1,pos2,strain1,strain2,"
       << "strain1div,strain2div,cycle_time_ms,out1,out2,ref1,ref2\n";

  for (const Sample &s : data) {
    file << s.pos1 << "," << s.pos2 << "," << s.strain1 << "," << s.strain2
         << "," << s.strain1div << "," << s.strain2div << "," << s.cycle_time_ms
         << "," << s.out1 << "," << s.out2 << "," << s.ref1 << "," << s.ref2
         << "\n";
  }
}

// Helper for runControlLoop. Decodes one int16 from two consecutive bytes.
float extractField(const uint8_t *buf, int offset, float scale) {
  return static_cast<float>(
             static_cast<int16_t>(buf[offset] | buf[offset + 1] << 8)) /
         scale;
}

uint8_t encodeHybridRef(float ref, float ref_max) {
  const float r = std::clamp(ref, -ref_max, ref_max);
  const float norm = (ref_max > 0.0F) ? (r / ref_max) : 0.0F;
  const int byte = static_cast<int>(std::lround(127.0F + 127.0F * norm));
  return static_cast<uint8_t>(std::clamp(byte, 0, 255));
}

float decodeHybridRef(uint8_t byte, float ref_max) {
  const float norm = (static_cast<float>(byte) - 127.0F) / 127.0F;
  return norm * ref_max;
}

// =============================================================================
// runControlLoop — depends on CTRL_MODE (compile-time constant)
//
// HIGH_LEVEL: PC computes K*x, sends DAC values, waits for each ESP32 reply.
// LOW_LEVEL:  PC is a passive logger; ESP32 runs PI autonomously.
// HYBRID:     PC sends K*x as feedforward; ESP32 inner PI loop adds it.
//
// On break (SIGINT, timeout, bad packet) the loop returns whatever was
// collected so far. The reason is written into stop_reason (out parameter).
// =============================================================================
std::vector<Sample> runControlLoop(serialib &serial, int len,
                                   std::string &stop_reason) {
  std::vector<Sample> data;
  data.reserve(len);
  stop_reason = "completed";

  unsigned char buffer_write[PACKET_WRITE_SIZE] = {0};
  unsigned char buffer_read[PACKET_READ_SIZE] = {0};
  float hybrid_ref1_prev = 0.0F;
  float hybrid_ref2_prev = 0.0F;

  // Send START. ESP32 stops zeroing the encoder once stop=false.
  buffer_write[0] = FLAG_STARTSTOP;
  buffer_write[1] = 0;
  buffer_write[2] = 0;
  serial.writeBytes(buffer_write, PACKET_WRITE_SIZE);

  for (int i = 0; i < len; i++) {
    if (stop_requested.load(std::memory_order_relaxed)) {
      stop_reason = "user interrupt (Ctrl-C) at sample " + std::to_string(i);
      break;
    }

    const int n =
        serial.readBytes(buffer_read, PACKET_READ_SIZE, SERIAL_TIMEOUT_MS);
    if (n != PACKET_READ_SIZE) {
      stop_reason = "ESP32 stopped transmitting at sample " +
                    std::to_string(i) +
                    " (likely Hall safety trip or out-of-bounds)";
      break;
    }

    const int identifier_read =
        static_cast<int16_t>(buffer_read[0] | buffer_read[1] << 8);
    if (identifier_read != FLAG_SEND) {
      stop_reason = "unexpected packet identifier " +
                    std::to_string(identifier_read) + " at sample " +
                    std::to_string(i);
      break;
    }

    Sample s;
    s.pos1 = extractField(buffer_read, 2, 100.0F);
    s.pos2 = extractField(buffer_read, 4, 100.0F);
    s.strain1 = extractField(buffer_read, 6, 1000.0F);
    s.strain2 = extractField(buffer_read, 8, 1000.0F);
    s.strain1div = extractField(buffer_read, 10, 100.0F);
    s.strain2div = extractField(buffer_read, 12, 100.0F);
    s.cycle_time_ms = extractField(buffer_read, 14, 100.0F);

    if constexpr (CTRL_MODE == CtrlMode::HIGH_LEVEL) {
      const std::array<float, 6> state = {
          s.pos1, s.pos2, s.strain1, s.strain2, s.strain1div, s.strain2div};

      // const std::array<float, 6> state = {s.pos1 - REF_POS1, s.pos2 -
      // REF_POS2,
      //             s.strain1,         s.strain2,
      //             s.strain1div,      s.strain2div};

      float out1_raw = 0.0f;
      float out2_raw = 0.0f;
      for (int k = 0; k < 6; k++) {
        out1_raw += K[0][k] * state[k];
        out2_raw += K[1][k] * state[k];
      }
      s.out1 = out1_raw;
      s.out2 = out2_raw;

      int out1d = static_cast<int>(DAC_SCALE * out1_raw + OUT_NEUTRAL_1);
      int out2d = static_cast<int>(DAC_SCALE * out2_raw + OUT_NEUTRAL_2);
      out1d = std::clamp(out1d, OUT_MIN, OUT_MAX);
      out2d = std::clamp(out2d, OUT_MIN, OUT_MAX);

      buffer_write[0] = FLAG_CONTROL;
      buffer_write[1] = static_cast<unsigned char>(out1d);
      buffer_write[2] = static_cast<unsigned char>(out2d);
      serial.writeBytes(buffer_write, PACKET_WRITE_SIZE);

    } else if constexpr (CTRL_MODE == CtrlMode::LOW_LEVEL) {
      s.out1 = 0.0f;
      s.out2 = 0.0f;
      // No writeBytes — ESP32 runs PI autonomously.

    } else if constexpr (CTRL_MODE == CtrlMode::HYBRID) {
      // --- HYBRID outer loop (PC, ~200 Hz) ---
      //
      // Cascade control: this loop computes a *position reference* for the
      // ESP32 inner PI loop rather than a direct DAC voltage.
      //
      //  Step 1: K*x  — same gain matrix as HIGH_LEVEL, scaled by
      //          HYBRID_OUTER_K_SCALE (0→disable outer loop, 1→full K*x).
      //          The result (out1_raw) is a voltage-like signal but we
      //          reinterpret its magnitude as a position offset (rad).
      //          Tuning note: start with HYBRID_OUTER_K_SCALE ≤ 0.1.
      //
      //  Step 2: Clamp to ±HYBRID_REF_MAX (rad) — keeps reference inside
      //          the physical range the joint can actually reach.
      //          MUST match HYBRID_REF_MAX_1/2 in the ESP32 config.h.
      //
      //  Step 3: EMA smooth — ref = α*new + (1-α)*prev. Prevents the inner
      //          PI from chasing a jittery reference at 200 Hz bandwidth.
      //          HYBRID_REF_SMOOTH_ALPHA: 1.0 = no smoothing, ~0.3 = slow.
      //
      //  Step 4: Encode to byte — ref_byte = 127 + round(127 * ref/REF_MAX).
      //          127 = neutral (zero position reference).
      //          Sent as FLAG_CONTROL + ref1_byte + ref2_byte (3 bytes).
      //
      // HYBRID_FORCE_NEUTRAL_REF=true forces ref=0 at all times: ESP32 inner
      // PI then acts as a pure LOW_LEVEL stabiliser. Use this to verify the
      // inner loop works before enabling the outer loop.

      const std::array<float, 6> x = {s.pos1,    s.pos2,       s.strain1,
                                      s.strain2, s.strain1div, s.strain2div};

      // Step 1: outer K*x
      float out1_raw = 0.0f, out2_raw = 0.0f;
      for (int k = 0; k < 6; k++) {
        out1_raw += (HYBRID_OUTER_K_SCALE * K[0][k]) * x[k];
        out2_raw += (HYBRID_OUTER_K_SCALE * K[1][k]) * x[k];
      }
      s.out1 = out1_raw;  // log raw K*x for diagnosis
      s.out2 = out2_raw;

      // Step 2: clamp to physical reference range
      float ref1 = std::clamp(out1_raw, -HYBRID_REF_MAX_1, HYBRID_REF_MAX_1);
      float ref2 = std::clamp(out2_raw, -HYBRID_REF_MAX_2, HYBRID_REF_MAX_2);

      // Step 3: EMA smoothing
      ref1 = HYBRID_REF_SMOOTH_ALPHA * ref1 +
             (1.0F - HYBRID_REF_SMOOTH_ALPHA) * hybrid_ref1_prev;
      ref2 = HYBRID_REF_SMOOTH_ALPHA * ref2 +
             (1.0F - HYBRID_REF_SMOOTH_ALPHA) * hybrid_ref2_prev;
      hybrid_ref1_prev = ref1;
      hybrid_ref2_prev = ref2;
      if (HYBRID_FORCE_NEUTRAL_REF) {
        ref1 = 0.0F;
        ref2 = 0.0F;
      }

      // Step 4: encode and transmit
      const uint8_t ref1b = encodeHybridRef(ref1, HYBRID_REF_MAX_1);
      const uint8_t ref2b = encodeHybridRef(ref2, HYBRID_REF_MAX_2);
      buffer_write[0] = FLAG_CONTROL;
      buffer_write[1] = ref1b;
      buffer_write[2] = ref2b;
      serial.writeBytes(buffer_write, PACKET_WRITE_SIZE);

      // Decode back to radians for logging (round-trip through byte encoding).
      // s.out1/out2 already hold the raw K*x voltage.
      s.ref1 = decodeHybridRef(ref1b, HYBRID_REF_MAX_1);
      s.ref2 = decodeHybridRef(ref2b, HYBRID_REF_MAX_2);
    }

    data.push_back(s);
  }

  // Send STOP at end (or after early break).
  buffer_write[0] = FLAG_STARTSTOP;
  buffer_write[1] = 1;
  buffer_write[2] = 1;
  serial.writeBytes(buffer_write, PACKET_WRITE_SIZE);

  return data;
}

int main(int argc, char *argv[]) {
  std::signal(SIGINT, sigint_handler);

  const int len = askSimulationLength();

  serialib serial;
  if (!openConnection(serial))
    return 1;

  std::string stop_reason;
  const std::vector<Sample> data = runControlLoop(serial, len, stop_reason);

  sleep(1);
  serial.closeDevice();
  std::cout << "Done! Stop reason: " << stop_reason << "\n";
  std::cout << "Collected " << data.size() << " samples.\n";

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
    char timestamp[32];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S",
                  std::localtime(&t));
    filepath =
        (data_dir / (std::string("output_") + timestamp + ".csv")).string();
  }

  saveToFile(data, filepath, stop_reason);
  std::cout << "Data saved to: " << filepath << "\n";
  return 0;
}
