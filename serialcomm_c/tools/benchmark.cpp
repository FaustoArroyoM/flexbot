// tools/benchmark.cpp
// Measures USB serial roundtrip latency (RTT) over N echo cycles.
// Requires the ESP32 to be running echo_firmware (see Step 2).
// Usage: ./benchmark [/dev/ttyUSB0] [baud]

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <linux/serial.h>
#include <numeric>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <vector>

// Add this function before main()
static void setLowLatency(int fd) {
  struct serial_struct ser {};
  if (ioctl(fd, TIOCGSERIAL, &ser) < 0) {
    perror("TIOCGSERIAL");
    return;
  }
  ser.flags |= ASYNC_LOW_LATENCY; // 0x2000
  if (ioctl(fd, TIOCSSERIAL, &ser) < 0) {
    perror("TIOCSSERIAL");
    return;
  }
  printf("ASYNC_LOW_LATENCY: set\n");
}

// We use POSIX termios directly here (not serialib) so we can set
// ASYNC_LOW_LATENCY and inspect termios flags without touching serialib.

static int openPort(const char *path, int baud_code) {
  int fd = open(path, O_RDWR | O_NOCTTY | O_SYNC);
  if (fd < 0) {
    perror("open");
    return -1;
  }

  struct termios tty {};
  tcgetattr(fd, &tty);
  cfsetispeed(&tty, baud_code);
  cfsetospeed(&tty, baud_code);
  cfmakeraw(&tty);     // no buffering, no line discipline
  tty.c_cc[VMIN] = 1;  // block until 1 byte received
  tty.c_cc[VTIME] = 0; // no timeout (blocking read)
  tcsetattr(fd, TCSANOW, &tty);
  tcflush(fd, TCIOFLUSH);
  return fd;
}

int main(int argc, char *argv[]) {
  const char *port = (argc > 1) ? argv[1] : "/dev/ttyUSB0";
  int baud_code =
      (argc > 2 && std::strcmp(argv[2], "921600") == 0) ? B921600 : B115200;
  int baud_val = (baud_code == B921600) ? 921600 : 115200;

  int fd = openPort(port, baud_code);
  if (fd < 0)
    return 1;
  setLowLatency(fd);

  constexpr int WARMUP = 20;
  constexpr int N = 500;

  printf("Port: %s | Baud: %d | Warmup: %d | Samples: %d\n", port, baud_val,
         WARMUP, N);

  std::vector<double> rtt_us;
  rtt_us.reserve(N);

  uint8_t tx = 0xAB, rx = 0;

  // Warmup: let the driver/chip settle, discard results
  for (int i = 0; i < WARMUP; ++i) {
    write(fd, &tx, 1);
    read(fd, &rx, 1);
  }

  for (int i = 0; i < N; ++i) {
    auto t0 = std::chrono::steady_clock::now();
    write(fd, &tx, 1);
    read(fd, &rx, 1);
    auto t1 = std::chrono::steady_clock::now();
    rtt_us.push_back(
        std::chrono::duration<double, std::micro>(t1 - t0).count());
  }

  close(fd);

  // Statistics
  std::sort(rtt_us.begin(), rtt_us.end());
  double sum = std::accumulate(rtt_us.begin(), rtt_us.end(), 0.0);
  double mean = sum / N;
  double p50 = rtt_us[N * 50 / 100];
  double p95 = rtt_us[N * 95 / 100];
  double p99 = rtt_us[N * 99 / 100];
  double maxv = rtt_us.back();

  printf("--- RTT Results ---\n");
  printf("Mean : %7.1f µs\n", mean);
  printf("p50  : %7.1f µs\n", p50);
  printf("p95  : %7.1f µs\n", p95);
  printf("p99  : %7.1f µs\n", p99);
  printf("Max  : %7.1f µs\n", maxv);

  return 0;
}
