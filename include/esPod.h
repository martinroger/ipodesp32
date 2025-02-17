#pragma once
#include "Arduino.h"
#include "L0x00.h"
#include "L0x04.h"
#include "esPod_conf.h"
#include "esPod_utils.h"

#ifndef IPOD_TAG
    #define IPOD_TAG "esPod"
#endif

class esPod
{
public:
    typedef void playStatusHandler_t(byte playControlCommand);

    //State variables
    bool extendedInterfaceModeActive = false;
    bool disabled = true; //espod starts disabled... it means it keeps flushing the Serial until it is ready to process something
    
    //Metadata variables
    char trackTitle[255]        =   "Title";
    char prevTrackTitle[255]    =   " ";
    char artistName[255]        =   "Artist";
    char prevArtistName[255]    =   " ";
    char albumName[255]         =   "Album";
    char prevAlbumName[255]     =   " ";
    char trackGenre[255]        =   "Genre";
    char playList[255]          =   "Spotify";
    char composer[255]          =   "Composer";
    uint32_t trackDuration      =   1;  //Track duration in ms
    uint32_t prevTrackDuration  =   1;
    uint32_t playPosition       =   0;  //Current playing position of the track in ms

    //Playback Engine
    byte playStatus                     =   PB_STATE_PAUSED; //Current state of the PBEngine
    byte playStatusNotificationState    =   NOTIF_OFF; //Current state of the Notifications engine
    byte trackChangeAckPending          =   0x00; //Indicate there is a pending track change.
    uint64_t trackChangeTimestamp       =   0; //Trigger for the last track change request. Time outs the pending track change.
    byte shuffleStatus                  =   0x00; //00 No Shuffle, 0x01 Tracks 0x02 Albums
    byte repeatStatus                   =   0x02; //00 Repeat off, 01 One track, 02 All tracks

    //TrackList variables
    uint32_t currentTrackIndex             =   0;
    uint32_t prevTrackIndex                =   TOTAL_NUM_TRACKS-1;  //Starts at the end of the tracklist
    const uint32_t totalNumberTracks       =   TOTAL_NUM_TRACKS;
    uint32_t trackList[TOTAL_NUM_TRACKS]   =   {0};
    uint32_t trackListPosition             =   0; //Locator for the position of the track ID in the TrackList (of IDS)


private:
    //FreeRTOS Queues
    QueueHandle_t _cmdQueue;
    QueueHandle_t _txQueue;
    QueueHandle_t _timerQueue;

    //FreeRTOS tasks (and methods...)
    TaskHandle_t _rxTaskHandle;
    TaskHandle_t _processTaskHandle;
    TaskHandle_t _txTaskHandle;
    TaskHandle_t _timerTaskHandle;

    static void _rxTask(void *pvParameters);
    static void _processTask(void *pvParameters);
    static void _txTask(void *pvParameters);
    static void _timerTask(void *pvParameters); // Add this line

    //FreeRTOS timers for delayed acks
    TimerHandle_t _pendingTimer_0x00;
    TimerHandle_t _pendingTimer_0x04;

    //Callbacks for each timer
    static void _pendingTimerCallback_0x00(TimerHandle_t xTimer);
    static void _pendingTimerCallback_0x04(TimerHandle_t xTimer);
    byte _pendingCmdId_0x00;
    byte _pendingCmdId_0x04;

    //Serial to the listening device
    Stream& _targetSerial;

    //Packet utilities
    static byte _checksum(const byte* byteArray, uint32_t len);
    void _sendPacket(const byte* byteArray, uint32_t len);
    void _queuePacket(const byte* byteArray, uint32_t len);
    void _processPacket(const byte* byteArray,uint32_t len);

    bool _rxIncomplete = false;

    //Device metadata
    const char* _name           =   "ipodESP32";
    const uint8_t _SWMajor      =   0x01;
    const uint8_t _SWMinor      =   0x03;
    const uint8_t _SWrevision   =   0x00;
    const char* _serialNumber   =   "AB345F7HIJK";

    //MINI metadata
    bool _accessoryNameReceived             =   false;
    bool _accessoryNameRequested            =   false;
    bool _accessoryCapabilitiesReceived     =   false;
    bool _accessoryCapabilitiesRequested    =   false;
    bool _accessoryFirmwareReceived         =   false;
    bool _accessoryFirmwareRequested        =   false;
    bool _accessoryManufReceived            =   false;
    bool _accessoryManufRequested           =   false;
    bool _accessoryModelReceived            =   false;
    bool _accessoryModelRequested           =   false;
    bool _accessoryHardwareReceived         =   false;
    bool _accessoryHardwareRequested        =   false;

    //Handler functions
    playStatusHandler_t *_playStatusHandler = nullptr;

public:
    esPod(Stream& targetSerial);
    ~esPod();
    void resetState();
    void attachPlayControlHandler(playStatusHandler_t playHandler);
        
    //Processors
    void processLingo0x00(const byte* byteArray, uint32_t len);
    void processLingo0x04(const byte* byteArray, uint32_t len);
    
    //Lingo 0x00
    void L0x00_0x02_iPodAck(byte cmdStatus, byte cmdID);
    void L0x00_0x02_iPodAck(byte cmdStatus, byte cmdID, uint32_t numField);
    void L0x00_0x04_ReturnExtendedInterfaceMode(byte extendedModeByte);
    void L0x00_0x08_ReturniPodName();
    void L0x00_0x0A_ReturniPodSoftwareVersion();
    void L0x00_0x0C_ReturniPodSerialNum();
    void L0x00_0x0E_ReturniPodModelNum();
    void L0x00_0x10_ReturnLingoProtocolVersion(byte targetLingo);
    void L0x00_0x27_GetAccessoryInfo(byte desiredInfo);

    //Lingo 0x04
    void L0x04_0x01_iPodAck(byte cmdStatus, byte cmdID);
    void L0x04_0x01_iPodAck(byte cmdStatus, byte cmdID, uint32_t numField);
    void L0x04_0x0D_ReturnIndexedPlayingTrackInfo(byte trackInfoType, char *trackInfoChars);
    void L0x04_0x0D_ReturnIndexedPlayingTrackInfo(uint32_t trackDuration_ms);
    void L0x04_0x0D_ReturnIndexedPlayingTrackInfo(byte trackInfoType, uint16_t releaseYear);
    void L0x04_0x13_ReturnProtocolVersion();
    void L0x04_0x19_ReturnNumberCategorizedDBRecords(uint32_t categoryDBRecords);
    void L0x04_0x1B_ReturnCategorizedDatabaseRecord(uint32_t index, char* recordString);
    void L0x04_0x1D_ReturnPlayStatus(uint32_t position, uint32_t duration, byte playStatus);
    void L0x04_0x1F_ReturnCurrentPlayingTrackIndex(uint32_t trackIndex);
    void L0x04_0x21_ReturnIndexedPlayingTrackTitle(char* trackTitle);
    void L0x04_0x23_ReturnIndexedPlayingTrackArtistName(char* trackArtistName);
    void L0x04_0x25_ReturnIndexedPlayingTrackAlbumName(char* trackAlbumName);
    void L0x04_0x27_PlayStatusNotification(byte notification, uint32_t numField);
    void L0x04_0x27_PlayStatusNotification(byte notification);
    void L0x04_0x2D_ReturnShuffle(byte shuffleStatus);
    void L0x04_0x30_ReturnRepeat(byte repeatStatus);
    void L0x04_0x36_ReturnNumPlayingTracks(uint32_t numPlayingTracks);
};