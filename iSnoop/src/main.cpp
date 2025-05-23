#include <Arduino.h>

#include "snooper.h"

HardwareSerial RXUart_1(1);
HardwareSerial RXUart_2(2);

#ifndef UART1_RX
#define UART1_RX 18
#endif

#ifndef UART1_TX
#define UART1_TX 19
#endif

#ifndef UART2_RX
#define UART2_RX 16
#endif

#ifndef UART2_TX
#define UART2_TX 17
#endif

// snooper espod(ipodSerial);
snooper UART1(RXUart_1, "UART1");
snooper UART2(RXUart_2, "UART2");

#ifndef IPOD_DETECT
#define IPOD_DETECT 4
#endif


#pragma region Helper Functions declaration

void initializeSerial();
#pragma endregion


void setup()
{
	initializeSerial();
	UART1.detectPin = IPOD_DETECT;
	UART2.detectPin = IPOD_DETECT;
	ESP_LOGI("SETUP", "Setup finished");
}

void loop()
{
}


/// @brief Sets up and starts the appropriate Serial interface
void initializeSerial()
{
	Serial.begin(115200);
	RXUart_1.setPins(UART1_RX, UART1_TX);
	RXUart_2.setPins(UART2_RX, UART2_TX);
	RXUart_1.setRxBufferSize(1024);
	RXUart_1.setTxBufferSize(1024);
	RXUart_2.setRxBufferSize(1024);
	RXUart_2.setTxBufferSize(1024);
	RXUart_1.begin(19200);
	RXUart_2.begin(19200);
}
