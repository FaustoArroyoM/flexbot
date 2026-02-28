// ESP32-DEVKITC V4 for XXX
// May 2024 by Johannes Teutsch

// Dependencies:
// - Add ESP32 board
// - Install Libary EspSoftware Serial by Dirk Kaar and Peter Lerup

#include "FlexBot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

const int CYCLE_TIME_MS = 1;
const int COMM_TIME_MS = 5;
const int DEBUG_TIME_MS = 100000;
int computation_time;
int communication_time;
float load;
int deadtime = 0;
bool stop = 1;
bool enable_debug = false;
bool enable_transmit = false;

float pos1 = 0;
float pos2 = 0;

float strain1 = 0;
float strain2 = 0;

float strain1_filtered = 0;
float strain2_filtered = 0;
float strain1div_filtered = 0;
float strain2div_filtered = 0;

float out1 = 127;
float out2 = 125;
float out1_buffer = 127;
float out2_buffer = 125;

int64_t init_time = 0;
int64_t end_time = 0;
// const int64_t comm_max_us = int64_t(900*COMM_TIME_MS);


void setup() {
  // Start comunication with the NUC
  Serial.begin(115200); //115200, 921600 
  // Setup
  setup_enc();
  setup_hall();
  setup_analog();

  TaskHandle_t cycle_h;
  TaskHandle_t serialcomm_h;
  TaskHandle_t debug_msg_h;
  xTaskCreatePinnedToCore(cycle, "Cycle", 10000, NULL, 1, &cycle_h, 0);
  xTaskCreatePinnedToCore(serialcomm, "Serial Communication", 10000, NULL, 2, &serialcomm_h, 0);
  xTaskCreatePinnedToCore(debug_msg, "Debug Message", 10000, NULL, 3, &debug_msg_h, 0);
}

void loop() {
}


void cycle(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  while (1) {
    // Start of Time measurement
    int64_t time_start = esp_timer_get_time();

    //Check Hall sensors: stop control in case of unsafe movement
    if(stop){
      out1 = 127;
      out2 = 125;
      out1_buffer = 127;
      out2_buffer = 125;
      reset_pos_enc(pos1, pos2);
    }
    else{
      // low-level control:
      // out1 = controller1(pos1, strain1_filtered, strain1div_filtered);
      // out2 = controller2(pos2, strain2_filtered, strain2div_filtered);
      out1 = out1_buffer;
      out2 = out2_buffer;

    }
    // Apply control
    write_control(out1, out2);

    // Measurement
    get_pos_enc(pos1, pos2);
    get_strain(strain1, strain2);

    // Filtering
    lowpass(strain1, strain2, strain1_filtered, strain2_filtered, strain1div_filtered, strain2div_filtered);

    
    // End of Time measurement
    computation_time = esp_timer_get_time() - time_start;
    load = computation_time / float(CYCLE_TIME_MS) / 10.0;

    // variable xLastWakeTime is updated internally
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(CYCLE_TIME_MS));
  }
}

void serialcomm(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  while (1) {
    // Start of Time measurement
    int64_t time_start = esp_timer_get_time();
    bool flag_receive = true;

    // Communication via serial port
    if(enable_transmit && !stop){
      // Transmit measurements
      init_time = esp_timer_get_time();
      transmit_measurement(pos1, pos2, strain1_filtered, strain2_filtered, strain1div_filtered, strain2div_filtered, int(end_time), out1_buffer);
      enable_transmit = false;
    }

    if (Serial.available()) {
      // Receive control inputs from serial connection
      receive_control(out1_buffer, out2_buffer, stop);
      end_time = esp_timer_get_time() - init_time;
      enable_transmit = true;
    }

    // while(~Serial.available()) {
    //   if(esp_timer_get_time() - time_start > comm_max_us){
    //     flag_receive = false;
    //   }
    // }

    // if(flag_receive){
    //   // Receive control inputs from serial connection
    //   receive_control(out1_buffer, out2_buffer, stop);
    //   end_time = esp_timer_get_time() - init_time;
    //   enable_transmit = true;
    // }

    // // Communication via serial port
    // if (Serial.available()) {
    //   // Receive control inputs from serial connection
    //   receive_control(out1_buffer, out2_buffer, stop);
    //   end_time = esp_timer_get_time() - init_time;

    // if(!stop){
    //   // Transmit measurements
    //   transmit_measurement(pos1, pos2, strain1_filtered, strain2_filtered, strain1div_filtered, strain2div_filtered, int(end_time), out1_buffer);
    //   init_time = esp_timer_get_time();
    // }

    // }

    // End of Time measurement
    communication_time = esp_timer_get_time() - time_start;
    
    // variable xLastWakeTime is updated internally
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(COMM_TIME_MS));
  }
}

void debug_msg(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  while (1) {
    if(enable_debug){
      Serial.println("Start Debug Msg");
      Serial.print("Computation time in µs: ");
      Serial.println(computation_time);
      Serial.print("Communication time in µs: ");
      Serial.println(communication_time);
      Serial.print("Load ");
      Serial.println(load); 
      Serial.print("Pos1 ");
      Serial.println(pos1);
      Serial.print("Pos2 ");
      Serial.println(pos2);
      Serial.print("Strain1 ");
      Serial.println(strain1);
      Serial.print("Strain2 ");
      Serial.println(strain2);
      Serial.print("Strain1 filtered ");
      Serial.println(strain1_filtered);
      Serial.print("Strain2 filtered ");
      Serial.println(strain2_filtered);
      // Serial.print("Strain1div filtered ");
      // Serial.println(strain1div_filtered);
      // Serial.print("Strain2div filtered ");
      // Serial.println(strain2div_filtered);
      Serial.print("Out1 ");
      Serial.println(out1);
      Serial.print("Out2 ");
      Serial.println(out2);
      Serial.print("Stop: ");
      Serial.println(stop);
      Serial.println("End Debug Msg");
    }
    // variable xLastWakeTime is updated internally
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(DEBUG_TIME_MS));
  }
}
