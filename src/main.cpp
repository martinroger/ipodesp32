#include <Arduino.h>

#include "AudioTools.h"
#include "BluetoothA2DPSink.h"
#include "ESP32AudioProvider.h"
#include "esPod.h"

#pragma region Board IO Macros
// LED Logic inversion
#ifndef INVERT_LED_LOGIC
#define INVERT_LED_LOGIC(stateBoolean) stateBoolean
#else
#undef INVERT_LED_LOGIC
#define INVERT_LED_LOGIC(stateBoolean) !stateBoolean
#endif

// DCD Logic inversion
#ifndef INVERT_DCD_LOGIC
#define INVERT_DCD_LOGIC(stateBoolean) stateBoolean
#else
#undef INVERT_DCD_LOGIC
#define INVERT_DCD_LOGIC(stateBoolean) !stateBoolean
#endif

// DCD control pin to pretend there is a physical disconnect
#if defined(ENABLE_ACTIVE_DCD) && !defined(DCD_CTRL_PIN)
#define DCD_CTRL_PIN 5
#endif
#pragma endregion

#pragma region A2DP Sink Configuration and Serial Initialization

#ifdef AUDIOKIT

#ifdef USE_SD
#include "sdLogUpdate.h"
bool sdLoggerEnabled = false;
#endif

#include "AudioBoard.h"
#include "AudioTools/AudioLibs/I2SCodecStream.h"
AudioInfo info(44100, 2, 16);
DriverPins minimalPins;
AudioBoard minimalAudioKit(AudioDriverES8388, minimalPins);
I2SCodecStream i2s(minimalAudioKit);
BluetoothA2DPSink a2dp_sink(i2s);

#ifdef USE_ALT_SERIAL
HardwareSerial ipodSerial(1);

#ifndef UART1_RX
#define UART1_RX 19
#endif

#ifndef UART1_TX
#define UART1_TX 22
#endif

#else // Use main Serial
HardwareSerial ipodSerial(0);
#endif

#else // Case not using the audiokit, like Sandwich Carrier Board
#define USE_SERIAL_1

#ifndef UART1_RX
#define UART1_RX 16
#endif

#ifndef UART1_TX
#define UART1_TX 17
#endif

#ifndef UART1_RST
#define UART1_RST 13
#endif

HardwareSerial ipodSerial(1);

#endif

ESP32AudioProvider audioProvider;
esPod espod(ipodSerial, audioProvider);

#pragma endregion

#pragma region FreeRTOS tasks defines
#ifndef AVRC_QUEUE_SIZE
#define AVRC_QUEUE_SIZE 32
#endif
#ifndef PROCESS_AVRC_TASK_STACK_SIZE
#define PROCESS_AVRC_TASK_STACK_SIZE 4096
#endif
#ifndef PROCESS_AVRC_TASK_PRIORITY
#define PROCESS_AVRC_TASK_PRIORITY 6
#endif
#ifndef AVRC_INTERVAL_MS
#define AVRC_INTERVAL_MS 5
#endif
#pragma endregion

#pragma region Helper Functions declaration
void initializeSDCard();
void initializeSerial();
#pragma endregion

void setup()
{
// If available, reset the UART1 transceiver
#ifdef UART1_RST
	pinMode(UART1_RST, OUTPUT);
	digitalWrite(UART1_RST, LOW);
#endif

#ifdef LED_BUILTIN
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, INVERT_LED_LOGIC(LOW));
#endif

#ifdef ENABLE_ACTIVE_DCD
	pinMode(DCD_CTRL_PIN, OUTPUT);
	digitalWrite(DCD_CTRL_PIN, INVERT_DCD_LOGIC(HIGH)); // Logic is inverted
#endif

	esp_log_level_set("*", ESP_LOG_NONE);
	ESP_LOGI("SETUP", "setup() start");

	initializeSDCard();

	// Inform of possible errors that led to a reset
	ESP_LOGI("RESET", "Reset reason: %d", esp_reset_reason());
	delay(5);

	// Publish build information
	ESP_LOGI("BUILD_INFO", "env:%s\t date: %s\t time: %s", PIOENV, __DATE__, __TIME__);
	delay(5);
	ESP_LOGI("VERSION", "%s", VERSION_STRING);
	delay(5);
	ESP_LOGI("BRANCH", "%s", VERSION_BRANCH);
	delay(5);

	if (audioProvider.start() != ESP_OK)
		esp_restart();
#ifdef UART1_RST // Re-enable the UART1 transceiver if available
	digitalWrite(UART1_RST, HIGH);
#endif
	initializeSerial();
	ESP_LOGI("SETUP", "Waiting for peer");
	while (!audioProvider.is_connected())
	{
		delay(10);
	}
	delay(50);
	ESP_LOGI("SETUP", "Peer connected: %s", audioProvider.get_device_name());
	ESP_LOGI("SETUP", "Setup finished");
}

void loop()
{
}

#pragma region AVRC Task and Queue declaration/definition
// Metadata universal structure
struct avrcMetadata
{
	uint8_t id = 0;
	uint8_t *payload = nullptr;
};

#pragma region Helper Function Definitions
/// @brief Attempts to initialize the SD card if present and enabled
void initializeSDCard()
{
#ifdef USE_SD
	if (digitalRead(SD_DETECT) == LOW)
	{
		if (initSD())
		{
#ifdef LOG_TO_SD
			sdLoggerEnabled = initSDLogger();
			if (sdLoggerEnabled)
				esp_log_level_set("*", ESP_LOG_INFO);
#endif
			updateFromFS(SD_MMC);
		}
	}
#endif
}

/// @brief Sets up and starts the appropriate Serial interface
void initializeSerial()
{
#ifndef IPOD_SERIAL_BAUDRATE
#define IPOD_SERIAL_BAUDRATE 19200
#endif
#if defined(USE_SERIAL_1) || defined(USE_ALT_SERIAL) // If Alt Serial or Serial 1 is used
	ipodSerial.setPins(UART1_RX, UART1_TX);
#endif
	ipodSerial.setRxBufferSize(1024);
	ipodSerial.setTxBufferSize(1024);
	ipodSerial.begin(IPOD_SERIAL_BAUDRATE);
}
#pragma endregion
