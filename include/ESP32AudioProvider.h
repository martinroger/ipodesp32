#pragma once
#include <esp_a2dp_api.h>
#include "AudioTools.h"
#include "BluetoothA2DP.h"
#include "BluetoothAudioProvider.h"

#include <mutex>

class ESP32AudioProvider final : public BluetoothAudioProvider
{
    static I2SStream i2s;
    BluetoothA2DPSink a2dp_sink;

    /// @brief Data stream reader callback
    /// @param data Data buffer to pass to the I2S
    /// @param length Length of the data buffer
    static void read_data_stream(const uint8_t *data, uint32_t length);

    void initializeA2DPSink();

    static void connectionStateChanged(esp_a2d_connection_state_t state, void *ptr);
    static void audioStateChanged(esp_a2d_audio_state_t state, void *ptr);
    static void avrc_rn_play_pos_callback(uint32_t play_pos);
    static void avrc_metadata_callback(uint8_t id, const uint8_t *text);

    // Metadata fields
    char _title[128] = "";
    char _artist[128] = "";
    char _album[128] = "";
    char _genre[64] = "";
    uint32_t _duration = 0;
    uint32_t _position = 0;
    byte _playState = PB_STATE_PAUSED; // Add this line

    // Mutex for thread safety
    SemaphoreHandle_t _metadataMutex = nullptr;

public:
    ESP32AudioProvider();

    esp_err_t start() override;
    bool is_connected() override;
    bool is_playing() override;

    const char* get_device_name() override;
    const char* get_playing_song_title() override;
    const char* get_playing_song_artist() override;
    const char* get_playing_song_album() override;
    const char* get_playing_song_genre() override;
    uint32_t get_playing_song_total_time() override;
    uint32_t get_playing_song_elapsed_time() override;
    bool play() override;
    bool pause() override;
    bool stop() override;
    bool next() override;
    bool previous() override;
    bool seek(long position) override;
};
