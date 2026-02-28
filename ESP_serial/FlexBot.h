// FlexBot.h
#pragma once
#include <Arduino.h>
#include "esp_timer.h"


// header: ESP_FlexBot.ino
extern const int CYCLE_TIME_MS;         // ms
extern const float max_velocity;        // m/s
extern const float max_steering_angle;  // rad
extern bool stop;                       // for hall sensor
extern bool enable_debug;               // for printing messages


// header: com.cpp
void transmit_measurement(float, float, float, float, float, float, int, float);
void receive_control(float &, float &, bool &);
void error_case(float &, float &, bool &);


// header: encoder.cpp
void setup_enc();
void get_pos_enc(float &, float &);
void reset_pos_enc(float &, float &);

// header: analog.cpp
void setup_analog();
void get_strain(float &, float &);
void write_control(float &, float &);

// header: hall.cpp
void setup_hall();

// header: filter.cpp
void lowpass(float &, float &, float &, float &, float &, float &);

// header: control.cpp
float controller1(float, float, float);
float controller2(float, float, float);
