// encoder.cpp
// Read the velocity of the car with the encoders
#include "FlexBot.h"

const int PIN_ENC1_A = 32;  // Pin A of the first encoder
const int PIN_ENC1_B = 33;  // Pin B of the first encoder
const int PIN_ENC1_Z = 27;  // Reset Pin of the first encoder
const int PIN_ENC2_A = 22;  // Pin A of the second encoder
const int PIN_ENC2_B = 17;  // Pin B of the second encoder
const int PIN_ENC2_Z = 16;  // Reset Pin of the second encoder

const float RESOLUTION1 = 50600;
const float RESOLUTION2 = 25850;

volatile int32_t cnt1 = 0;
volatile int32_t cnt2 = 0;


void IRAM_ATTR ISR_ENC1() {  // Interupt Service Routine
  cnt1 += 2 * digitalRead(PIN_ENC1_B) - 1;
}

void IRAM_ATTR ISR_ENC2() {  // Interupt Service Routine
  cnt2 += 2 * digitalRead(PIN_ENC2_B) - 1;
}

void IRAM_ATTR ISR_ENC1_reset() {  // Interupt Service Routine
  cnt1 = 0;
}

void IRAM_ATTR ISR_ENC2_reset() {  // Interupt Service Routine
  cnt2 = 0;
}

void setup_enc() {  // setup nessesary for Function is_auto_mode()
  pinMode(PIN_ENC1_A, INPUT);
  pinMode(PIN_ENC1_B, INPUT);
  pinMode(PIN_ENC2_A, INPUT);
  pinMode(PIN_ENC2_B, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC1_A), ISR_ENC1, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC2_A), ISR_ENC2, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC1_Z), ISR_ENC1_reset, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC2_Z), ISR_ENC2_reset, RISING);
}


void get_pos_enc(float &pos1, float &pos2) {
  pos1 = float(cnt1 / RESOLUTION1 * PI );
  pos2 = float(cnt2 / RESOLUTION2 * PI);
}


void reset_pos_enc(float &pos1, float &pos2) {
  pos1 = 0;
  pos2 = 0;
  cnt1 = 0;
  cnt2 = 0;
}