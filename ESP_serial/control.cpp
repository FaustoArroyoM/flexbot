// control.cpp
// PI and PD control
#include "FlexBot.h"

// sampling time
const float T = CYCLE_TIME_MS/1000;

// storage of past values for integration:
volatile float pos1_int = 0;  // filter input u1[k-1]
volatile float pos2_int = 0;  // filter input u1[k-2]

// controller parameter:
const float K_pos1 = 2;
const float K_posint1 = 0.1;
const float K_strain1 = -0.1;
const float K_straindiv1 = -0.01;
const float K_pos2 = 3;
const float K_posint2 = 0.1;
const float K_strain2 = -0.05;
const float K_straindiv2 = -0.005;

float controller1(float pos1, float strain1_filtered, float strain1div_filtered) {
  float out1;
  out1 = K_pos1*pos1 + K_posint1*pos1_int + K_strain1*strain1_filtered + K_straindiv1*strain1_filtered;
  out1 = -out1;

  pos1_int += T*pos1;
  if(pos1_int > 10){
    pos1_int = 10;
  }
  if(pos1_int < -10){
    pos1_int = -10;
  }

  return out1;
}

float controller2(float pos2, float strain2_filtered, float strain2div_filtered) {
  float out2;
  out2 = K_pos2*pos2 + K_posint2*pos2_int + K_strain2*strain2_filtered + K_straindiv2*strain2_filtered;
  out2 = -out2;

  pos2_int += T*pos2;
  if(pos2_int > 10){
    pos2_int = 10;
  }
  if(pos2_int < -10){
    pos2_int = -10;
  }

  return out2;
}




