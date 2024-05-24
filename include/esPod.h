#pragma once
#include "Arduino.h"
#include "L0x00.h"
#include "L0x04.h"

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

class esPod
{
public:
    typedef void playStatusHandler_t(byte playControlCommand);

    //State variables
    bool _extendedInterfaceModeActive;
    uint64_t lastConnected = 0;
    
    //metadata variables
    char _trackTitle[255] = "Title";
    char _artistName[255] = "Artist";
    char _albumName[255] = "Album";
    char _trackGenre[255] = "Genre";
    char _playList[255] = "Spotify";
    char _composer[255] = "Composer";

    //PlaybackEngine
    byte _playStatus = 0x02; //PlayStatus, 00 Stopped, 01 Playing, 02 Paused
    byte _playStatusNotifications = 0x00;
    bool _playStatusNotificationsPaused = false;
    bool notifyTrackChange = false;
    byte _shuffleStatus = 0x01; //00 No Shuffle, 0x01 Tracks 0x02 Albums
    byte _repeatStatus = 0x02; //00 Repeat off, 01 One track, 02 All tracks

private:
    //Serial to the listening device
    Stream& _targetSerial;
    #ifdef DEBUG_MODE
    HardwareSerial& _debugSerial;
    #endif
    byte _prevRxByte;
    
    //Track Index Flip-Flop
    uint32_t _currentTrackIndex = 0;
    uint32_t _totalNumberTracks = 16000;
    
    //Packet-related 
    byte _rxBuf[1024];
    uint32_t _rxLen;
    uint32_t _rxCounter;
    bool _handshakeOK = false;


    //Device metadata
    const char* _name;
    const uint8_t _SWMajor;
    const uint8_t _SWMinor;
    const uint8_t _SWrevision;
    const char* _serialNumber;

    //Mini metadata
    bool _accessoryNameRequested = false;
    bool _accessoryCapabilitiesRequested = false;
    bool _accessoryFirmwareRequested = false;
    bool _accessoryManufRequested = false;
    bool _accessoryModelRequested = false;
    bool _accessoryHardwareRequested = false;

    //Handler functions
    playStatusHandler_t *_playStatusHandler;

public:

    esPod(Stream& targetSerial);

    void resetState();

    void attachPlayControlHandler(playStatusHandler_t playHandler);
    
    //Packet handling
    static byte checksum(const byte* byteArray, uint32_t len);
    void sendPacket(const byte* byteArray, uint32_t len);
    void L0x00_0x02_iPodAck(byte cmdStatus, byte cmdID);
    void L0x00_0x02_iPodAck_pending(uint32_t pendingDelayMS, byte cmdID);
    void L0x00_0x04_ReturnExtendedInterfaceMode(byte extendedModeByte);
    void L0x00_0x08_ReturniPodName();
    void L0x00_0x0A_ReturniPodSoftwareVersion();
    void L0x00_0x0C_ReturniPodSerialNum();
    void L0x00_0x0E_ReturniPodModelNum();
    void L0x00_0x10_ReturnLingoProtocolVersion(byte targetLingo);
    void L0x00_0x27_GetAccessoryInfo(byte desiredInfo);

    void L0x04_0x01_iPodAck(byte cmdStatus, byte cmdID);
    void L0x04_0x0D_ReturnIndexedPlayingTrackInfo(byte trackInfoType, char *trackInfoChars);
    void L0x04_0x0D_ReturnIndexedPlayingTrackInfo(byte trackInfoType);
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


    void processLingo0x00(const byte *byteArray, uint32_t len);
    void processLingo0x04(const byte* byteArray, uint32_t len);
    void processPacket(const byte* byteArray,uint32_t len);

    void refresh();
    void cyclicNotify();
};
