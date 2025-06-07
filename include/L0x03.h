#pragma once
#include "Arduino.h"
#include "esPod_utils.h"
class esPod;

// Defines

class L0x03
{

public:
    static void processLingo(esPod *esp, const byte *byteArray, uint32_t len);
    static void _0x00_iPodAck(esPod *esp);
    static void _0x02_RetCurrentEQProfileIndex(esPod *esp);
    static void _0x05_RetNumEQProfiles(esPod *esp);
    static void _0x07_RetIndexedEQProfileName(esPod *esp);
    static void _0x09_RemoteEventNotification(esPod *esp);
    static void _0x0B_RetRemoteEventStatus(esPod *esp);
    static void _0x0D_RetiPodStateInfo(esPod *esp);
    static void _0x10_RetPlayStatus(esPod *esp);
    static void _0x13_RetIndexedPlayingTrackInfo(esPod *esp);
    static void _0x15_RetNumPlayingTracks(esPod *esp);
};
