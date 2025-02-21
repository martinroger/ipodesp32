#include <Arduino.h>

#include "AudioTools.h"
#include "BluetoothA2DPSink.h"
#include "esPod.h"

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
HardwareSerial altSerial(1);
esPod espod(altSerial);
#else
esPod espod(Serial);
#endif
#else
I2SStream i2s;
BluetoothA2DPSink a2dp_sink(i2s);
esPod espod(Serial2);
#endif
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
void initializeA2DPSink();
esp_err_t initializeAVRCTask();
#pragma endregion

#pragma region A2DP/AVRC callbacks declaration
void connectionStateChanged(esp_a2d_connection_state_t state, void *ptr);
void audioStateChanged(esp_a2d_audio_state_t state, void *ptr);
void avrc_rn_play_pos_callback(uint32_t play_pos);
void avrc_metadata_callback(uint8_t id, const uint8_t *text);
void playStatusHandler(byte playCommand);
#pragma endregion

void setup()
{
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


    if (initializeAVRCTask() != ESP_OK)
        esp_restart();
    initializeA2DPSink();
    initializeSerial();
    espod.attachPlayControlHandler(playStatusHandler);
    ESP_LOGI("SETUP", "Waiting for peer");
    while (a2dp_sink.get_connection_state() != ESP_A2D_CONNECTION_STATE_CONNECTED)
    {
        delay(10);
    }
    delay(50);
    ESP_LOGI("SETUP", "Peer connected: %s", a2dp_sink.get_peer_name());
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

// AVRC Queue and Task
QueueHandle_t avrcMetadataQueue;
TaskHandle_t processAVRCTaskHandle;

/// @brief Low priority task to process a queue of received metadata
/// @param pvParameters
static void processAVRCTask(void *pvParameters)
{
    avrcMetadata incMetadata; // Incoming metadata (pointer to payload)
    // Metadata buffers
    char incAlbumName[255] = "incAlbum";
    char incArtistName[255] = "incArtist";
    char incTrackTitle[255] = "incTitle";
    uint32_t incTrackDuration = 0;
    bool albumNameUpdated = false;
    bool artistNameUpdated = false;
    bool trackTitleUpdated = false;
    bool trackDurationUpdated = false;

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
            ESP_LOGI("HWM Logging", "Process AVRC Task High Watermark: %d, used stack: %d", minHightWaterMark,
                     PROCESS_AVRC_TASK_STACK_SIZE - minHightWaterMark);
        }
#endif
        // Check incoming metadata in queue
        if (xQueueReceive(avrcMetadataQueue, &incMetadata, 0) == pdTRUE)
        {
            // Start processing
            switch (incMetadata.id)
            {
            case ESP_AVRC_MD_ATTR_ALBUM:
                strcpy(incAlbumName,
                       (char *)incMetadata.payload); // Buffer the incoming album string
                if (espod.trackChangeAckPending > 0x00)
                { // There is a pending metadata update
                    if (!albumNameUpdated)
                    { // The album Name has not been updated yet
                        strcpy(espod.albumName, incAlbumName);
                        albumNameUpdated = true;
                        ESP_LOGD("AVRC_CB", "Album rxed, ACK pending, albumNameUpdated to %s", espod.albumName);
                    }
                    else
                    {
                        ESP_LOGD("AVRC_CB", "Album rxed, ACK pending, already updated to %s", espod.albumName);
                    }
                }
                else
                { // There is no pending track change from iPod : active or
                  // passive track change from avrc target
                    if (strcmp(incAlbumName, espod.albumName) != 0)
                    { // Different incoming metadata
                        strcpy(espod.prevAlbumName, espod.albumName);
                        strcpy(espod.albumName, incAlbumName);
                        albumNameUpdated = true;
                        ESP_LOGD("AVRC_CB", "Album rxed, NO ACK pending, albumNameUpdated to %s", espod.albumName);
                    }
                    else
                    { // Despammer for double sends
                        ESP_LOGD("AVRC_CB", "Album rxed, NO ACK pending, already updated to %s", espod.albumName);
                    }
                }
                break;

            case ESP_AVRC_MD_ATTR_ARTIST:
                strcpy(incArtistName,
                       (char *)incMetadata.payload); // Buffer the incoming artist string
                if (espod.trackChangeAckPending > 0x00)
                { // There is a pending metadata update
                    if (!artistNameUpdated)
                    { // The artist name has not been updated
                      // yet
                        strcpy(espod.artistName, incArtistName);
                        artistNameUpdated = true;
                        ESP_LOGD("AVRC_CB", "Artist rxed, ACK pending, artistNameUpdated to %s", espod.artistName);
                    }
                    else
                    {
                        ESP_LOGD("AVRC_CB", "Artist rxed, ACK pending, already updated to %s", espod.artistName);
                    }
                }
                else
                { // There is no pending track change from iPod : active or
                  // passive track change from avrc target
                    if (strcmp(incArtistName, espod.artistName) != 0)
                    { // Different incoming metadata
                        strcpy(espod.prevArtistName, espod.artistName);
                        strcpy(espod.artistName, incArtistName);
                        artistNameUpdated = true;
                        ESP_LOGD("AVRC_CB", "Artist rxed, NO ACK pending, artistNameUpdated to %s", espod.artistName);
                    }
                    else
                    { // Despammer for double sends
                        ESP_LOGD("AVRC_CB", "Artist rxed, NO ACK pending, already updated to %s", espod.artistName);
                    }
                }
                break;

            case ESP_AVRC_MD_ATTR_TITLE: // Title change triggers the NEXT track
                                         // assumption if unexpected. It is too
                                         // intensive to try to do NEXT/PREV
                                         // guesswork
                strcpy(incTrackTitle,
                       (char *)incMetadata.payload); // Buffer the incoming track title
                if (espod.trackChangeAckPending > 0x00)
                { // There is a pending metadata update
                    if (!trackTitleUpdated)
                    { // The track title has not been updated
                      // yet
                        strcpy(espod.trackTitle, incTrackTitle);
                        trackTitleUpdated = true;
                        ESP_LOGD("AVRC_CB", "Title rxed, ACK pending, trackTitleUpdated to %s", espod.trackTitle);
                    }
                    else
                    {
                        ESP_LOGD("AVRC_CB", "Title rxed, ACK pending, already updated to %s", espod.trackTitle);
                    }
                }
                else
                { // There is no pending track change from iPod : active or
                  // passive track change from avrc target
                    if (strcmp(incTrackTitle, espod.trackTitle) != 0)
                    { // Different from current track Title -> Systematic NEXT
                        // Assume it is Next, perform cursor operations
                        espod.trackListPosition = (espod.trackListPosition + 1) % TOTAL_NUM_TRACKS;
                        espod.prevTrackIndex = espod.currentTrackIndex;
                        espod.currentTrackIndex = (espod.currentTrackIndex + 1) % TOTAL_NUM_TRACKS;
                        espod.trackList[espod.trackListPosition] = (espod.currentTrackIndex);
                        // Copy new title and flag that data has been updated
                        strcpy(espod.prevTrackTitle, espod.trackTitle);
                        strcpy(espod.trackTitle, incTrackTitle);
                        trackTitleUpdated = true;
                        ESP_LOGD("AVRC_CB",
                                 "Title rxed, NO ACK pending, AUTONEXT, trackTitleUpdated "
                                 "to %s\n\ttrackPos %d trackIndex %d",
                                 espod.trackTitle, espod.trackListPosition, espod.currentTrackIndex);
                    }
                    else
                    { // Despammer for double sends
                        ESP_LOGD("AVRC_CB", "Title rxed, NO ACK pending, same name : %s", espod.trackTitle);
                    }
                }
                break;

            case ESP_AVRC_MD_ATTR_PLAYING_TIME:
                incTrackDuration = String((char *)incMetadata.payload).toInt();
                if (espod.trackChangeAckPending > 0x00)
                { // There is a pending metadata update
                    if (!trackDurationUpdated)
                    { // The duration has not been updated
                      // yet
                        espod.trackDuration = incTrackDuration;
                        trackDurationUpdated = true;
                        ESP_LOGD("AVRC_CB", "Duration rxed, ACK pending, trackDurationUpdated to %d",
                                 espod.trackDuration);
                    }
                    else
                    {
                        ESP_LOGD("AVRC_CB", "Duration rxed, ACK pending, already updated to %d", espod.trackDuration);
                    }
                }
                else
                { // There is no pending track change from iPod : active or
                  // passive track change from avrc target
                    if (incTrackDuration != espod.trackDuration)
                    { // Different incoming metadata
                        espod.trackDuration = incTrackDuration;
                        trackDurationUpdated = true;
                        ESP_LOGD("AVRC_CB", "Duration rxed, NO ACK pending, trackDurationUpdated to %d",
                                 espod.trackDuration);
                    }
                    else
                    { // Despammer for double sends
                        ESP_LOGD("AVRC_CB", "Duration rxed, NO ACK pending, already updated to %d",
                                 espod.trackDuration);
                    }
                }
                break;
            }

            // Check if it is time to send a notification
            if (albumNameUpdated && artistNameUpdated && trackTitleUpdated && trackDurationUpdated)
            {
                // If all fields have received at least one update and the
                // trackChangeAckPending is still hanging. The failsafe for this one is
                // in the espod _processTask
                if (espod.trackChangeAckPending > 0x00)
                {
                    ESP_LOGD("AVRC_CB",
                             "Artist+Album+Title+Duration +++ ACK Pending "
                             "0x%x\n\tPending duration: %d",
                             espod.trackChangeAckPending, millis() - espod.trackChangeTimestamp);
                    espod.L0x04_0x01_iPodAck(iPodAck_OK, espod.trackChangeAckPending);
                    espod.trackChangeAckPending = 0x00;
                    ESP_LOGD("AVRC_CB", "trackChangeAckPending reset to 0x00");
                }
                albumNameUpdated = false;
                artistNameUpdated = false;
                trackTitleUpdated = false;
                trackDurationUpdated = false;
                ESP_LOGD("AVRC_CB", "Artist+Album+Title+Duration : True -> False");
                // Inform the car
                if (espod.playStatusNotificationState == NOTIF_ON)
                {
                    espod.L0x04_0x27_PlayStatusNotification(0x01, espod.currentTrackIndex);
                }
            }

            // End Processing, deallocate memory
            delete[] incMetadata.payload;
            incMetadata.payload = nullptr;
        }
        vTaskDelay(pdMS_TO_TICKS(AVRC_INTERVAL_MS));
    }
}
#pragma endregion

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
#ifndef AUDIOKIT
    Serial2.setRxBufferSize(1024);
    Serial2.setTxBufferSize(1024);
    Serial2.begin(19200);
