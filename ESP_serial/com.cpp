// com.cpp
#include "FlexBot.h"

const uint8_t flag_startstop = uint8_t(120);
const uint8_t flag_control = uint8_t(99);
const uint8_t flag_send = uint8_t(109);

void transmit_measurement(float pos1, float pos2, float strain1_filtered, float strain2_filtered, float strain1div_filtered, float strain2div_filtered, int time, float test) {
  // Send Data
  // Send data in the form:
  // @pa<number>pb<number>sa<number>sb<number>sad<number>sbd<number>
  // Serial.printf("m%.2f %.2f %.4f %.4f %.4f %.4f %d %.2f\n", pos1, pos2, strain1_filtered, strain2_filtered, strain1div_filtered, strain2div_filtered, time, test);
  int16_t buffer_pos1 = int16_t(pos1*100);
  int16_t buffer_pos2 = int16_t(pos2*100);
  int16_t buffer_strain1 = int16_t(strain1_filtered*1000);
  int16_t buffer_strain2 = int16_t(strain2_filtered*1000);
  int16_t buffer_strain1div = int16_t(strain1div_filtered*100);
  int16_t buffer_strain2div = int16_t(strain2div_filtered*100);
  int16_t buffer_time = int16_t(time/10);
  
  byte buffer[16] = {flag_send, 0,
                     lowByte(int16_t(buffer_pos1)), highByte(int16_t(buffer_pos1)),
                     lowByte(int16_t(buffer_pos2)), highByte(int16_t(buffer_pos2)),
                     lowByte(int16_t(buffer_strain1)), highByte(int16_t(buffer_strain1)),
                     lowByte(int16_t(buffer_strain2)), highByte(int16_t(buffer_strain2)),
                     lowByte(int16_t(buffer_strain1div)), highByte(int16_t(buffer_strain1div)),
                     lowByte(int16_t(buffer_strain2div)), highByte(int16_t(buffer_strain2div)),
                     lowByte(int16_t(buffer_time)), highByte(int16_t(buffer_time))
                    };
  Serial.write(buffer,16);
}

void receive_control(float &out1_buffer, float &out2_buffer, bool &stop) {
  // Convert string to the following float values:
  // oa<number>ob<number>x<bool>
  // e.g.    a1.23b1.23x1
  // Number behind oa is for out1, Number behind ob is for out2
  char data_received[3];

  Serial.readBytes(data_received,3);

  if (uint8_t(data_received[0]) == flag_startstop) {
    stop = (uint8_t(data_received[1]) != 0);
    if(enable_debug){
      Serial.println("Received start/stop");
    }
  } 
  else if (uint8_t(data_received[0]) == flag_control && !stop){
    // out1_buffer = float(3.3/256*uint8_t(data_received[1]) - 3.3/256*127);
    // out2_buffer = float(3.3/256*uint8_t(data_received[2]) - 3.3/256*125);
    out1_buffer = float(uint8_t(data_received[1]));
    out2_buffer = float(uint8_t(data_received[2]));
    if(enable_debug){
      Serial.println("Received controls");
    }
  }
  else {
    error_case(out1_buffer, out2_buffer, stop);
  }
}

// void receive_control(float &out1_buffer, float &out2_buffer, bool &stop) {
//   // Convert string to the following float values:
//   // oa<number>ob<number>x<bool>
//   // e.g.    a1.23b1.23x1
//   // Number behind oa is for out1, Number behind ob is for out2
//   String received_string = Serial.readStringUntil('\n');

//   uint16_t idx_a = received_string.indexOf("a");
//   uint16_t idx_b = received_string.indexOf("b");
//   uint16_t idx_end = received_string.length();

//   if (received_string.substring(0, 1) == String("x")) {
//     stop = (received_string.substring(1, idx_end) != String("0"));
//     if(enable_debug){
//       Serial.println("Received start/stop");
//     }
//   } 
//   else if (received_string.substring(0, 1) == String("c") && !stop){
//     if(idx_a != 65535 && idx_b != 65535){
//       out1_buffer = received_string.substring(idx_a + 1, idx_b).toFloat();
//       out2_buffer = received_string.substring(idx_b + 1, idx_end).toFloat();
//       if(enable_debug){
//         Serial.println("Received controls");
//       }
//     } else {
//       error_case(out1_buffer, out2_buffer, stop);
//     }
//   }
//   else {
//     error_case(out1_buffer, out2_buffer, stop);
//   }
// }

void error_case (float &out1_buffer, float &out2_buffer, bool &stop) {
    out1_buffer = 127;
    out2_buffer = 125;
    stop = true;
    if(enable_debug){
      Serial.println("Error");
    }    
}


