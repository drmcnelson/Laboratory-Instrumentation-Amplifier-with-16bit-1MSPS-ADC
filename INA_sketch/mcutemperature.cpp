/*
  Author:   Mitchell C. Nelson
  Date:     June 1, 2024
  Contact:  drmcnelsonlab@gmail.com

 */

#include "Arduino.h"

#include "mcutemperature.h"

extern float tempmonGetTemp(void);

float getMCUTemperature() {
  return tempmonGetTemp();
}

void sendMCUTemperature( ){
  Serial.print( "MCUTEMPERATURE " );
  Serial.println( tempmonGetTemp() );
}
