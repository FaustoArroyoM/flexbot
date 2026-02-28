// hall.cpp
// Interrupts for hall sensors (angle limitations)
#include "FlexBot.h"

const int PIN_HALL0 = 34;  // Hall sensor for angle limitation (elbow)
const int PIN_HALL1 = 35;  // Hall sensor for angle limitation (elbow)
const int PIN_HALL2 = 36;  // Hall sensor for angle limitation (shoulder)
const int PIN_HALL3 = 39;  // Hall sensor for angle limitation (shoulder)

void IRAM_ATTR ISR_HALL() {  // Interupt Service Routine
  stop = true;
}


void setup_hall() {  // setup nessesary for Function is_auto_mode()
  pinMode(PIN_HALL0, INPUT);
  pinMode(PIN_HALL1, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_HALL0), ISR_HALL, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_HALL1), ISR_HALL, FALLING);
  
  // NOT WORKING:
  // pinMode(PIN_HALL2, INPUT);
  // pinMode(PIN_HALL3, INPUT);
  // attachInterrupt(digitalPinToInterrupt(PIN_HALL2), ISR_HALL, FALLING);
  // attachInterrupt(digitalPinToInterrupt(PIN_HALL3), ISR_HALL, FALLING);
}

