#pragma once
#include "Arduino.h"
#include "L0x00.h"
#include "L0x03.h"
#include "L0x04.h"
#include "esPod_conf.h"
#include "esPod_utils.h"

#ifndef IPOD_TAG
#define IPOD_TAG "esPod"
#endif

class esPod
{
    friend class L0x00;
    friend class L0x03;
    friend class L0x04;

public:
    typedef void playStatusHandler_t(byte playControlCommand);

    // State variables
    bool extendedInterfaceModeActive = false;
    bool disabled = true; // espod starts disabled... it means it keeps flushing the Serial until it is ready to process something

    // Metadata variables
    char trackTitle[255] = "Title";
    char prevTrackTitle[255] = " ";
    char artistName[255] = "Artist";
    char prevArtistName[255] = " ";
    char albumName[255] = "Album";
    char prevAlbumName[255] = " ";
    char trackGenre[255] = "Genre";
    char playList[255] = "Spotify";
    char composer[255] = "Composer";
    uint32_t trackDuration = 1; // Track duration in ms
    uint32_t prevTrackDuration = 1;
    uint32_t playPosition = 0; // Current playing position of the track in ms

    // Playback Engine
    byte playStatus = PB_STATE_PAUSED;            // Current state of the PBEngine
    byte playStatusNotificationState = NOTIF_OFF; // Current state of the Notifications engine
    byte trackChangeAckPending = 0x00;            // Indicate there is a pending track change.
    uint64_t trackChangeTimestamp = 0;            // Trigger for the last track change request. Time outs the pending track change.
    byte shuffleStatus = 0x00;                    // 00 No Shuffle, 0x01 Tracks 0x02 Albums
    byte repeatStatus = 0x02;                     // 00 Repeat off, 01 One track, 02 All tracks

    // TrackList variables
    uint32_t currentTrackIndex = 0;
    uint32_t prevTrackIndex = TOTAL_NUM_TRACKS - 1; // Starts at the end of the tracklist
    const uint32_t totalNumberTracks = TOTAL_NUM_TRACKS;
    uint32_t trackList[TOTAL_NUM_TRACKS] = {0};
    uint32_t trackListPosition = 0; // Locator for the position of the track ID in the TrackList (of IDS)

private:
    // FreeRTOS Queues
    QueueHandle_t _cmdQueue;
    QueueHandle_t _txQueue;
    QueueHandle_t _timerQueue;

    // FreeRTOS tasks (and methods...)
    TaskHandle_t _rxTaskHandle;
    TaskHandle_t _processTaskHandle;
    TaskHandle_t _txTaskHandle;
    TaskHandle_t _timerTaskHandle;

    static void _rxTask(void *pvParameters);
    static void _processTask(void *pvParameters);
    static void _txTask(void *pvParameters);
    static void _timerTask(void *pvParameters); // Add this line

    // FreeRTOS timers for delayed acks
    TimerHandle_t _pendingTimer_0x00;
    TimerHandle_t _pendingTimer_0x03;
    TimerHandle_t _pendingTimer_0x04;

    // Callbacks for each timer
    static void _pendingTimerCallback_0x00(TimerHandle_t xTimer);
    static void _pendingTimerCallback_0x03(TimerHandle_t xTimer);
    static void _pendingTimerCallback_0x04(TimerHandle_t xTimer);
    byte _pendingCmdId_0x00;
    byte _pendingCmdId_0x03;
    byte _pendingCmdId_0x04;

    // Serial to the listening device
    Stream &_targetSerial;

    // Packet utilities
    static byte _checksum(const byte *byteArray, uint32_t len);
    void _sendPacket(const byte *byteArray, uint32_t len);
    void _queuePacket(const byte *byteArray, uint32_t len);
    void _queuePacketToFront(const byte *byteArray, uint32_t len);
    void _processPacket(const byte *byteArray, uint32_t len);

    bool _rxIncomplete = false;

    // Device metadata
    const char *_name = ESPIPOD_NAME;
    const uint8_t _SWMajor = 0x01;
    const uint8_t _SWMinor = 0x03;
    const uint8_t _SWrevision = 0x00;
    const char *_serialNumber = "AB345F7HIJK";

    // Handler functions
    playStatusHandler_t *_playStatusHandler = nullptr;

    // Boolean flags for track change management
    bool _albumNameUpdated = false;
    bool _artistNameUpdated = false;
    bool _trackTitleUpdated = false;
    bool _trackDurationUpdated = false;

public:
    esPod(Stream &targetSerial);
    ~esPod();
    void resetState();
    void attachPlayControlHandler(playStatusHandler_t playHandler);

    // Wrappers for A2DP and AVRC integration
    void play();
    void pause();
    void updatePlayPosition(uint32_t position);
    void updateAlbumName(const char *albumName);
    void updateArtistName(const char *artistName);
    void updateTrackTitle(const char *trackTitle);
    void updateTrackDuration(uint32_t trackDuration);
};