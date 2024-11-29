#pragma once
#include "Arduino.h"
#include "L0x00.h"
#include "L0x04.h"

#ifndef TOTAL_NUM_TRACKS
    #define TOTAL_NUM_TRACKS 3000
#endif

#ifndef TRACK_CHANGE_TIMEOUT
    #define TRACK_CHANGE_TIMEOUT 1100
#endif

enum PB_STATUS : byte
{
    PB_STATE_STOPPED        =   0x00,
    PB_STATE_PLAYING        =   0x01,
    PB_STATE_PAUSED         =   0x02,
    PB_STATE_ERROR          =   0xFF
};

enum PB_COMMAND : byte
{
    PB_CMD_TOGGLE           =   0x01,
    PB_CMD_STOP             =   0x02,
    PB_CMD_NEXT_TRACK       =   0x03,
    PB_CMD_PREVIOUS_TRACK   =   0x04,
    PB_CMD_SEEK_FF          =   0x05,
    PB_CMD_SEEK_RW          =   0x06,
    PB_CMD_STOP_SEEK        =   0x07,
    PB_CMD_NEXT             =   0x08,
    PB_CMD_PREV             =   0x09,
    PB_CMD_PLAY             =   0x0A,
    PB_CMD_PAUSE            =   0x0B
};

enum DB_CATEGORY : byte
{
    DB_CAT_PLAYLIST         =   0x01,
    DB_CAT_ARTIST           =   0x02,
    DB_CAT_ALBUM            =   0x03,
    DB_CAT_GENRE            =   0x04,
    DB_CAT_TRACK            =   0x05,
    DB_CAT_COMPOSER         =   0x06
}; //Just a small selection

enum A2DP_PB_CMD : byte
{
    A2DP_STOP               =   0x00,
    A2DP_PLAY               =   0x01,
    A2DP_PAUSE              =   0x02,
    A2DP_REWIND             =   0x03,
    A2DP_NEXT               =   0x04,
    A2DP_PREV               =   0x05
};

enum NOTIF_STATES : byte
{
    NOTIF_OFF           =   0x00,
    NOTIF_ON            =   0x01
};

class esPod
{
public:
    typedef void playStatusHandler_t(byte playControlCommand);

    //State variables
    bool extendedInterfaceModeActive = false;
    bool disabled = true; //espod starts disabled... it means it keeps flushing the Serial until it is ready to process something
    uint64_t lastConnected  =   0;
    
    //metadata variables
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
    //Serial to the listening device
    Stream& _targetSerial;

    //Packet-related 
    byte _prevRxByte    =   0x00;
    byte _rxBuf[1024]   =   {0x00};
    uint32_t _rxLen     =   0;
    uint32_t _rxCounter =   0;
    bool _rxInProgress  =   false;


    //Device metadata
    const char* _name           =   "ipodESP32";
    const uint8_t _SWMajor      =   0x01;
    const uint8_t _SWMinor      =   0x03;
    const uint8_t _SWrevision   =   0x00;
    const char* _serialNumber   =   "AB345F7HIJK";

    //MINI metadata
    bool _accessoryNameRequested            =   false;
    bool _accessoryCapabilitiesRequested    =   false;
    bool _accessoryFirmwareRequested        =   false;
    bool _accessoryManufRequested           =   false;
    bool _accessoryModelRequested           =   false;
    bool _accessoryHardwareRequested        =   false;

    //Handler functions
    playStatusHandler_t *_playStatusHandler = nullptr;

public:

    esPod(Stream& targetSerial);
    void resetState();
    void attachPlayControlHandler(playStatusHandler_t playHandler);
    
    //Packet handling
    static byte checksum(const byte* byteArray, uint32_t len);
    void sendPacket(const byte* byteArray, uint32_t len);
    
    //Lingo 0x00
    void L0x00_0x02_iPodAck(byte cmdStatus, byte cmdID);
    void L0x00_0x02_iPodAck(byte cmdStatus, byte cmdID, uint32_t numField);
    //void L0x00_0x02_iPodAck_pending(uint32_t pendingDelayMS, byte cmdID);
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

    //Processors
    void processLingo0x00(const byte* byteArray, uint32_t len);
    void processLingo0x04(const byte* byteArray, uint32_t len);
    void processPacket(const byte* byteArray,uint32_t len);

    //Cyclic functions
    void refresh();
};