#else
#ifdef USE_ALT_SERIAL
    altSerial.setPins(19, 22);
    altSerial.setRxBufferSize(1024);
    altSerial.setTxBufferSize(1024);
    altSerial.begin(19200);
#else
    Serial.setRxBufferSize(1024);
    Serial.setTxBufferSize(1024);
    Serial.begin(19200);
#endif
#endif
}

/// @brief Configures the CODEC or DAC and starts the A2DP Sink
void initializeA2DPSink()
{
#ifdef AUDIOKIT
    minimalPins.addI2C(PinFunction::CODEC, 32, 33);
    minimalPins.addI2S(PinFunction::CODEC, 0, 27, 25, 26, 35);
    auto cfg = i2s.defaultConfig();
    cfg.copyFrom(info);
    i2s.begin(cfg);
#else
    auto cfg = i2s.defaultConfig(TX_MODE);
    cfg.pin_ws = 25;
    cfg.pin_bck = 26;
    cfg.pin_data = 22;
    cfg.sample_rate = 44100;
    cfg.i2s_format = I2S_LSB_FORMAT;
    i2s.begin(cfg);
    /*
    Default pins are as follows :
    WSEL  ->  25
    DIN   ->  22
    BCLK  ->  26
    */
#endif

    a2dp_sink.set_auto_reconnect(true);
    a2dp_sink.set_on_connection_state_changed(connectionStateChanged);
    a2dp_sink.set_on_audio_state_changed(audioStateChanged);
    a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
    a2dp_sink.set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST |
                                               ESP_AVRC_MD_ATTR_ALBUM | ESP_AVRC_MD_ATTR_PLAYING_TIME);
    a2dp_sink.set_avrc_rn_play_pos_callback(avrc_rn_play_pos_callback, 1);

