#include <Arduino.h>
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"
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

#pragma region AVRC-related FreeRTOS tasks defines
#ifndef AVRC_QUEUE_SIZE
#define AVRC_QUEUE_SIZE 32
#endif
#ifndef PROCESS_AVRC_TASK_STACK_SIZE
#define PROCESS_AVRC_TASK_STACK_SIZE 4096
#endif
#ifndef PROCESS_AVRC_TASK_PRIORITY
#define PROCESS_AVRC_TASK_PRIORITY 6
#endif
#pragma endregion

#pragma region A2DP Sink Configuration
// A2DP instance name
#ifndef A2DP_SINK_NAME
#define A2DP_SINK_NAME "espiPod"
#endif
#ifndef WS_PIN
#define WS_PIN 25
#endif
#ifndef DIN_PIN
#define DIN_PIN 26
#endif
#ifndef BCLK_PIN
#define BCLK_PIN 27
#endif

#ifdef AUDIOKIT // Using the AiThink A1S AudioKit chip
#include "AudioBoard.h"
#include "AudioTools/AudioLibs/I2SCodecStream.h"
AudioInfo info(44100, 2, 16);
DriverPins minimalPins;
AudioBoard minimalAudioKit(AudioDriverES8388, minimalPins);
I2SCodecStream i2s(minimalAudioKit);
BluetoothA2DPSink a2dp_sink(i2s);
#else // Case not using the audiokit, like Sandwich Carrier Board
I2SStream i2s;
BluetoothA2DPSink a2dp_sink;
#endif

/// @brief Data stream reader callback
/// @param data Data buffer to pass to the I2S
/// @param length Length of the data buffer
void read_data_stream(const uint8_t *data, uint32_t length)
{
	i2s.write(data, length);
}

#pragma endregion

#pragma region Helper Functions declaration
void initializeA2DPSink();
esp_err_t initializeAVRCTask();
#pragma endregion

#pragma region A2DP/AVRC callbacks declaration
void connectionStateChanged(esp_a2d_connection_state_t state, void *ptr);
void audioStateChanged(esp_a2d_audio_state_t state, void *ptr);
void avrc_rn_play_pos_callback(uint32_t play_pos);
void avrc_metadata_callback(uint8_t id, const uint8_t *text);
void playStatusHandler(PB_COMMAND playCommand);
#pragma endregion

// Declare the principal object... multiple syntaxes are available
// Autobaud syntax example
// esPod espod(1,UART1_RX,UART1_TX,0);
esPod espod(1, UART1_RX, UART1_TX, 19200);
bool pendingPlayReq = false; // Might use this to make sure play requests are not ignored.

void setup()
{

#ifdef LED_BUILTIN
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, INVERT_LED_LOGIC(LOW));
#endif

#ifdef ENABLE_ACTIVE_DCD
	pinMode(DCD_CTRL_PIN, OUTPUT);
	digitalWrite(DCD_CTRL_PIN, INVERT_DCD_LOGIC(HIGH)); // Logic is inverted
#endif

	ESP_LOGI(__func__, "setup() start");

	// Inform of possible errors that led to a reset
	ESP_LOGI("RESET", "Reset reason: %d", esp_reset_reason());

	// Publish build information
	ESP_LOGI("BUILD_INFO", "env:%s\t date: %s\t time: %s", PIOENV, __DATE__, __TIME__);
	ESP_LOGI("VERSION", "%s", VERSION_STRING);
	ESP_LOGI("BRANCH", "%s", VERSION_BRANCH);

	// Start AVRC Notifications handler
	if (initializeAVRCTask() != ESP_OK)
		esp_restart();

	// Start the A2DP Sink
	initializeA2DPSink();

	espod.attachPlayControlHandler(playStatusHandler);
	ESP_LOGI(__func__, "Waiting for peer");
	while (a2dp_sink.get_connection_state() != ESP_A2D_CONNECTION_STATE_CONNECTED)
	{
		delay(10);
	}
	delay(50);
	ESP_LOGI(__func__, "Peer connected: %s", a2dp_sink.get_peer_name());
	ESP_LOGI(__func__, "Setup finished");
}

void loop()
{
	vTaskDelay(1); // Purely out of precaution
}

#pragma region AVRC Task and Queue declaration/definition
// Metadata universal structure
struct avrcMetadata
{
	uint8_t id = 0;
	uint8_t *payload = nullptr;
};

// AVRC Queue and Task
QueueHandle_t avrcMetadataQueue;
TaskHandle_t processAVRCTaskHandle;

