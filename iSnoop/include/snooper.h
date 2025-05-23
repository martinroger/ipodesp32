#pragma once
#include "Arduino.h"
#include "L0x00.h"
#include "L0x04.h"
#include "esPod_conf.h"
#include "esPod_utils.h"


class snooper
{
public:


    // Metadata variables
    const char* snooperName;


private:
    // FreeRTOS Queues
    QueueHandle_t _cmdQueue;
    QueueHandle_t _txQueue;


    // FreeRTOS tasks (and methods...)
    TaskHandle_t _rxTaskHandle;
    TaskHandle_t _processTaskHandle;
    TaskHandle_t _txTaskHandle;


    static void _rxTask(void *pvParameters);
    static void _processTask(void *pvParameters);
    static void _txTask(void *pvParameters);



    // Serial to the listening device
    Stream &_targetSerial;

    // Packet utilities
    static byte _checksum(const byte *byteArray, uint32_t len);
    void _sendPacket(const byte *byteArray, uint32_t len);
    void _queuePacket(const byte *byteArray, uint32_t len);
    void _processPacket(const byte *byteArray, uint32_t len);

    bool _rxIncomplete = false;



public:
    snooper(Stream &targetSerial, const char* name);
    ~snooper();
    void resetState();


    // Processors
    void processLingo0x00(const byte *byteArray, uint32_t len);
    void processLingo0x04(const byte *byteArray, uint32_t len);

    // // Lingo 0x00
    // void L0x00_0x00_RequestIdentify();
    // void L0x00_0x02_iPodAck(byte cmdStatus, byte cmdID);
    // void L0x00_0x02_iPodAck(byte cmdStatus, byte cmdID, uint32_t numField);
    // void L0x00_0x04_ReturnExtendedInterfaceMode(byte extendedModeByte);
    // void L0x00_0x08_ReturniPodName();
    // void L0x00_0x0A_ReturniPodSoftwareVersion();
    // void L0x00_0x0C_ReturniPodSerialNum();
    // void L0x00_0x0E_ReturniPodModelNum();
    // void L0x00_0x10_ReturnLingoProtocolVersion(byte targetLingo);
    // void L0x00_0x27_GetAccessoryInfo(byte desiredInfo);

    // // Lingo 0x04
    // void L0x04_0x01_iPodAck(byte cmdStatus, byte cmdID);
    // void L0x04_0x01_iPodAck(byte cmdStatus, byte cmdID, uint32_t numField);
    // void L0x04_0x0D_ReturnIndexedPlayingTrackInfo(byte trackInfoType, char *trackInfoChars);
    // void L0x04_0x0D_ReturnIndexedPlayingTrackInfo(uint32_t trackDuration_ms);
    // void L0x04_0x0D_ReturnIndexedPlayingTrackInfo(byte trackInfoType, uint16_t releaseYear);
    // void L0x04_0x13_ReturnProtocolVersion();
    // void L0x04_0x19_ReturnNumberCategorizedDBRecords(uint32_t categoryDBRecords);
    // void L0x04_0x1B_ReturnCategorizedDatabaseRecord(uint32_t index, char *recordString);
    // void L0x04_0x1D_ReturnPlayStatus(uint32_t position, uint32_t duration, byte playStatus);
    // void L0x04_0x1F_ReturnCurrentPlayingTrackIndex(uint32_t trackIndex);
    // void L0x04_0x21_ReturnIndexedPlayingTrackTitle(char *trackTitle);
    // void L0x04_0x23_ReturnIndexedPlayingTrackArtistName(char *trackArtistName);
    // void L0x04_0x25_ReturnIndexedPlayingTrackAlbumName(char *trackAlbumName);
    // void L0x04_0x27_PlayStatusNotification(byte notification, uint32_t numField);
    // void L0x04_0x27_PlayStatusNotification(byte notification);
    // void L0x04_0x2D_ReturnShuffle(byte shuffleStatus);
    // void L0x04_0x30_ReturnRepeat(byte repeatStatus);
    // void L0x04_0x36_ReturnNumPlayingTracks(uint32_t numPlayingTracks);
};