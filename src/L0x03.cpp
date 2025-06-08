#include "Arduino.h"
#include "L0x03.h"
#include "esPod.h"

void L0x03::processLingo(esPod *esp, const byte *byteArray, uint32_t len)
{
    byte cmdID = byteArray[0];
    // byte category;
    // uint32_t startIndex, counts, tempTrackIndex;
    switch (cmdID)
    {
    case L0x03_GetCurrentEQProfileIndex:
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x GetCurrentEQProfileIndex", cmdID);
    }
    break;
    case L0x03_SetCurrentEQProfileIndex:
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x SetCurrentEQProfileIndex", cmdID);
    }
    break;
    case L0x03_GetNumEQProfiles:
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x GetNumEQProfiles", cmdID);
    }
    break;
    case L0x03_GetIndexedEQProfileName:
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x GetIndexedEQProfileName", cmdID);
    }
    break;
    case L0x03_SetRemoteEventNotification:
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x SetRemoteEventNotification", cmdID);
    }
    break;
    case L0x03_GetRemoteEventStatus:
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x GetRemoteEventStatus", cmdID);
    }
    break;
    case L0x03_GetiPodStateInfo:
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x GetiPodStateInfo", cmdID);
    }
    break;
    case L0x03_SetiPodStateInfo:
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x SetiPodStateInfo", cmdID);
    }
    break;
    case L0x03_GetPlayStatus:
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x GetPlayStatus", cmdID);
    }
    break;
    case L0x03_SetCurrentPlayingTrack:
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x SetCurrentPlayingTrack", cmdID);
    }
    break;
    case L0x03_GetIndexedPlayingTrackInfo:
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x GetIndexedPlayingTrackInfo", cmdID);
    }
    break;
    case L0x03_GetNumPlayingTracks:
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x GetNumPlayingTracks", cmdID);
    }
    break;
    case L0x03_GetArtworkFormats:
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x GetArtworkFormats", cmdID);
    }
    break;
    case L0x03_GetTrackArtworkData:
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x GetTrackArtworkData", cmdID);
    }
    break;
    case L0x03_GetPowerBatteryState:
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x GetPowerBatteryState", cmdID);
    }
    break;
    case L0x03_GetSoundCheckState:
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x GetSoundCheckState", cmdID);
    }
    break;
    case L0x03_SetSoundCheckState:
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x SetSoundCheckState", cmdID);
    }
    break;
    case L0x03_GetTrackArtworkTimes:
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x GetTrackArtworkTimes", cmdID);
    }
    break;
    case L0x03_CreateGeniusPlaylist:
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x CreateGeniusPlaylist", cmdID);
    }
    break;
    case L0x03_IsGeniusAvailableForTrack:
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x IsGeniusAvailableForTrack", cmdID);
    }
    break;
    default:
        break;
    }
}

void L0x03::_0x00_iPodAck(esPod *esp, IPOD_ACK_CODE ackCode, byte cmdID)
{
    ESP_LOGI(IPOD_TAG, "Ack 0x%02x to command 0x%02x", ackCode, cmdID);

    const byte txPacket[] = {
        0x03,
        0x00,
        ackCode,
        cmdID};
        esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x03::_0x02_RetCurrentEQProfileIndex(esPod *esp)
{
}

void L0x03::_0x05_RetNumEQProfiles(esPod *esp)
{
}

void L0x03::_0x07_RetIndexedEQProfileName(esPod *esp)
{
}

void L0x03::_0x09_RemoteEventNotification(esPod *esp)
{
}

void L0x03::_0x0B_RetRemoteEventStatus(esPod *esp)
{
}

void L0x03::_0x0D_RetiPodStateInfo(esPod *esp)
{
}

void L0x03::_0x10_RetPlayStatus(esPod *esp)
{
}

void L0x03::_0x13_RetIndexedPlayingTrackInfo(esPod *esp)
{
}

void L0x03::_0x15_RetNumPlayingTracks(esPod *esp)
{
}

void L0x03::_0x17_RetArtworkFormats(esPod *esp)
{
}

void L0x03::_0x19_RetTrackArtworkData(esPod *esp)
{
}

void L0x03::_0x1B_RetPowerBatteryState(esPod *esp)
{
}

void L0x03::_0x1D_RetSoundCheckState(esPod *esp)
{
}

void L0x03::_0x20_RetTrackArtworkTimes(esPod *esp)
{
}