/// @brief Low priority task to process a queue of received metadata
/// @param pvParameters
static void processAVRCTask(void *pvParameters)
{
	avrcMetadata incMetadata; // Incoming metadata (pointer to payload)

#ifdef STACK_HIGH_WATERMARK_LOG
	UBaseType_t uxHighWaterMark;
	UBaseType_t minHightWaterMark = PROCESS_AVRC_TASK_STACK_SIZE;
#endif

	// Main loop
	while (true)
	{
// Stack high watermark logging
#ifdef STACK_HIGH_WATERMARK_LOG
		uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
		if (uxHighWaterMark < minHightWaterMark)
		{
			minHightWaterMark = uxHighWaterMark;
			ESP_LOGI("HWM", "Process AVRC Task High Watermark: %d, used stack: %d", minHightWaterMark,
					 PROCESS_AVRC_TASK_STACK_SIZE - minHightWaterMark);
		}
#endif
		// Check incoming metadata in queue, block indefinitely if there is nothing
		if (xQueueReceive(avrcMetadataQueue, &incMetadata, portMAX_DELAY) == pdTRUE)
		{
			if (incMetadata.payload != nullptr)
			{
				// Start processing
				switch (incMetadata.id)
				{
				case ESP_AVRC_MD_ATTR_ALBUM:

					espod.updateAlbumName((char *)incMetadata.payload);
					break;

				case ESP_AVRC_MD_ATTR_ARTIST:
					espod.updateArtistName((char *)incMetadata.payload);
					break;

				case ESP_AVRC_MD_ATTR_TITLE: // Title change triggers the NEXT track if unexpected
					espod.updateTrackTitle((char *)incMetadata.payload);
					break;

				case ESP_AVRC_MD_ATTR_PLAYING_TIME:
					espod.updateTrackDuration(atoi((char *)incMetadata.payload));
					break;
				}

				// End Processing, deallocate memory
				free(incMetadata.payload);
				incMetadata.payload = nullptr;
			}
		}
	}
}
#pragma endregion

#pragma region Helper Function Definitions

/// @brief Configures the CODEC or DAC and starts the A2DP Sink
void initializeA2DPSink()
{
#ifdef AUDIOKIT
	minimalPins.addI2C(PinFunction::CODEC, 32, 33);
	minimalPins.addI2S(PinFunction::CODEC, 0, BCLK_PIN, WS_PIN, DIN_PIN, 35);
	// a2dp_sink.set_stream_reader(read_data_stream, false); // Might need commenting out
	auto cfg = i2s.defaultConfig();
	cfg.copyFrom(info);
	i2s.begin(cfg);
#else
	a2dp_sink.set_stream_reader(read_data_stream, false);
	auto cfg = i2s.defaultConfig(TX_MODE);
	cfg.pin_ws = WS_PIN;
	cfg.pin_data = DIN_PIN;
	cfg.pin_bck = BCLK_PIN;
	cfg.sample_rate = 44100;
	cfg.i2s_format = I2S_LSB_FORMAT;
	i2s.begin(cfg);
#endif

	a2dp_sink.set_auto_reconnect(true, 10000);
	a2dp_sink.set_on_connection_state_changed(connectionStateChanged);
	a2dp_sink.set_on_audio_state_changed(audioStateChanged);
	a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
	a2dp_sink.set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST |
											   ESP_AVRC_MD_ATTR_ALBUM | ESP_AVRC_MD_ATTR_PLAYING_TIME);
	a2dp_sink.set_avrc_rn_play_pos_callback(avrc_rn_play_pos_callback, 1);

	a2dp_sink.start(A2DP_SINK_NAME);

	ESP_LOGI(__func__, "a2dp_sink started: %s", A2DP_SINK_NAME);
	delay(5);
}

/// @brief Initializes the AVRC metadata queue, and attempts to start the
/// related task
/// @return ESP_FAIL if the queue or task could not be created, ESP_OK otherwise
esp_err_t initializeAVRCTask()
{
	avrcMetadataQueue = xQueueCreate(AVRC_QUEUE_SIZE, sizeof(avrcMetadata));
	if (avrcMetadataQueue == nullptr)
	{
		ESP_LOGE(__func__, "Failed to create metadata queue");
		return ESP_FAIL;
	}

	xTaskCreatePinnedToCore(processAVRCTask, "processAVRCTask", PROCESS_AVRC_TASK_STACK_SIZE, NULL,
							PROCESS_AVRC_TASK_PRIORITY, &processAVRCTaskHandle, ARDUINO_RUNNING_CORE);
	if (processAVRCTaskHandle == nullptr)
	{
		ESP_LOGE(__func__, "Failed to create processAVRCTask");
		return ESP_FAIL;
	}

	return ESP_OK;
}
#pragma endregion