#ifdef AUDIOKIT
    a2dp_sink.start("MiNiPoD56");
#else
    a2dp_sink.start("espiPod 2");
#endif
    ESP_LOGI("SETUP", "a2dp_sink started: %s", a2dp_sink.get_name());
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
        ESP_LOGE("SETUP", "Failed to create metadata queue");
        return ESP_FAIL;
    }

    xTaskCreatePinnedToCore(processAVRCTask, "processAVRCTask", PROCESS_AVRC_TASK_STACK_SIZE, NULL,
                            PROCESS_AVRC_TASK_PRIORITY, &processAVRCTaskHandle, ARDUINO_RUNNING_CORE);
    if (processAVRCTaskHandle == nullptr)
    {
        ESP_LOGE("SETUP", "Failed to create processAVRCTask");
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
        ESP_LOGD("A2DP_CB", "ESP_A2D_CONNECTION_STATE_CONNECTED, espod enabled");
        espod.disabled = false;
        break;
    case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
        ESP_LOGD("A2DP_CB", "ESP_A2D_CONNECTION_STATE_DISCONNECTED, espod disabled");
        espod.resetState();
        espod.disabled = true;
        break;
    }
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
        espod.playStatus = PB_STATE_PLAYING;
        ESP_LOGD("A2DP_CB", "ESP_A2D_AUDIO_STATE_STARTED, espod.playStatus = PB_STATE_PLAYING");
        break;
    case ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND:
        espod.playStatus = PB_STATE_PAUSED;
        ESP_LOGD("A2DP_CB", "ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND, espod.playStatus "
                            "= PB_STATE_PAUSED");
        break;
    case ESP_A2D_AUDIO_STATE_STOPPED:
        espod.playStatus = PB_STATE_STOPPED;
        ESP_LOGD("A2DP_CB", "ESP_A2D_AUDIO_STATE_STOPPED, espod.playStatus = PB_STATE_STOPPED");
        break;
    }
}

