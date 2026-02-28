// analog.cpp
// Read and write analog inputs/outputs
#include "FlexBot.h"

const uint8_t PIN_STRAIN1 = A14;  // Analog input pin for strain guage 1
const uint8_t PIN_STRAIN2 = A10;  // Analog input pin for strain guage 2

const float AIoffset1 = -0.05; // DC offset in analog input 1
const float AIoffset2 = 0.13; // DC offset in analog input 2

const int AOoffset1 = 0; // DC offset in analog output 1
const int AOoffset2 = 2; // DC offset in analog output 2

int val1 = 127;
int val2 = 127;

void setup_analog() {
  pinMode(PIN_STRAIN1, INPUT);
  pinMode(PIN_STRAIN2, INPUT);
  analogReadResolution(12); 
  analogSetPinAttenuation(PIN_STRAIN1, ADC_11db); 
  analogSetPinAttenuation(PIN_STRAIN2, ADC_11db); 

}

void get_strain(float &strain1, float &strain2) {
  strain1 = 20.0/4096.0*float(analogRead(PIN_STRAIN1)) - 10.0 - AIoffset1;
  strain2 = 20.0/4096.0*float(analogRead(PIN_STRAIN2)) - 10.0 - AIoffset2;
}

void write_control(float &out1, float &out2) {
  // int val1 = int(256/3.3*out1 + 127 - AOoffset1);
  int val1 = int(out1);
  if(val1 < 0){val1 = 0;}
  if(val1 > 255){val1 = 255;}
  // val1 = 127;
  dacWrite(DAC1, uint8_t(val1));

  // elbow:
  // if(out2 < -0.6){out2 = -0.6;}
  // if(out2 > 0.6){out2 = 0.6;}
  int val2 = int(out2);
  if(val2 < 78){val2 = 78;}
  if(val2 > 171){val2 = 171;}
  // val2 = 125;
  dacWrite(DAC2, uint8_t(val2));
}