#pragma region A2DP/AVRC callbacks Definitions
/// @brief Callback on changes of A2DP connection and AVRCP connection. On
/// disconnect the esPod becomes silent.
/// @param state New state passed by the callback.
/// @param ptr Not used.
void connectionStateChanged(esp_a2d_connection_state_t state, void *ptr)
{
	switch (state)
	{
	case ESP_A2D_CONNECTION_STATE_CONNECTED:
		ESP_LOGD(__func__, "ESP_A2D_CONNECTION_STATE_CONNECTED, espod enabled");
		espod.disabled = false;
		// Meant to pre-fetch playing status
		ESP_LOGI(__func__, "Attempting to send play request.");
		a2dp_sink.play();
#ifdef LED_BUILTIN
		digitalWrite(LED_BUILTIN, INVERT_LED_LOGIC(HIGH));
#endif
		break;
	case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
		ESP_LOGD(__func__, "ESP_A2D_CONNECTION_STATE_DISCONNECTED, espod disabled");
		espod.resetState();
		espod.disabled = true;
#ifdef LED_BUILTIN
		digitalWrite(LED_BUILTIN, INVERT_LED_LOGIC(LOW));
#endif
		break;
	}
#ifdef ENABLE_ACTIVE_DCD
	digitalWrite(DCD_CTRL_PIN, INVERT_DCD_LOGIC(espod.disabled)); // Logic inversion by MACRO
#endif
}

/// @brief Callback for the change of playstate after connection. Aligns the
/// state of the esPod to the state of the phone. Play should be called by the
/// espod interaction
/// @param state The A2DP Stream to align to.
/// @param ptr Not used.
void audioStateChanged(esp_a2d_audio_state_t state, void *ptr)
{
	switch (state)
	{
	case ESP_A2D_AUDIO_STATE_STARTED:
		espod.play(true);
		break;
	case ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND:
		espod.pause(true);
		break;
		// case ESP_A2D_AUDIO_STATE_STOPPED:
		//  espod.stop();
		// 	break;
	}
}

/// @brief Play position callback returning the ms spent since start on every
/// interval - normally 1s
/// @param play_pos Playing Position in ms
void avrc_rn_play_pos_callback(uint32_t play_pos)
{
	espod.updatePlayPosition(play_pos);
	ESP_LOGV(__func__, "PlayPosition called");
}

/// @brief Catch callback for the AVRC metadata. There can be duplicates !
/// @param id Metadata attribute ID : ESP_AVRC_MD_ATTR_xxx
/// @param text Text data passed around, sometimes it's a uint32_t disguised as
/// text
void avrc_metadata_callback(uint8_t id, const uint8_t *text)
{
	// Guard checks
	if (text == NULL)
	{
		ESP_LOGW(__func__, "Received empty pointer for ID %d", id);
		return;
	}
	if ((id != ESP_AVRC_MD_ATTR_PLAYING_TIME) && (text[0] == '\0'))
	{
		ESP_LOGW(__func__, "Empty string received for ID %d", id);
		return;
	}

	avrcMetadata incMetadata;
	incMetadata.id = id;
	incMetadata.payload = (uint8_t *)strdup((const char *)text);

	if (incMetadata.payload == nullptr)
	{
		ESP_LOGE(__func__, "Memory allocation failed for metadata");
		return;
	}

	if (xQueueSend(avrcMetadataQueue, &incMetadata, 0) != pdTRUE)
	{
		ESP_LOGW(__func__, "Metadata queue full, discarding metadata");
		free(incMetadata.payload);
	}
}

/// @brief Callback function that passes intended playback operations from the
/// esPod to the A2DP player (i.e. the phone)
/// @param playCommand
void playStatusHandler(PB_COMMAND playCommand)
{
	switch (playCommand)
	{
	case PB_CMD_STOP:
		a2dp_sink.stop();
		ESP_LOGD(__func__, "A2DP_STOP");
		break;
	case PB_CMD_PLAY:
		a2dp_sink.play();
		ESP_LOGD(__func__, "A2DP_PLAY");
		break;
	case PB_CMD_PAUSE:
		a2dp_sink.pause();
		ESP_LOGD(__func__, "A2DP_PAUSE");
		break;
	case PB_CMD_PREVIOUS_TRACK:
		a2dp_sink.previous();
		ESP_LOGD(__func__, "A2DP_REWIND");
		break;
	case PB_CMD_NEXT_TRACK:
		a2dp_sink.next();
		ESP_LOGD(__func__, "A2DP_NEXT");
		break;
	case PB_CMD_NEXT:
		a2dp_sink.next();
		ESP_LOGD(__func__, "A2DP_NEXT");
		break;
	case PB_CMD_PREV:
		a2dp_sink.previous();
		ESP_LOGD(__func__, "A2DP_PREV");
		break;
	}
}

#pragma endregion