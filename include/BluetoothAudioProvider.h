#pragma once
#include <cstdint>
#include <esp_err.h>

#include "esPod_utils.h"

class BluetoothAudioProvider
{
public:
    virtual ~BluetoothAudioProvider() = default;
    virtual bool is_connected() = 0;
    virtual bool is_playing() = 0;

    virtual const char* get_device_name() = 0;

    virtual const char* get_playing_song_title() = 0;
    virtual const char* get_playing_song_artist() = 0;
    virtual const char* get_playing_song_album() = 0;
    virtual const char* get_playing_song_genre() = 0;


    virtual uint32_t get_playing_song_total_time() = 0;
    virtual uint32_t get_playing_song_elapsed_time() = 0;

    virtual bool play() = 0;
    virtual bool pause() = 0;
    virtual bool stop() = 0;
    virtual bool next() = 0;
    virtual bool previous() = 0;
    virtual bool seek(long position) = 0;
    virtual esp_err_t start()
    {
        return ESP_OK;
    }
};