/// @brief Play position callback returning the ms spent since start on every
/// interval - normally 1s
/// @param play_pos Playing Position in ms
void avrc_rn_play_pos_callback(uint32_t play_pos)
{
    espod.playPosition = play_pos;
    ESP_LOGV("AVRC_CB", "PlayPosition called");
    if (espod.playStatusNotificationState == NOTIF_ON && espod.trackChangeAckPending == 0x00)
    {
        espod.L0x04_0x27_PlayStatusNotification(0x04, play_pos);
    }
}

/// @brief Catch callback for the AVRC metadata. There can be duplicates !
/// @param id Metadata attribute ID : ESP_AVRC_MD_ATTR_xxx
/// @param text Text data passed around, sometimes it's a uint32_t disguised as
/// text
void avrc_metadata_callback(uint8_t id, const uint8_t *text)
{
    avrcMetadata incMetadata;
    incMetadata.id = id;
    incMetadata.payload = new uint8_t[255];
    memcpy(incMetadata.payload, text, 255);
    if (xQueueSend(avrcMetadataQueue, &incMetadata, 0) != pdTRUE)
    {
        ESP_LOGW("AVRC_CB", "Metadata queue full, discarding metadata");
        delete[] incMetadata.payload;
        incMetadata.payload = nullptr;
    }
}

/// @brief Callback function that passes intended playback operations from the
/// esPod to the A2DP player (i.e. the phone)
/// @param playCommand A2DP_xx command instruction. It does not match the
/// PB_CMD_xx codes !!!
void playStatusHandler(byte playCommand)
{
    switch (playCommand)
    {
    case A2DP_STOP:
        a2dp_sink.stop();
        ESP_LOGD("A2DP_CB", "A2DP_STOP");
        break;
    case A2DP_PLAY:
        a2dp_sink.play();
        ESP_LOGD("A2DP_CB", "A2DP_PLAY");
        break;
    case A2DP_PAUSE:
        a2dp_sink.pause();
        ESP_LOGD("A2DP_CB", "A2DP_PAUSE");
        break;
    case A2DP_REWIND:
        a2dp_sink.previous();
        ESP_LOGD("A2DP_CB", "A2DP_REWIND");
        break;
    case A2DP_NEXT:
        a2dp_sink.next();
        ESP_LOGD("A2DP_CB", "A2DP_NEXT");
        break;
    case A2DP_PREV:
        a2dp_sink.previous();
        ESP_LOGD("A2DP_CB", "A2DP_PREV");
        break;
    }
}

#pragma endregion