#pragma once
#include "Arduino.h"
#include "esPod_utils.h"
class esPod;

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
#define L0x04_GetMonoDisplayImageLimits 0x33
#define L0x04_GetNumPlayingTracks 0x35
#define L0x04_SetCurrentPlayingTrack 0x37
#define L0x04_SelectSortDBRecord 0x38
#define L0x04_GetColorDisplayImageLimits 0x39

class L0x04
{
public:
    static void processLingo(esPod *esp, const byte *byteArray, uint32_t len);

    // Lingo 0x04 Handlers
    static void _0x01_iPodAck(esPod *esp, IPOD_ACK_CODE ackCode, byte cmdID);
    static void _0x01_iPodAck(esPod *esp, IPOD_ACK_CODE ackCode, byte cmdID, uint32_t numField);
    // static void _0x03_ReturnCurrentPlayingTrackChapterInfo(esPod *esp);
    // static void _0x06_ReturnCurrentPlayingTrackChapterPlayStatus(esPod *esp);
    // static void _0x08_ReturnCurrentPlayingTrackChapterName(esPod *esp);
    // static void _0x0A_ReturnAudiobookSpeed(esPod *esp);
    static void _0x0D_ReturnIndexedPlayingTrackInfo(esPod *esp, byte trackInfoType, char *trackInfoChars);
    static void _0x0D_ReturnIndexedPlayingTrackInfo(esPod *esp, uint32_t trackDuration_ms);
    static void _0x0D_ReturnIndexedPlayingTrackInfo(esPod *esp, byte trackInfoType, uint16_t releaseYear);
    // static void _0x0F_RetArtworkFormats(esPod *esp);
    // static void _0x11_RetTrackArtworkData(esPod *esp);
    static void _0x13_ReturnProtocolVersion(esPod *esp);
    // static void _0x15_ReturniPodName(esPod *esp);
    static void _0x19_ReturnNumberCategorizedDBRecords(esPod *esp, uint32_t categoryDBRecords);
    static void _0x1B_ReturnCategorizedDatabaseRecord(esPod *esp, uint32_t index, char *recordString);
    static void _0x1D_ReturnPlayStatus(esPod *esp, uint32_t position, uint32_t duration, byte playStatus);
    static void _0x1F_ReturnCurrentPlayingTrackIndex(esPod *esp, uint32_t trackIndex);
    static void _0x21_ReturnIndexedPlayingTrackTitle(esPod *esp, char *trackTitle);
    static void _0x23_ReturnIndexedPlayingTrackArtistName(esPod *esp, char *trackArtistName);
    static void _0x25_ReturnIndexedPlayingTrackAlbumName(esPod *esp, char *trackAlbumName);
    static void _0x27_PlayStatusNotification(esPod *esp, byte notification, uint32_t numField);
    static void _0x27_PlayStatusNotification(esPod *esp, byte notification);
    // static void _0x2B_RetTrackArtworkTimes(esPod *esp);
    static void _0x2D_ReturnShuffle(esPod *esp, byte shuffleStatus);
    static void _0x30_ReturnRepeat(esPod *esp, byte repeatStatus);
    static void _0x34_ReturnMonoDisplayImageLimits(esPod *esp, uint16_t maxImageW, uint16_t maxImageH, byte dispPixelFmt);
    static void _0x36_ReturnNumPlayingTracks(esPod *esp, uint32_t numPlayingTracks);
    static void _0x3A_ReturnColorDisplayImageLimits(esPod *esp, uint16_t maxImageW, uint16_t maxImageH, byte dispPixelFmt);
    // static void _0x3D_RetDBiTunesInfo(esPod *esp);
    // static void _0x3F_RetUIDTrackInfo(esPod *esp);
    // static void _0x41_RetDBTrackInfo(esPod *esp);
    // static void _0x43_RetPBTrackInfo(esPod *esp);
    // static void _0x49_RetPlaylistInfo(esPod *esp);
    // static void _0x4D_RetArtworkTimes(esPod *esp);
    // static void _0x4F_RetArtworkData(esPod *esp);
};