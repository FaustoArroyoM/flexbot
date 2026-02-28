// filter.cpp
// 2nd order low pass filters for analog input signals, discretized via bilinear transform
#include "FlexBot.h"

// constant for bilinear transform
const float K = 2/CYCLE_TIME_MS*1000;

// storage for past values of lowpass filter variables:
volatile float u1m1 = 0;  // filter input u1[k-1]
volatile float u1m2 = 0;  // filter input u1[k-2]
volatile float y1m1 = 0;  // filter input y1[k-1]
volatile float y1m2 = 0;  // filter input y1[k-2]

volatile float u2m1 = 0;  // filter input u2[k-1]
volatile float u2m2 = 0;  // filter input u2[k-2]
volatile float y2m1 = 0;  // filter input y2[k-1]
volatile float y2m2 = 0;  // filter input y2[k-2]

// lowpass filter parameters:
const float b = 2*PI*15 * 2*PI*15;
const float a = 2 * 0.9 * 2*PI*15;

// storage for past values of derivative lowpass filter variables:
volatile float y1m1d = 0;  // filter input y1[k-1]
volatile float y1m2d = 0;  // filter input y1[k-2]
volatile float y2m1d = 0;  // filter input y2[k-1]
volatile float y2m2d = 0;  // filter input y2[k-2]

// derivative lowpass filter parameters:
const float bd = 0.2*2*PI*15 * 0.2*2*PI*15;
const float ad = 2 * 0.9 * 0.2*2*PI*10;


void lowpass(float &strain1, float &strain2, float &strain1_filtered, float &strain2_filtered, float &strain1div_filtered, float &strain2div_filtered) {
  // filter equation based on bilinear transform:
  strain1_filtered = (b*strain1 + 2*b*u1m1 + b*u1m2 - (2*b - 2*K*K)*y1m1 - (K*K - a*K + b)*y1m2)/(K*K + a*K + b);
  strain2_filtered = (b*strain2 + 2*b*u2m1 + b*u2m2 - (2*b - 2*K*K)*y2m1 - (K*K - a*K + b)*y2m2)/(K*K + a*K + b);
  
  strain1div_filtered = (bd*K*strain1 - bd*K*u1m2 - (2*bd - 2*K*K)*y1m1d - (K*K - ad*K + bd)*y1m2d)/(K*K + ad*K + bd);
  strain2div_filtered = (bd*K*strain2 - bd*K*u2m2 - (2*bd - 2*K*K)*y2m1d - (K*K - ad*K + bd)*y2m2d)/(K*K + ad*K + bd);

  // update storage:
  u1m2 = u1m1;
  u1m1 = strain1;
  y1m2 = y1m1;
  y1m1 = strain1_filtered;
  u2m2 = u2m1;
  u2m1 = strain2;
  y2m2 = y2m1;
  y2m1 = strain2_filtered;
  y1m2d = y2m1d;
  y1m1d = strain2div_filtered;
  y2m2d = y2m1d;
  y2m1d = strain2div_filtered;
}



