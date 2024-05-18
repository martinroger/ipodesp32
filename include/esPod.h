#pragma once
#include "Arduino.h"
#include "L0x00.h"
#include "L0x04.h"

class esPod
{
private:
    //Serial to the listening device
    Stream& _targetSerial;
    #ifdef DEBUG_MODE
    HardwareSerial& _debugSerial;
    #endif
    byte _prevRxByte;
    
    //State variables
    bool _extendedInterfaceModeActive;
    

    //metadata variables
    uint8_t *trackTitle;
    uint8_t *artistName;
    uint8_t *albumName;
    char _trackGenre[8] = "Spotify";

    //PlaybackEngine
    byte _playStatus = 0x00; //PlayStatus, 00 Stopped, 01 Playing, 02 Paused
    
    //Packet-related 
    byte _rxBuf[1024];
    uint32_t _rxLen;
    uint32_t _rxCounter;

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

public:
    esPod(Stream& targetSerial);
    
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
    void L0x04_0x2D_ReturnShuffle(byte shuffleStatus);
    void L0x04_0x30_ReturnRepeat(byte repeatStatus);
    void L0x04_0x36_ReturnNumPlayingTracks(uint32_t numPlayingTracks);


    void processLingo0x00(const byte *byteArray, uint32_t len);
    void processLingo0x04(const byte* byteArray, uint32_t len);
    void processPacket(const byte* byteArray,uint32_t len);

    void refresh();
};


