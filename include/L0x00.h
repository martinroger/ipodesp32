#pragma once
#include "Arduino.h"
#include "esPod_utils.h"
class esPod;

#define L0x00_Identify 0x01
#define L0x00_RequestExtendedInterfaceMode 0x03
#define L0x00_EnterExtendedInterfaceMode 0x05
#define L0x00_ExitExtendedInterfaceMode 0x06
#define L0x00_RequestiPodName 0x07
#define L0x00_RequestiPodSoftwareVersion 0x09
#define L0x00_RequestiPodSerialNum 0x0B
#define L0x00_RequestiPodModelNum 0x0D
#define L0x00_RequestLingoProtocolVersion 0x0F
#define L0x00_IdentifyDeviceLingoes 0x13
#define L0x00_GetiPodOptions 0x24
#define L0x00_RetAccessoryInfo 0x28

class L0x00
{
public:
    static void processLingo(esPod *esp, const byte *byteArray, uint32_t len);

    static void _0x00_RequestIdentify(esPod *esp);
    static void _0x02_iPodAck(esPod *esp, IPOD_ACK_CODE ackCode, byte cmdID);
    static void _0x02_iPodAck(esPod *esp, IPOD_ACK_CODE ackCode, byte cmdID, uint32_t numField);
    static void _0x04_ReturnExtendedInterfaceMode(esPod *esp, byte extendedModeByte);
    static void _0x08_ReturniPodName(esPod *esp);
    static void _0x0A_ReturniPodSoftwareVersion(esPod *esp);
    static void _0x0C_ReturniPodSerialNum(esPod *esp);
    static void _0x0E_ReturniPodModelNum(esPod *esp);
    static void _0x10_ReturnLingoProtocolVersion(esPod *esp, byte targetLingo);
    // static void _0x12_ReturnTransportMaxPayloadSize(esPod *esp);
    // static void _0x14_GetAccessoryAuthenticationInfo(esPod *esp);
    // static void _0x16_AckAccessoryAuthenticationInfo(esPod *esp);
    // static void _0x17_GetAccessoryAuthenticationSignature(esPod *esp);
    // static void _0x19_AckAccessoryAuthenticationStatus(esPod *esp);
    // static void _0x1B_RetiPodAuthenticationInfo(esPod *esp);
    // static void _0x1E_RetiPodAuthenticationSignature(esPod *esp);
    // static void _0x23_NotifyiPodStateChange(esPod *esp, byte stateChg);
    static void _0x25_RetiPodOptions(esPod *esp, uint64_t optBitField);
    static void _0x27_GetAccessoryInfo(esPod *esp, byte desiredInfo);
    // static void _0x2A_RetiPodPreferences(esPod *esp);
    // static void _0x36_RetUIMode(esPod *esp);
    // static void _0x3A_AckFIDTokenValues(esPod *esp);
    // static void _0x3C_IDPSStatus(esPod *esp);
    // static void _0x3F_OpenDataSessionForProtocol(esPod *esp);
    // static void _0x40_CloseDataSession(esPod *esp);
    // static void _0x43_iPodDataTransfer(esPod *esp);
    // static void _0x46_SetAccessoryStatusNotification(esPod *esp);
    // static void _0x4A_iPodNotification(esPod *esp);
    // static void _0x4C_RetiPodOptionsForLingo(esPod *esp);
    // static void _0x4E_RetEventNotification(esPod *esp);
    // static void _0x51_RetSupportedEventNotification(esPod *esp);
    // static void _0x66_RetNowPlayingApplicationBundleName(esPod *esp);
    // static void _0x68_RetLocalizationInfo(esPod *esp);
    // static void _0x6A_WiFiConnectionInfo(esPod *esp);
};
