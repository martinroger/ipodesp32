#include "Arduino.h"
#include "L0x03.h"
#include "esPod.h"

void L0x03::processLingo(esPod *esp, const byte *byteArray, uint32_t len)
{
    byte cmdID = byteArray[0];
    // byte category;
    uint32_t currentEQProfileIndex, startIndex, counts, tempTrackIndex;
    switch (cmdID)
    {
    case L0x03_GetCurrentEQProfileIndex:
    {
        ESP_LOGI(__func__, "CMD: 0x%02x GetCurrentEQProfileIndex", cmdID);
        L0x03::_0x02_RetCurrentEQProfileIndex(esp);
    }
    break;

    case L0x03_SetCurrentEQProfileIndex:
    {
        currentEQProfileIndex = swap_endian<uint32_t>(*((uint32_t *)&byteArray[1]));
        ESP_LOGI(__func__, "CMD: 0x%02x SetCurrentEQProfileIndex 0x%02x", cmdID, currentEQProfileIndex);
        L0x03::_0x00_iPodAck(esp, iPodAck_OK, cmdID);
    }
    break;

    case L0x03_GetNumEQProfiles:
    {
        ESP_LOGI(__func__, "CMD: 0x%02x GetNumEQProfiles", cmdID);
        L0x03::_0x05_RetNumEQProfiles(esp);
    }
    break;

    case L0x03_GetIndexedEQProfileName:
    {
        ESP_LOGI(__func__, "CMD: 0x%02x GetIndexedEQProfileName", cmdID);
        L0x03::_0x07_RetIndexedEQProfileName(esp);
    }
    break;

    case L0x03_SetRemoteEventNotification:
    {
        ESP_LOGI(__func__, "CMD: 0x%02x SetRemoteEventNotification", cmdID);
        L0x03::_0x00_iPodAck(esp, iPodAck_OK, cmdID);
        // Not really implemented, should not be used
        L0x03::_0x09_RemoteEventNotification(esp);
    }
    break;

    case L0x03_GetRemoteEventStatus:
    {
        ESP_LOGI(__func__, "CMD: 0x%02x GetRemoteEventStatus", cmdID);
        // Not implemented
        L0x03::_0x0B_RetRemoteEventStatus(esp, 0);
    }
    break;

    case L0x03_GetiPodStateInfo:
    {
        ESP_LOGI(__func__, "CMD: 0x%02x GetiPodStateInfo", cmdID);
        // Not implemented
        L0x03::_0x0D_RetiPodStateInfo(esp);
    }
    break;

    case L0x03_SetiPodStateInfo:
    {
        ESP_LOGI(__func__, "CMD: 0x%02x SetiPodStateInfo", cmdID);
        // Not implemented
        L0x03::_0x00_iPodAck(esp, iPodAck_OK, cmdID);
    }
    break;

    case L0x03_GetPlayStatus:
    {
        ESP_LOGI(__func__, "CMD: 0x%02x GetPlayStatus", cmdID);
        L0x03::_0x10_RetPlayStatus(esp, esp->playStatus, esp->currentTrackIndex, esp->trackDuration, esp->playPosition);
    }
    break;

    case L0x03_SetCurrentPlayingTrack:
    {
        tempTrackIndex = swap_endian<uint32_t>(*((uint32_t *)&byteArray[1]));
        ESP_LOGI(__func__, "CMD: 0x%02x SetCurrentPlayingTrack index %d", cmdID, tempTrackIndex);
        if (esp->playStatus != PB_STATE_PLAYING)
        {
            esp->playStatus = PB_STATE_PLAYING; // Playing status forced
            if (esp->_playStatusHandler)
            {
                esp->_playStatusHandler(A2DP_PLAY); // Send play to the a2dp
            }
        }
        if (tempTrackIndex == esp->trackList[(esp->trackListPosition + TOTAL_NUM_TRACKS - 1) % TOTAL_NUM_TRACKS]) // Desired trackIndex is the left entry
        {
            // This is only for when the system requires the data of the previously active track
            esp->prevTrackIndex = esp->currentTrackIndex;
            strcpy(esp->prevAlbumName, esp->albumName);
            strcpy(esp->prevArtistName, esp->artistName);
            strcpy(esp->prevTrackTitle, esp->trackTitle);
            esp->prevTrackDuration = esp->trackDuration;

            // Cursor operations for PREV
            esp->trackListPosition = (esp->trackListPosition + TOTAL_NUM_TRACKS - 1) % TOTAL_NUM_TRACKS; // Shift esp->trackListPosition one to the right
            esp->currentTrackIndex = tempTrackIndex;

            // Engage the pending ACK for expected metadata
            esp->trackChangeAckPending = cmdID;
            esp->trackChangeTimestamp = millis();
            ESP_LOGD(__func__, "Prev. index %d New index %d Tracklist pos. %d Pending Meta %d Timestamp: %d --> PREV ", esp->prevTrackIndex, esp->currentTrackIndex, esp->trackListPosition, (esp->trackChangeAckPending > 0x00), esp->trackChangeTimestamp);
            L0x03::_0x00_iPodAck(esp, iPodAck_CmdPending, cmdID, TRACK_CHANGE_TIMEOUT);

            // Fire the A2DP when ready
            if (esp->_playStatusHandler)
                esp->_playStatusHandler(A2DP_PREV); // Fire the metadata trigger indirectly
        }
        else if (tempTrackIndex == esp->currentTrackIndex) // Somehow reselecting the current track
        {
            ESP_LOGD(__func__, "Selected same track as current: %d", tempTrackIndex);
            L0x03::_0x00_iPodAck(esp, iPodAck_OK, cmdID);

            // Fire the A2DP when ready
            if (esp->_playStatusHandler)
                esp->_playStatusHandler(A2DP_PREV); // Fire the metadata trigger indirectly
        }
        else // If it is not the previous or the current track, it automatically becomes a next track
        {
            // This is only for when the system requires the data of the previously active track
            esp->prevTrackIndex = esp->currentTrackIndex;
            strcpy(esp->prevAlbumName, esp->albumName);
            strcpy(esp->prevArtistName, esp->artistName);
            strcpy(esp->prevTrackTitle, esp->trackTitle);
            esp->prevTrackDuration = esp->trackDuration;

            // Cursor operations for NEXT
            esp->trackListPosition = (esp->trackListPosition + 1) % TOTAL_NUM_TRACKS;
            esp->trackList[esp->trackListPosition] = tempTrackIndex;
            esp->currentTrackIndex = tempTrackIndex;

            // Engage the pending ACK for expected metadata
            esp->trackChangeAckPending = cmdID;
            esp->trackChangeTimestamp = millis();
            ESP_LOGD(__func__, "Prev. index %d New index %d Tracklist pos. %d Pending Meta %d Timestamp: %d --> NEXT ", esp->prevTrackIndex, esp->currentTrackIndex, esp->trackListPosition, (esp->trackChangeAckPending > 0x00), esp->trackChangeTimestamp);
            L0x03::_0x00_iPodAck(esp, iPodAck_CmdPending, cmdID, TRACK_CHANGE_TIMEOUT);

            // Fire the A2DP when ready
            if (esp->_playStatusHandler)
                esp->_playStatusHandler(A2DP_NEXT); // Fire the metadata trigger indirectly
        }
    }
    break;

    case L0x03_GetIndexedPlayingTrackInfo:
    {
        tempTrackIndex = swap_endian<uint32_t>(*((uint32_t *)&byteArray[2]));
        switch (byteArray[1]) // Switch on the type of track info requested (careful with overloads)
        {
        case 0x00: // General track Capabilities and Information
            ESP_LOGI(__func__, "CMD 0x%02x GetIndexedPlayingTrackInfo 0x%02x for index %d (previous %d) : Duration", cmdID, byteArray[1], tempTrackIndex, esp->prevTrackIndex);
            if (tempTrackIndex == esp->prevTrackIndex)
            {
                L0x03::_0x13_RetIndexedPlayingTrackInfo(esp, (uint32_t)esp->prevTrackDuration);
            }
            else
            {
                L0x03::_0x13_RetIndexedPlayingTrackInfo(esp, (uint32_t)esp->trackDuration);
            }
            break;

        case 0x02: // Artist Name
            ESP_LOGI(__func__, "CMD 0x%02x GetIndexedPlayingTrackInfo 0x%02x for index %d (previous %d) : Artist", cmdID, byteArray[1], tempTrackIndex, esp->prevTrackIndex);
            if (tempTrackIndex == esp->prevTrackIndex)
            {
                L0x03::_0x13_RetIndexedPlayingTrackInfo(esp, byteArray[1], esp->prevArtistName);
            }
            else
            {
                L0x03::_0x13_RetIndexedPlayingTrackInfo(esp, byteArray[1], esp->artistName);
            }
            break;

        case 0x03: // Album Name
            ESP_LOGI(__func__, "CMD 0x%02x GetIndexedPlayingTrackInfo 0x%02x for index %d (previous %d) : Album", cmdID, byteArray[1], tempTrackIndex, esp->prevTrackIndex);
            if (tempTrackIndex == esp->prevTrackIndex)
            {
                L0x03::_0x13_RetIndexedPlayingTrackInfo(esp, byteArray[1], esp->prevAlbumName);
            }
            else
            {
                L0x03::_0x13_RetIndexedPlayingTrackInfo(esp, byteArray[1], esp->albumName);
            }
            break;

        case 0x04: // Track Genre
            ESP_LOGI(__func__, "CMD 0x%02x GetIndexedPlayingTrackInfo 0x%02x for index %d (previous %d) : Genre", cmdID, byteArray[1], tempTrackIndex, esp->prevTrackIndex);
            L0x03::_0x13_RetIndexedPlayingTrackInfo(esp, byteArray[1], esp->trackGenre);
            break;
        case 0x05: // Track Title
            ESP_LOGI(__func__, "CMD 0x%02x GetIndexedPlayingTrackInfo 0x%02x for index %d (previous %d) : Title", cmdID, byteArray[1], tempTrackIndex, esp->prevTrackIndex);
            if (tempTrackIndex == esp->prevTrackIndex)
            {
                L0x03::_0x13_RetIndexedPlayingTrackInfo(esp, byteArray[1], esp->prevTrackTitle);
            }
            else
            {
                L0x03::_0x13_RetIndexedPlayingTrackInfo(esp, byteArray[1], esp->trackTitle);
            }
            break;
        case 0x06: // Track Composer
            ESP_LOGI(__func__, "CMD 0x%042 GetIndexedPlayingTrackInfo 0x%02x for index %d (previous %d) : Composer", cmdID, byteArray[1], tempTrackIndex, esp->prevTrackIndex);
            L0x03::_0x13_RetIndexedPlayingTrackInfo(esp, byteArray[1], esp->composer);
            break;
        default: // In case the request is beyond the track capabilities
            ESP_LOGW(__func__, "CMD 0x%02x GetIndexedPlayingTrackInfo 0x%02x for index %d (previous %d) : Type not recognised!", cmdID, byteArray[1], tempTrackIndex, esp->prevTrackIndex);
            L0x03::_0x00_iPodAck(esp, iPodAck_BadParam, cmdID);
            break;
        }
    }
    break;

    case L0x03_GetNumPlayingTracks:
    {
        ESP_LOGI(__func__, "CMD: 0x%02x GetNumPlayingTracks", cmdID);
        L0x03::_0x15_RetNumPlayingTracks(esp, TOTAL_NUM_TRACKS);
    }
    break;

    case L0x03_GetArtworkFormats:
    {
        ESP_LOGI(__func__, "CMD: 0x%02x GetArtworkFormats", cmdID);
        L0x03::_0x17_RetArtworkFormats(esp);
    }
    break;

    case L0x03_GetTrackArtworkData:
    {
        ESP_LOGI(__func__, "CMD: 0x%02x GetTrackArtworkData", cmdID);
        L0x03::_0x19_RetTrackArtworkData(esp);
    }
    break;

    case L0x03_GetPowerBatteryState:
    {
        ESP_LOGI(__func__, "CMD: 0x%02x GetPowerBatteryState", cmdID);
        L0x03::_0x1B_RetPowerBatteryState(esp, 0x05);
    }
    break;

    case L0x03_GetSoundCheckState:
    {
        ESP_LOGI(__func__, "CMD: 0x%02x GetSoundCheckState", cmdID);
        L0x03::_0x1D_RetSoundCheckState(esp, 0x00);
    }
    break;

    case L0x03_SetSoundCheckState:
    {
        ESP_LOGI(__func__, "CMD: 0x%02x SetSoundCheckState", cmdID);
        L0x03::_0x00_iPodAck(esp, iPodAck_OK, cmdID);
    }
    break;

    case L0x03_GetTrackArtworkTimes:
    {
        ESP_LOGI(__func__, "CMD: 0x%02x GetTrackArtworkTimes", cmdID);
        L0x03::_0x20_RetTrackArtworkTimes(esp);
    }
    break;

    case L0x03_CreateGeniusPlaylist:
    {
        ESP_LOGI(__func__, "CMD: 0x%02x CreateGeniusPlaylist", cmdID);
        L0x03::_0x00_iPodAck(esp, iPodAck_SelNotGenius, cmdID);
    }
    break;

    case L0x03_IsGeniusAvailableForTrack:
    {
        ESP_LOGI(__func__, "CMD: 0x%02x IsGeniusAvailableForTrack", cmdID);
        L0x03::_0x00_iPodAck(esp, iPodAck_SelNotGenius, cmdID);
    }
    break;

    default:
    {
        ESP_LOGW(__func__, "CMD 0x%02x not recognized.", cmdID);
        L0x03::_0x00_iPodAck(esp, iPodAck_CmdFailed, cmdID);
    }
    break;
        }
}

