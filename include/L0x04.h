#pragma once
#include "Arduino.h"
#include "esPod.h"

#define L0x04_GetIndexedPlayingTrackInfo 0x0C
#define L0x04_RequestProtocolVersion 0x12
#define L0x04_ResetDBSelection 0x16
#define L0x04_SelectDBRecord 0x17
#define L0x04_GetNumberCategorizedDBRecords 0x18
#define L0x04_RetrieveCategorizedDatabaseRecords 0x1A
#define L0x04_GetPlayStatus 0x1C
#define L0x04_GetCurrentPlayingTrackIndex 0x1E
#define L0x04_GetIndexedPlayingTrackTitle 0x20
#define L0x04_GetIndexedPlayingTrackArtistName 0x22
#define L0x04_GetIndexedPlayingTrackAlbumName 0x24
#define L0x04_SetPlayStatusChangeNotification 0x26
#define L0x04_PlayCurrentSelection 0x28
#define L0x04_PlayControl 0x29
#define L0x04_GetShuffle 0x2C
#define L0x04_SetShuffle 0x2E
#define L0x04_GetRepeat 0x2F
#define L0x04_SetRepeat 0x31
#define L0x04_GetNumPlayingTracks 0x35
#define L0x04_SetCurrentPlayingTrack 0x37

class L0x04
{
public:
    void processLingo(esPod *esp, const byte *byteArray, uint32_t len);

    // Lingo 0x04 Handlers
    void _0x01_iPodAck(esPod *esp, byte cmdStatus, byte cmdID);
    void _0x01_iPodAck(esPod *esp, byte cmdStatus, byte cmdID, uint32_t numField);
    void _0x0D_ReturnIndexedPlayingTrackInfo(esPod *esp, byte trackInfoType, char *trackInfoChars);
    void _0x0D_ReturnIndexedPlayingTrackInfo(esPod *esp, uint32_t trackDuration_ms);
    void _0x0D_ReturnIndexedPlayingTrackInfo(esPod *esp, byte trackInfoType, uint16_t releaseYear);
    void _0x13_ReturnProtocolVersion(esPod *esp);
    void _0x19_ReturnNumberCategorizedDBRecords(esPod *esp, uint32_t categoryDBRecords);
    void _0x1B_ReturnCategorizedDatabaseRecord(esPod *esp, uint32_t index, char *recordString);
    void _0x1D_ReturnPlayStatus(esPod *esp, uint32_t position, uint32_t duration, byte playStatus);
    void _0x1F_ReturnCurrentPlayingTrackIndex(esPod *esp, uint32_t trackIndex);
    void _0x21_ReturnIndexedPlayingTrackTitle(esPod *esp, char *trackTitle);
    void _0x23_ReturnIndexedPlayingTrackArtistName(esPod *esp, char *trackArtistName);
    void _0x25_ReturnIndexedPlayingTrackAlbumName(esPod *esp, char *trackAlbumName);
    void _0x27_PlayStatusNotification(esPod *esp, byte notification, uint32_t numField);
    void _0x27_PlayStatusNotification(esPod *esp, byte notification);
    void _0x2D_ReturnShuffle(esPod *esp, byte shuffleStatus);
    void _0x30_ReturnRepeat(esPod *esp, byte repeatStatus);
    void _0x36_ReturnNumPlayingTracks(esPod *esp, uint32_t numPlayingTracks);
};