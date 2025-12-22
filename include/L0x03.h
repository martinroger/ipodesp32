#pragma once
#include "Arduino.h"
#include "esPod_utils.h"
class esPod;

#define L0x03_GetCurrentEQProfileIndex 0x01
#define L0x03_SetCurrentEQProfileIndex 0x03
#define L0x03_GetNumEQProfiles 0x04
#define L0x03_GetIndexedEQProfileName 0x06
#define L0x03_SetRemoteEventNotification 0x08
#define L0x03_GetRemoteEventStatus 0x0A
#define L0x03_GetiPodStateInfo 0x0C
#define L0x03_SetiPodStateInfo 0x0E
#define L0x03_GetPlayStatus 0x0F
#define L0x03_SetCurrentPlayingTrack 0x11
#define L0x03_GetIndexedPlayingTrackInfo 0x12
#define L0x03_GetNumPlayingTracks 0x14
#define L0x03_GetArtworkFormats 0x16
#define L0x03_GetTrackArtworkData 0x18
#define L0x03_GetPowerBatteryState 0x1A
#define L0x03_GetSoundCheckState 0x1C
#define L0x03_SetSoundCheckState 0x1E
#define L0x03_GetTrackArtworkTimes 0x1F
#define L0x03_CreateGeniusPlaylist 0x21
#define L0x03_IsGeniusAvailableForTrack 0x22

class L0x03
{

public:
    static void processLingo(esPod *esp, const byte *byteArray, uint32_t len);
    static void _0x00_iPodAck(esPod *esp, IPOD_ACK_CODE ackCode, byte cmdID);
    static void _0x00_iPodAck(esPod *esp, IPOD_ACK_CODE ackCode, byte cmdID, uint32_t numField);
    static void _0x02_RetCurrentEQProfileIndex(esPod *esp);
    static void _0x05_RetNumEQProfiles(esPod *esp);
    static void _0x07_RetIndexedEQProfileName(esPod *esp);
    static void _0x09_RemoteEventNotification(esPod *esp);
    static void _0x0B_RetRemoteEventStatus(esPod *esp, uint32_t remEventStatus);
    static void _0x0D_RetiPodStateInfo(esPod *esp);
    static void _0x10_RetPlayStatus(esPod *esp, byte playState, uint32_t trackIndex, uint32_t trackTotMs, uint32_t trackPosMs);
    static void _0x13_RetIndexedPlayingTrackInfo(esPod *esp, byte trackInfoType, char *trackInfoChars);
    static void _0x13_RetIndexedPlayingTrackInfo(esPod *esp, uint32_t trackDuration_ms);
    static void _0x15_RetNumPlayingTracks(esPod *esp, uint32_t numPlayingTracks);
    static void _0x17_RetArtworkFormats(esPod *esp);
    static void _0x19_RetTrackArtworkData(esPod *esp);
    static void _0x1B_RetPowerBatteryState(esPod *esp, byte powerBatteryState);
    static void _0x1D_RetSoundCheckState(esPod *esp, byte soundCheckState);
    static void _0x20_RetTrackArtworkTimes(esPod *esp);
};
