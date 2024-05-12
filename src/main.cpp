#include <Arduino.h>

#include "esPod.h"

//int validChecksums = 0;
//uint32_t packetCounter = 0;
esPod espod(USBSerial);


void setup() {
  USBSerial.begin(19200);
  USBSerial.setRxBufferSize(4096);
  #ifdef DEBUG_MODE
  Serial.setTxBufferSize(4096);
  Serial.begin(115200);
  #endif
  //Flush the RX
  while(Serial.available()) Serial.read();
  while(USBSerial.available()) USBSerial.read();
}

void loop() {
  espod.refresh();

}