void L0x03::_0x00_iPodAck(esPod *esp, IPOD_ACK_CODE ackCode, byte cmdID)
{
    ESP_LOGI(__func__, "Ack 0x%02x to command 0x%02x", ackCode, cmdID);

    const byte txPacket[] = {
        0x03,
        0x00,
        ackCode,
        cmdID};
    // Stop the timer if the same command is acknowledged before the elapsed time
    if (cmdID == esp->_pendingCmdId_0x03) // If the pending command is the one being acknowledged
    {
        stopTimer(esp->_pendingTimer_0x03);
        esp->_pendingCmdId_0x03 = 0x00;
        esp->_queuePacketToFront(txPacket, sizeof(txPacket)); // Jump the queue
    }
    else
        esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x03::_0x00_iPodAck(esPod *esp, IPOD_ACK_CODE ackCode, byte cmdID, uint32_t numField)
{
    ESP_LOGI(__func__, "Ack 0x%02x to command 0x%02x Numfield: %d", ackCode, cmdID, numField);
    const byte txPacket[8] = {
        0x03,
        0x00,
        ackCode,
        cmdID};
    *((uint32_t *)&txPacket[4]) = swap_endian<uint32_t>(numField);
    esp->_queuePacket(txPacket, sizeof(txPacket));
    // Starting delayed timer for the iPodAck
    esp->_pendingCmdId_0x03 = cmdID;
    startTimer(esp->_pendingTimer_0x03, numField);
}

void L0x03::_0x02_RetCurrentEQProfileIndex(esPod *esp)
{
    ESP_LOGI(__func__, "Return EQ Profile Index 0x00 0x00 0x00 0x00");

    const byte txPacket[] = {
        0x03,
        0x02,
        0x00, 0x00, 0x00, 0x00};
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x03::_0x05_RetNumEQProfiles(esPod *esp)
{
    ESP_LOGI(__func__, "Return Num Profiles : 1");

    const byte txPacket[] = {
        0x03,
        0x05,
        0x00, 0x00, 0x00, 0x00};
    *((uint32_t *)&txPacket[2]) = swap_endian<uint32_t>(1);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x03::_0x07_RetIndexedEQProfileName(esPod *esp)
{
    const char *EQProfileName = "Base EQ";
    ESP_LOGI(__func__, "Return EQ name : %s", EQProfileName);
    byte txPacket[255] = {
        0x03,
        0x07};
    strcpy((char *)&txPacket[2], EQProfileName);
    esp->_queuePacket(txPacket, 3 + strlen(EQProfileName));
}

void L0x03::_0x09_RemoteEventNotification(esPod *esp)
{
    ESP_LOGW(__func__, "RemoteEventNotificiation not implemented");
}

void L0x03::_0x0B_RetRemoteEventStatus(esPod *esp, uint32_t remEventStatus)
{
    ESP_LOGW(__func__, "RetRemoveEventStatus not implemented");
}

void L0x03::_0x0D_RetiPodStateInfo(esPod *esp)
{
    ESP_LOGW(__func__, "RetiPodStateInfo not implemented");
}

void L0x03::_0x10_RetPlayStatus(esPod *esp, byte playState, uint32_t trackIndex, uint32_t trackTotMs, uint32_t trackPosMs)
{
    ESP_LOGI(__func__, "Play status 0x%02x of index %d at pos. %d / %d ms", playState, trackIndex, trackPosMs, trackTotMs);
    byte txPacket[] = {
        0x03,
        0x10,
        playState,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00};
    *((uint32_t *)&txPacket[3]) = swap_endian<uint32_t>(trackIndex);
    *((uint32_t *)&txPacket[7]) = swap_endian<uint32_t>(trackTotMs);
    *((uint32_t *)&txPacket[11]) = swap_endian<uint32_t>(trackPosMs);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x03::_0x13_RetIndexedPlayingTrackInfo(esPod *esp, byte trackInfoType, char *trackInfoChars)
{
    ESP_LOGI(__func__, "Req'd track info type: 0x%02x", trackInfoType);
    byte txPacket[255] = {
        0x03,
        0x13,
        trackInfoType};
    strcpy((char *)&txPacket[3], trackInfoChars);
    esp->_queuePacket(txPacket, 3 + strlen(trackInfoChars) + 1);
}

void L0x03::_0x13_RetIndexedPlayingTrackInfo(esPod *esp, uint32_t trackDuration_ms)
{
    ESP_LOGI(__func__, "Track duration: %d", trackDuration_ms);
    byte txPacket[13] = {
        0x03,
        0x13,
        0x00,                   // Info type
        0x00, 0x00, 0x00, 0x00, // This says it does not have artwork etc
        0x00, 0x00, 0x00, 0x01, // Track length in ms
        0x00, 0x00              // Chapter count (none)
    };
    *((uint32_t *)&txPacket[7]) = swap_endian<uint32_t>(trackDuration_ms);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x03::_0x15_RetNumPlayingTracks(esPod *esp, uint32_t numPlayingTracks)
{
    ESP_LOGI(__func__, "Playing tracks: %d", numPlayingTracks);
    byte txPacket[] = {
        0x03,
        0x15,
        0x00, 0x00, 0x00, 0x00};
    *((uint32_t *)&txPacket[2]) = swap_endian<uint32_t>(numPlayingTracks);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x03::_0x17_RetArtworkFormats(esPod *esp)
{
    ESP_LOGW(__func__, "RetArtworkFormats not implemented");
}

void L0x03::_0x19_RetTrackArtworkData(esPod *esp)
{
    ESP_LOGW(__func__, "RetTrackArtworkData not implemented");
}

void L0x03::_0x1B_RetPowerBatteryState(esPod *esp, byte powerBatteryState)
{
    ESP_LOGI(__func__, "RetPowerBatteryState 0x%02x", powerBatteryState);
    /*
    0x00 -> Internal battery power, low power
    0x01 -> Internal battery power
    0x02 -> External power, battery pack, no charging
    0x03 -> External power, no charging
    0x04 -> External power, battery charging
    0x05 -> External power, battery charged
    */
    byte batteryLevel = 0x00;
    switch (powerBatteryState)
    {
    case 0x00:
        batteryLevel = (29 / 100) * 255;
        break;
    case 0x01:
        batteryLevel = 255 / 2;
        break;
    default:
        batteryLevel = 0xFF;
        break;
    }

    byte txPacket[] = {
        0x03,
        0x1B,
        powerBatteryState,
        batteryLevel};
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x03::_0x1D_RetSoundCheckState(esPod *esp, byte soundCheckState)
{
    // Might need to introduce and internal variable for the sound check state here
    ESP_LOGI(__func__, "RetSoundCheckState  0x%02x", soundCheckState);
    byte txPacket[] = {
        0x03,
        0x1D,
        soundCheckState};
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x03::_0x20_RetTrackArtworkTimes(esPod *esp)
{
    ESP_LOGW(__func__, "RetTrackArtworkTimes not implemented");
}
