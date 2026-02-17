#include "Arduino.h"
#include "L0x04.h"
#include "esPod.h"

/// @brief
/// @param esp Pointer to the esPod instance
/// @param byteArray
/// @param len
void L0x04::processLingo(esPod *esp, const byte *byteArray, uint32_t len)
{
    byte cmdID = byteArray[1];
    // Initialising handlers to understand what is happening in some parts of the switch. They cannot be initialised in the switch-case scope
    byte category;
    uint32_t startIndex, counts, tempTrackIndex;
    char noCat[25] = "--";

    if (!esp->extendedInterfaceModeActive)
    { // Complain if not in extended interface mode
        ESP_LOGW(__func__, "CMD 0x%04x not executed : Not in extendedInterfaceMode!", cmdID);
        L0x04::_0x01_iPodAck(esp, iPodAck_BadParam, cmdID);
    }
    // Good to go if in Extended Interface mode
    else
    {
        switch (cmdID) // Reminder : we are technically switching on byteArray[1] now
        {
        case L0x04_GetIndexedPlayingTrackInfo:
        {
            tempTrackIndex = swap_endian<uint32_t>(*((uint32_t *)&byteArray[3]));
            switch (byteArray[2]) // Switch on the type of track info requested (careful with overloads)
            {
            case 0x00: // General track Capabilities and Information
                ESP_LOGI(__func__, "CMD 0x%04x GetIndexedPlayingTrackInfo 0x%02x for index %d (previous %d) : Duration", cmdID, byteArray[2], tempTrackIndex, esp->prevTrackIndex);
                if (tempTrackIndex == esp->prevTrackIndex)
                {
                    L0x04::_0x0D_ReturnIndexedPlayingTrackInfo(esp, (uint32_t)esp->prevTrackDuration);
                }
                else
                {
                    L0x04::_0x0D_ReturnIndexedPlayingTrackInfo(esp, (uint32_t)esp->trackDuration);
                }
                break;
            case 0x02: // Track Release Date (fictional)
                ESP_LOGI(__func__, "CMD 0x%04x GetIndexedPlayingTrackInfo 0x%02x for index %d (previous %d) : Release date", cmdID, byteArray[2], tempTrackIndex, esp->prevTrackIndex);
                L0x04::_0x0D_ReturnIndexedPlayingTrackInfo(esp, byteArray[2], (uint16_t)2001);
                break;
            case 0x01: // Track Title
                ESP_LOGI(__func__, "CMD 0x%04x GetIndexedPlayingTrackInfo 0x%02x for index %d (previous %d) : Title", cmdID, byteArray[2], tempTrackIndex, esp->prevTrackIndex);
                if (tempTrackIndex == esp->prevTrackIndex)
                {
                    L0x04::_0x0D_ReturnIndexedPlayingTrackInfo(esp, byteArray[2], esp->prevTrackTitle);
                }
                else
                {
                    L0x04::_0x0D_ReturnIndexedPlayingTrackInfo(esp, byteArray[2], esp->trackTitle);
                }
                break;
            case 0x05: // Track Genre
                ESP_LOGI(__func__, "CMD 0x%04x GetIndexedPlayingTrackInfo 0x%02x for index %d (previous %d) : Genre", cmdID, byteArray[2], tempTrackIndex, esp->prevTrackIndex);
                L0x04::_0x0D_ReturnIndexedPlayingTrackInfo(esp, byteArray[2], esp->trackGenre);
                break;
            case 0x06: // Track Composer
                ESP_LOGI(__func__, "CMD 0x%04x GetIndexedPlayingTrackInfo 0x%02x for index %d (previous %d) : Composer", cmdID, byteArray[2], tempTrackIndex, esp->prevTrackIndex);
                L0x04::_0x0D_ReturnIndexedPlayingTrackInfo(esp, byteArray[2], esp->composer);
                break;
            default: // In case the request is beyond the track capabilities
                ESP_LOGW(__func__, "CMD 0x%04x GetIndexedPlayingTrackInfo 0x%02x for index %d (previous %d) : Type not recognised!", cmdID, byteArray[2], tempTrackIndex, esp->prevTrackIndex);
                L0x04::_0x01_iPodAck(esp, iPodAck_BadParam, cmdID);
                break;
            }
        }
        break;

        case L0x04_RequestProtocolVersion: // Hardcoded return for L0x04
        {
            ESP_LOGI(__func__, "CMD 0x%04x RequestProtocolVersion", cmdID);
            L0x04::_0x13_ReturnProtocolVersion(esp); // Potentially should use L0x00_0x10 instead ? L0x00_0x10_ReturnLingoProtocolVersion(byteArray[2]);
            // L0x00_0x27_GetAccessoryInfo(0x00); // Attempting to start normal handshake
        }
        break;

        case L0x04_ResetDBSelection: // Not sure what to do here. Reset Current Track Index ?
        {
            ESP_LOGI(__func__, "CMD 0x%04x ResetDBSelection", cmdID);
            L0x04::_0x01_iPodAck(esp, iPodAck_OK, cmdID);
        }
        break;

        case L0x04_SelectDBRecord: // Used for browsing ?
        {
            ESP_LOGI(__func__, "CMD 0x%04x SelectDBRecord", cmdID);
            L0x04::_0x01_iPodAck(esp, iPodAck_OK, cmdID);
        }
        break;

        case L0x04_GetNumberCategorizedDBRecords: // Mini requests the number of records for a specific DB_CAT
        {
            category = byteArray[2];
            ESP_LOGI(__func__, "CMD 0x%04x GetNumberCategorizedDBRecords category: 0x%02x", cmdID, category);
            if (category == DB_CAT_TRACK)
            {
                L0x04::_0x19_ReturnNumberCategorizedDBRecords(esp, esp->totalNumberTracks); // Say there are fixed, large amount of tracks
            }
            else
            { // And only one of anything else (Playlist, album, artist etc...)
                L0x04::_0x19_ReturnNumberCategorizedDBRecords(esp, 1);
            }
        }
        break;

        case L0x04_RetrieveCategorizedDatabaseRecords: // Loops through the desired records for a given DB_CAT
        {
            category = byteArray[2]; // DBCat
            startIndex = swap_endian<uint32_t>(*(uint32_t *)&byteArray[3]);
            counts = swap_endian<uint32_t>(*(uint32_t *)&byteArray[7]);

            ESP_LOGI(__func__, "CMD 0x%04x RetrieveCategorizedDatabaseRecords category: 0x%02x from %d for %d counts", cmdID, category, startIndex, counts);
            switch (category)
            {
            case DB_CAT_PLAYLIST:
                for (uint32_t i = startIndex; i < startIndex + counts; i++)
                {
                    L0x04::_0x1B_ReturnCategorizedDatabaseRecord(esp, i, esp->playList);
                }
                break;
            case DB_CAT_ARTIST:
                for (uint32_t i = startIndex; i < startIndex + counts; i++)
                {
                    L0x04::_0x1B_ReturnCategorizedDatabaseRecord(esp, i, esp->artistName);
                }
                break;
            case DB_CAT_ALBUM:
                for (uint32_t i = startIndex; i < startIndex + counts; i++)
                {
                    L0x04::_0x1B_ReturnCategorizedDatabaseRecord(esp, i, esp->albumName);
                }
                break;
            case DB_CAT_GENRE:
                for (uint32_t i = startIndex; i < startIndex + counts; i++)
                {
                    L0x04::_0x1B_ReturnCategorizedDatabaseRecord(esp, i, esp->trackGenre);
                }
                break;
            case DB_CAT_TRACK: // Will sometimes return twice
                for (uint32_t i = startIndex; i < startIndex + counts; i++)
                {
                    L0x04::_0x1B_ReturnCategorizedDatabaseRecord(esp, i, esp->trackTitle);
                }
                break;
            case DB_CAT_COMPOSER:
                for (uint32_t i = startIndex; i < startIndex + counts; i++)
                {
                    L0x04::_0x1B_ReturnCategorizedDatabaseRecord(esp, i, esp->composer);
                }
                break;
            case DB_CAT_AUDIOBOOK:
                for (uint32_t i = startIndex; i < startIndex + counts; i++)
                {
                    L0x04::_0x1B_ReturnCategorizedDatabaseRecord(esp, i, noCat);
                }
                break;
            case DB_CAT_PODCAST:
                for (uint32_t i = startIndex; i < startIndex + counts; i++)
                {
                    L0x04::_0x1B_ReturnCategorizedDatabaseRecord(esp, i, noCat);
                }
                break;
            default:
                ESP_LOGW(__func__, "CMD 0x%04x RetrieveCategorizedDatabaseRecords category: 0x%02x not recognised", cmdID, category);
                L0x04::_0x01_iPodAck(esp, iPodAck_BadParam, cmdID);
                break;
            }
        }
        break;

        case L0x04_GetPlayStatus: // Returns the current esp->playStatus and the position/duration of the current track
        {
            ESP_LOGI(__func__, "CMD 0x%04x GetPlayStatus", cmdID);
            L0x04::_0x1D_ReturnPlayStatus(esp, esp->playPosition, esp->trackDuration, esp->playStatus);
        }
        break;

        case L0x04_GetCurrentPlayingTrackIndex: // Get the uint32 index of the currently playing song
        {
            ESP_LOGI(__func__, "CMD 0x%04x GetCurrentPlayingTrackIndex", cmdID);
            L0x04::_0x1F_ReturnCurrentPlayingTrackIndex(esp, esp->currentTrackIndex);
        }
        break;

        case L0x04_GetIndexedPlayingTrackTitle:
        {
            tempTrackIndex = swap_endian<uint32_t>(*((uint32_t *)&byteArray[2]));
            ESP_LOGI(__func__, "CMD 0x%04x GetIndexedPlayingTrackTitle for index %d (previous %d)", cmdID, tempTrackIndex, esp->prevTrackIndex);
            if (tempTrackIndex == esp->prevTrackIndex)
            {
                L0x04::_0x21_ReturnIndexedPlayingTrackTitle(esp, esp->prevTrackTitle);
            }
            else
            {
                L0x04::_0x21_ReturnIndexedPlayingTrackTitle(esp, esp->trackTitle);
            }
        }
        break;

        case L0x04_GetIndexedPlayingTrackArtistName:
        {
            tempTrackIndex = swap_endian<uint32_t>(*((uint32_t *)&byteArray[2]));

            ESP_LOGI(__func__, "CMD 0x%04x GetIndexedPlayingTrackArtistName for index %d (previous %d)", cmdID, tempTrackIndex, esp->prevTrackIndex);
            if (tempTrackIndex == esp->prevTrackIndex)
            {
                L0x04::_0x23_ReturnIndexedPlayingTrackArtistName(esp, esp->prevArtistName);
            }
            else
            {
                L0x04::_0x23_ReturnIndexedPlayingTrackArtistName(esp, esp->artistName);
            }
        }
        break;

        case L0x04_GetIndexedPlayingTrackAlbumName:
        {
            tempTrackIndex = swap_endian<uint32_t>(*((uint32_t *)&byteArray[2]));
            ESP_LOGI(__func__, "CMD 0x%04x GetIndexedPlayingTrackAlbumName for index %d (previous %d)", cmdID, tempTrackIndex, esp->prevTrackIndex);
            if (tempTrackIndex == esp->prevTrackIndex)
            {
                L0x04::_0x25_ReturnIndexedPlayingTrackAlbumName(esp, esp->prevAlbumName);
            }
            else
            {
                L0x04::_0x25_ReturnIndexedPlayingTrackAlbumName(esp, esp->albumName);
            }
        }
        break;

        case L0x04_SetPlayStatusChangeNotification: // Turns on basic notifications
        {
            esp->playStatusNotificationState = byteArray[2];
            ESP_LOGI(__func__, "CMD 0x%04x SetPlayStatusChangeNotification 0x%02x", cmdID, esp->playStatusNotificationState);
            L0x04::_0x01_iPodAck(esp, iPodAck_OK, cmdID);
        }
        break;

        case L0x04_PlayCurrentSelection: // Used to play a specific index, usually for "next" commands, but may be used to actually jump anywhere
        {
            tempTrackIndex = swap_endian<uint32_t>(*((uint32_t *)&byteArray[2]));
            ESP_LOGI(__func__, "CMD 0x%04x PlayCurrentSelection index %d", cmdID, tempTrackIndex);
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
                L0x04::_0x01_iPodAck(esp, iPodAck_CmdPending, cmdID, TRACK_CHANGE_TIMEOUT);

                // Fire the A2DP when ready
                if (esp->_playStatusHandler)
                    esp->_playStatusHandler(A2DP_PREV); // Fire the metadata trigger indirectly
            }
            else if (tempTrackIndex == esp->currentTrackIndex) // Somehow reselecting the current track
            {
                ESP_LOGD(__func__, "Selected same track as current: %d", tempTrackIndex);
                L0x04::_0x01_iPodAck(esp, iPodAck_OK, cmdID);

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
                L0x04::_0x01_iPodAck(esp, iPodAck_CmdPending, cmdID, TRACK_CHANGE_TIMEOUT);

                // Fire the A2DP when ready
                if (esp->_playStatusHandler)
                    esp->_playStatusHandler(A2DP_NEXT); // Fire the metadata trigger indirectly
            }
        }
        break;

        case L0x04_PlayControl: // Basic play control. Used for Prev, pause and play
        {
            ESP_LOGI(__func__, "CMD 0x%04x PlayControl req: 0x%02x vs esp->playStatus: 0x%02x", cmdID, byteArray[2], esp->playStatus);
            switch (byteArray[2]) // PlayControl byte
            {
            case PB_CMD_TOGGLE: // Just Toggle or start playing
            {
                if (esp->playStatus == PB_STATE_PLAYING)
                {
                    esp->playStatus = PB_STATE_PAUSED; // Toggle to paused if playing
                    if (esp->_playStatusHandler)
                        esp->_playStatusHandler(A2DP_PAUSE);
                }
                else
                {
                    esp->playStatus = PB_STATE_PLAYING; // Switch to playing in any other case
                    if (esp->_playStatusHandler)
                        esp->_playStatusHandler(A2DP_PLAY);
                }
                L0x04::_0x01_iPodAck(esp, iPodAck_OK, cmdID);
            }
            break;
            case PB_CMD_STOP: // Stop
            {
                esp->playStatus = PB_STATE_STOPPED;
                if (esp->_playStatusHandler)
                    esp->_playStatusHandler(A2DP_STOP);
                L0x04::_0x01_iPodAck(esp, iPodAck_OK, cmdID);
            }
            break;
            case PB_CMD_NEXT_TRACK: // Next track.. never seems to happen ?
            {
                // This is only for when the system requires the data of the previously active track
                esp->prevTrackIndex = esp->currentTrackIndex;
                strcpy(esp->prevAlbumName, esp->albumName);
                strcpy(esp->prevArtistName, esp->artistName);
                strcpy(esp->prevTrackTitle, esp->trackTitle);
                esp->prevTrackDuration = esp->trackDuration;

                // Cursor operations for NEXT
                esp->trackListPosition = (esp->trackListPosition + 1) % TOTAL_NUM_TRACKS;
                esp->currentTrackIndex = esp->trackList[esp->trackListPosition];

                // Engage the pending ACK for expected metadata
                esp->trackChangeAckPending = cmdID;
                esp->trackChangeTimestamp = millis();
                ESP_LOGD(__func__, "Prev. index %d New index %d Tracklist pos. %d Pending Meta %d Timestamp: %d --> EXPLICIT NEXT TRACK", esp->prevTrackIndex, esp->currentTrackIndex, esp->trackListPosition, (esp->trackChangeAckPending > 0x00), esp->trackChangeTimestamp);
                L0x04::_0x01_iPodAck(esp, iPodAck_CmdPending, cmdID, TRACK_CHANGE_TIMEOUT);

                // Fire the A2DP when ready
                if (esp->_playStatusHandler)
                    esp->_playStatusHandler(A2DP_NEXT); // Fire the metadata trigger indirectly
            }
            break;
            case PB_CMD_PREVIOUS_TRACK: // Prev track
            {
                ESP_LOGD(__func__, "Current index %d Tracklist pos. %d --> EXPLICIT SINGLE PREV TRACK", esp->currentTrackIndex, esp->trackListPosition);
                L0x04::_0x01_iPodAck(esp, iPodAck_OK, cmdID);

                // Fire the A2DP when ready
                if (esp->_playStatusHandler)
                    esp->_playStatusHandler(A2DP_PREV); // Fire the metadata trigger indirectly
            }
            break;
            case PB_CMD_NEXT: // Next track
            {
                // This is only for when the system requires the data of the previously active track
                esp->prevTrackIndex = esp->currentTrackIndex;
                strcpy(esp->prevAlbumName, esp->albumName);
                strcpy(esp->prevArtistName, esp->artistName);
                strcpy(esp->prevTrackTitle, esp->trackTitle);
                esp->prevTrackDuration = esp->trackDuration;

                // Cursor operations for NEXT
                esp->trackListPosition = (esp->trackListPosition + 1) % TOTAL_NUM_TRACKS;
                esp->currentTrackIndex = esp->trackList[esp->trackListPosition];

                // Engage the pending ACK for expected metadata
                esp->trackChangeAckPending = cmdID;
                esp->trackChangeTimestamp = millis();
                ESP_LOGD(__func__, "Prev. index %d New index %d Tracklist pos. %d Pending Meta %d Timestamp: %d --> EXPLICIT NEXT", esp->prevTrackIndex, esp->currentTrackIndex, esp->trackListPosition, (esp->trackChangeAckPending > 0x00), esp->trackChangeTimestamp);
                L0x04::_0x01_iPodAck(esp, iPodAck_CmdPending, cmdID, TRACK_CHANGE_TIMEOUT);

                // Fire the A2DP when ready
                if (esp->_playStatusHandler)
                    esp->_playStatusHandler(A2DP_NEXT); // Fire the metadata trigger indirectly
            }
            break;
            case PB_CMD_PREV: // Prev track
            {
                ESP_LOGD(__func__, "Current index %d Tracklist pos. %d --> EXPLICIT SINGLE PREV", esp->currentTrackIndex, esp->trackListPosition);
                L0x04::_0x01_iPodAck(esp, iPodAck_OK, cmdID);

                // Fire the A2DP when ready
                if (esp->_playStatusHandler)
                    esp->_playStatusHandler(A2DP_PREV); // Fire the metadata trigger indirectly
            }
            break;
            case PB_CMD_PLAY: // Play... do we need to have an ack pending ?
            {
                esp->playStatus = PB_STATE_PLAYING;

                // Engage the pending ACK for expected metadata
                esp->trackChangeAckPending = cmdID;
                esp->trackChangeTimestamp = millis();
                L0x04::_0x01_iPodAck(esp, iPodAck_CmdPending, cmdID, TRACK_CHANGE_TIMEOUT);

                if (esp->_playStatusHandler)
                    esp->_playStatusHandler(A2DP_PLAY);
            }
            break;
            case PB_CMD_PAUSE: // Pause
            {
                esp->playStatus = PB_STATE_PAUSED;
                L0x04::_0x01_iPodAck(esp, iPodAck_OK, cmdID);
                if (esp->_playStatusHandler)
                    esp->_playStatusHandler(A2DP_PAUSE);
            }
            break;
            }
            if ((esp->playStatus == PB_STATE_STOPPED) && (esp->playStatusNotificationState == NOTIF_ON))
                L0x04::_0x27_PlayStatusNotification(esp, 0x00); // Notify successful stop
        }
        break;

        case L0x04_GetShuffle: // Get Shuffle state from the PB Engine
        {
            ESP_LOGI(__func__, "CMD 0x%04x GetShuffle", cmdID);
            L0x04::_0x2D_ReturnShuffle(esp, esp->shuffleStatus);
        }
        break;

        case L0x04_SetShuffle: // Set Shuffle state
        {
            ESP_LOGI(__func__, "CMD 0x%04x SetShuffle req: 0x%02x vs shuffleStatus: 0x%02x", cmdID, byteArray[2], esp->shuffleStatus);
            esp->shuffleStatus = byteArray[2];
            L0x04::_0x01_iPodAck(esp, iPodAck_OK, cmdID);
        }
        break;

        case L0x04_GetRepeat: // Get Repeat state
        {
            ESP_LOGI(__func__, "CMD 0x%04x GetRepeat", cmdID);
            L0x04::_0x30_ReturnRepeat(esp, esp->repeatStatus);
        }
        break;

        case L0x04_SetRepeat: // Set Repeat state
        {
            ESP_LOGI(__func__, "CMD 0x%04x SetRepeat req: 0x%02x vs repeatStatus: 0x%02x", cmdID, byteArray[2], esp->repeatStatus);
            esp->repeatStatus = byteArray[2];
            L0x04::_0x01_iPodAck(esp, iPodAck_OK, cmdID);
        }
        break;

        case L0x04_GetMonoDisplayImageLimits:
        {
            ESP_LOGI(__func__, "CMD 0x0%04x GetMonoDisplayImageLimits", cmdID);
            L0x04::_0x34_ReturnMonoDisplayImageLimits(esp, 0, 0, 0x01); // Not sure this is OK
        }
        break;

        case L0x04_GetNumPlayingTracks: // Systematically return TOTAL_NUM_TRACKS
        {
            ESP_LOGI(__func__, "CMD 0x%04x GetNumPlayingTracks", cmdID);
            L0x04::_0x36_ReturnNumPlayingTracks(esp, esp->totalNumberTracks);
        }
        break;

        case L0x04_SetCurrentPlayingTrack: // Basically identical to PlayCurrentSelection
        {
            tempTrackIndex = swap_endian<uint32_t>(*((uint32_t *)&byteArray[2]));
            ESP_LOGI(__func__, "CMD 0x%04x SetCurrentPlayingTrack index %d", cmdID, tempTrackIndex);
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
                L0x04::_0x01_iPodAck(esp, iPodAck_CmdPending, cmdID, TRACK_CHANGE_TIMEOUT);

                // Fire the A2DP when ready
                if (esp->_playStatusHandler)
                    esp->_playStatusHandler(A2DP_PREV); // Fire the metadata trigger indirectly
            }
            else if (tempTrackIndex == esp->currentTrackIndex) // Somehow reselecting the current track
            {
                ESP_LOGD(__func__, "Selected same track as current: %d", tempTrackIndex);
                L0x04::_0x01_iPodAck(esp, iPodAck_OK, cmdID);

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
                L0x04::_0x01_iPodAck(esp, iPodAck_CmdPending, cmdID, TRACK_CHANGE_TIMEOUT);

                // Fire the A2DP when ready
                if (esp->_playStatusHandler)
                    esp->_playStatusHandler(A2DP_NEXT); // Fire the metadata trigger indirectly
            }
        }
        break;

        case L0x04_SelectSortDBRecord: // Used for browsing ?
        {
            ESP_LOGI(__func__, "CMD 0x%04x SelectSortDBRecord (deprecated)", cmdID);
            L0x04::_0x01_iPodAck(esp, iPodAck_OK, cmdID);
        }
        break;

        case L0x04_GetColorDisplayImageLimits:
        {
            ESP_LOGI(__func__, "CMD 0x0%04x GetColorDisplayImageLimits", cmdID);
            L0x04::_0x3A_ReturnColorDisplayImageLimits(esp, 0, 0, 0x01); // Not sure this is OK
        }
        break;

        default:
        {
            ESP_LOGW(__func__, "CMD 0x%04x not recognized.", cmdID);
            L0x04::_0x01_iPodAck(esp, iPodAck_CmdFailed, cmdID);
        }
        break;
        }
    }
}

/// @brief General response command for Lingo 0x04
/// @param esp Pointer to the esPod instance
/// @param cmdStatus Has to obey to iPodAck_xxx format as defined in L0x00.h
/// @param cmdID last two ID bytes of the Lingo 0x04 command replied to
void L0x04::_0x01_iPodAck(esPod *esp, IPOD_ACK_CODE ackCode, byte cmdID)
{
    ESP_LOGI(__func__, "Ack 0x%02x to command 0x%04x", ackCode, cmdID);
    // Queue the ack packet
    const byte txPacket[] = {
        0x04,
        0x00, 0x01,
        ackCode,
        0x00, cmdID};
    // Stop the timer if the same command is acknowledged before the elapsed time
    if (cmdID == esp->_pendingCmdId_0x04) // If the pending command is the one being acknowledged
    {
        stopTimer(esp->_pendingTimer_0x04);
        esp->_pendingCmdId_0x04 = 0x00;
        esp->_queuePacketToFront(txPacket, sizeof(txPacket)); // Jump the queue
    }
    else
        esp->_queuePacket(txPacket, sizeof(txPacket)); // Queue at the back
}

/// @brief General response command for Lingo 0x04 with numerical field (used for Ack Pending). Has to be followed up with a normal iPodAck
/// @param esp Pointer to the esPod instance
/// @param cmdStatus Unprotected, but should only be iPodAck_CmdPending
/// @param cmdID Single end-byte ID of the command being acknowledged with Pending
/// @param numField Pending delay in milliseconds
void L0x04::_0x01_iPodAck(esPod *esp, IPOD_ACK_CODE ackCode, byte cmdID, uint32_t numField)
{
    ESP_LOGI(__func__, "Ack 0x%02x to command 0x%04x Numfield: %d", ackCode, cmdID, numField);
    const byte txPacket[20] = {
        0x04,
        0x00, 0x01,
        ackCode,
        cmdID};
    *((uint32_t *)&txPacket[5]) = swap_endian<uint32_t>(numField);
    esp->_queuePacket(txPacket, 5 + 4);
    // Starting delayed timer for the iPodAck
    esp->_pendingCmdId_0x04 = cmdID;
    startTimer(esp->_pendingTimer_0x04, numField);
}

/// @brief Returns the pseudo-UTF8 string for the track info types 01/05/06
/// @param esp Pointer to the esPod instance
/// @param trackInfoType 0x01 : Title / 0x05 : Genre / 0x06 : Composer
/// @param trackInfoChars Character array to pass and package in the tail of the message
void L0x04::_0x0D_ReturnIndexedPlayingTrackInfo(esPod *esp, byte trackInfoType, char *trackInfoChars)
{
    ESP_LOGI(__func__, "Req'd track info type: 0x%02x", trackInfoType);
    byte txPacket[255] = {
        0x04,
        0x00, 0x0D,
        trackInfoType};
    strcpy((char *)&txPacket[4], trackInfoChars);
    esp->_queuePacket(txPacket, 4 + strlen(trackInfoChars) + 1);
}

/// @brief Returns the playing track total duration in milliseconds (Implicit track info 0x00)
/// @param esp Pointer to the esPod instance
/// @param trackDuration_ms trackduration in ms
void L0x04::_0x0D_ReturnIndexedPlayingTrackInfo(esPod *esp, uint32_t trackDuration_ms)
{
    ESP_LOGI(__func__, "Track duration: %d", trackDuration_ms);
    byte txPacket[14] = {
        0x04,
        0x00, 0x0D,
        0x00,
        0x00, 0x00, 0x00, 0x00, // This says it does not have artwork etc
        0x00, 0x00, 0x00, 0x01, // Track length in ms
        0x00, 0x00              // Chapter count (none)
    };
    *((uint32_t *)&txPacket[8]) = swap_endian<uint32_t>(trackDuration_ms);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Overloaded return of the playing track info : release year
/// @param esp Pointer to the esPod instance
/// @param trackInfoType Can only be 0x02
/// @param releaseYear Fictional release year of the song
void L0x04::_0x0D_ReturnIndexedPlayingTrackInfo(esPod *esp, byte trackInfoType, uint16_t releaseYear)
{
    ESP_LOGI(__func__, "Track info: 0x%02x Release Year: %d", trackInfoType, releaseYear);
    byte txPacket[12] = {
        0x04,
        0x00, 0x0D,
        trackInfoType,
        0x00, 0x00, 0x00, 0x01, 0x01, // First of Jan at 00:00:00
        0x00, 0x00,                   // year goes here
        0x01                          // it was a Monday
    };
    *((uint16_t *)&txPacket[9]) = swap_endian<uint16_t>(releaseYear);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Returns the L0x04 Lingo Protocol Version : hardcoded to 1.12, consistent with an iPod Classic 5.5G
/// @param esp Pointer to the esPod instance
void L0x04::_0x13_ReturnProtocolVersion(esPod *esp)
{
    ESP_LOGI(__func__, "Lingo protocol version 1.12");
    byte txPacket[] = {
        0x04,
        0x00, 0x13,
        0x01, 0x0C // Protocol version 1.12
    };
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Return the number of records of a given DBCategory (Playlist, tracks...)
/// @param esp Pointer to the esPod instance
/// @param categoryDBRecords The number of records to return
void L0x04::_0x19_ReturnNumberCategorizedDBRecords(esPod *esp, uint32_t categoryDBRecords)
{
    ESP_LOGI(__func__, "Category DB Records: %d", categoryDBRecords);
    byte txPacket[7] = {
        0x04,
        0x00, 0x19,
        0x00, 0x00, 0x00, 0x00};
    *((uint32_t *)&txPacket[3]) = swap_endian<uint32_t>(categoryDBRecords);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Returns the metadata for a certain database record. Category is implicit
/// @param esp Pointer to the esPod instance
/// @param index Index of the DBRecord
/// @param recordString Metadata to include in the return
void L0x04::_0x1B_ReturnCategorizedDatabaseRecord(esPod *esp, uint32_t index, char *recordString)
{
    ESP_LOGI(__func__, "Database record at index %d : %s", index, recordString);
    byte txPacket[255] = {
        0x04,
        0x00, 0x1B,
        0x00, 0x00, 0x00, 0x00 // Index goes here
    };
    *((uint32_t *)&txPacket[3]) = swap_endian<uint32_t>(index);
    strcpy((char *)&txPacket[7], recordString);
    esp->_queuePacket(txPacket, 7 + strlen(recordString) + 1);
}

/// @brief Returns the current playback status, indicating the position out of the duration
/// @param esp Pointer to the esPod instance
/// @param position Playing position in ms in the track
/// @param duration Total duration of the track in ms
/// @param playStatusArg Playback status (0x00 Stopped, 0x01 Playing, 0x02 Paused, 0xFF Error)
void L0x04::_0x1D_ReturnPlayStatus(esPod *esp, uint32_t position, uint32_t duration, byte playStatusArg)
{
    ESP_LOGI(__func__, "Play status 0x%02x at pos. %d / %d ms", playStatusArg, position, duration);
    byte txPacket[] = {
        0x04,
        0x00, 0x1D,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        playStatusArg};
    *((uint32_t *)&txPacket[3]) = swap_endian<uint32_t>(duration);
    *((uint32_t *)&txPacket[7]) = swap_endian<uint32_t>(position);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Returns the "Index" of the currently playing track, useful for pulling matching metadata
/// @param esp Pointer to the esPod instance
/// @param trackIndex The trackIndex to return. This is different from the position in the tracklist when Shuffle is ON
void L0x04::_0x1F_ReturnCurrentPlayingTrackIndex(esPod *esp, uint32_t trackIndex)
{
    ESP_LOGI(__func__, "Track index: %d", trackIndex);
    byte txPacket[] = {
        0x04,
        0x00, 0x1F,
        0x00, 0x00, 0x00, 0x00};
    *((uint32_t *)&txPacket[3]) = swap_endian<uint32_t>(trackIndex);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Returns the track Title after an implicit call for it
/// @param esp Pointer to the esPod instance
/// @param trackTitle Character array to return
void L0x04::_0x21_ReturnIndexedPlayingTrackTitle(esPod *esp, char *trackTitle)
{
    ESP_LOGI(__func__, "Track title: %s", trackTitle);
    byte txPacket[255] = {
        0x04,
        0x00, 0x21};
    strcpy((char *)&txPacket[3], trackTitle);
    esp->_queuePacket(txPacket, 3 + strlen(trackTitle) + 1);
}

/// @brief Returns the track Artist Name after an implicit call for it
/// @param esp Pointer to the esPod instance
/// @param trackArtistName Character array to return
void L0x04::_0x23_ReturnIndexedPlayingTrackArtistName(esPod *esp, char *trackArtistName)
{
    ESP_LOGI(__func__, "Track artist: %s", trackArtistName);
    byte txPacket[255] = {
        0x04,
        0x00, 0x23};
    strcpy((char *)&txPacket[3], trackArtistName);
    esp->_queuePacket(txPacket, 3 + strlen(trackArtistName) + 1);
}

/// @brief Returns the track Album Name after an implicit call for it
/// @param esp Pointer to the esPod instance
/// @param trackAlbumName Character array to return
void L0x04::_0x25_ReturnIndexedPlayingTrackAlbumName(esPod *esp, char *trackAlbumName)
{
    ESP_LOGI(__func__, "Track album: %s", trackAlbumName);
    byte txPacket[255] = {
        0x04,
        0x00, 0x25};
    strcpy((char *)&txPacket[3], trackAlbumName);
    esp->_queuePacket(txPacket, 3 + strlen(trackAlbumName) + 1);
}

/// @brief Only supports currently two types : 0x00 Playback Stopped, 0x01 Track index, 0x04 Track offset
/// @param esp Pointer to the esPod instance
/// @param notification Notification type that can be returned : 0x01 for track index change, 0x04 for the track offset
/// @param numField For 0x01 this is the new Track index, for 0x04 this is the current Track offset in ms
void L0x04::_0x27_PlayStatusNotification(esPod *esp, byte notification, uint32_t numField)
{
    ESP_LOGI(__func__, "Play status 0x%02x Numfield: %d", notification, numField);
    byte txPacket[] = {
        0x04,
        0x00, 0x27,
        notification,
        0x00, 0x00, 0x00, 0x00};
    *((uint32_t *)&txPacket[4]) = swap_endian<uint32_t>(numField);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Returns the PlayStatusNotification when it is STOPPED (0x00)
/// @param esp Pointer to the esPod instance
/// @param notification Can only be 0x00
void L0x04::_0x27_PlayStatusNotification(esPod *esp, byte notification)
{
    ESP_LOGI(__func__, "Play status 0x%02x STOPPED", notification);
    byte txPacket[] = {
        0x04,
        0x00, 0x27,
        notification};
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Returns the current shuffle status of the PBEngine
/// @param esp Pointer to the esPod instance
/// @param currentShuffleStatus 0x00 No Shuffle, 0x01 Tracks, 0x02 Albums
void L0x04::_0x2D_ReturnShuffle(esPod *esp, byte currentShuffleStatus)
{
    ESP_LOGI(__func__, "Shuffle status: 0x%02x", currentShuffleStatus);
    byte txPacket[] = {
        0x04,
        0x00, 0x2D,
        currentShuffleStatus};
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Returns the current repeat status of the PBEngine
/// @param esp Pointer to the esPod instance
/// @param currentRepeatStatus 0x00 No Repeat, 0x01 One Track, 0x02 All tracks
void L0x04::_0x30_ReturnRepeat(esPod *esp, byte currentRepeatStatus)
{
    ESP_LOGI(__func__, "Repeat status: 0x%02x", currentRepeatStatus);
    byte txPacket[] = {
        0x04,
        0x00, 0x30,
        currentRepeatStatus};
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

/// @brief
/// @param esp Pointer to the esPod instance
/// @param maxImageW
/// @param maxImageH
/// @param dispPixelFmt
void L0x04::_0x34_ReturnMonoDisplayImageLimits(esPod *esp, uint16_t maxImageW, uint16_t maxImageH, byte dispPixelFmt)
{
    ESP_LOGI(__func__, "Return monochrome image limits : %d x %d x %d", maxImageW, maxImageH, dispPixelFmt);
    byte txPacket[] = {
        0x04,
        0x00, 0x34,
        0x00, 0x00,
        0x00, 0x00,
        dispPixelFmt};
    *((uint16_t *)&txPacket[3]) = swap_endian<uint16_t>(maxImageW);
    *((uint16_t *)&txPacket[5]) = swap_endian<uint16_t>(maxImageH);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Returns the number of playing tracks in the current selection (here it is all the tracks)
/// @param esp Pointer to the esPod instance
/// @param numPlayingTracks Number of playing tracks to return
void L0x04::_0x36_ReturnNumPlayingTracks(esPod *esp, uint32_t numPlayingTracks)
{
    ESP_LOGI(__func__, "Playing tracks: %d", numPlayingTracks);
    byte txPacket[] = {
        0x04,
        0x00, 0x36,
        0x00, 0x00, 0x00, 0x00};
    *((uint32_t *)&txPacket[3]) = swap_endian<uint32_t>(numPlayingTracks);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

/// @brief
/// @param esp Pointer to the esPod instance
/// @param maxImageW
/// @param maxImageH
/// @param dispPixelFmt
void L0x04::_0x3A_ReturnColorDisplayImageLimits(esPod *esp, uint16_t maxImageW, uint16_t maxImageH, byte dispPixelFmt)
{
    ESP_LOGI(__func__, "Return color image limits : %d x %d x %d", maxImageW, maxImageH, dispPixelFmt);
    byte txPacket[] = {
        0x04,
        0x00, 0x3A,
        0x00, 0x00,
        0x00, 0x00,
        dispPixelFmt};
    *((uint16_t *)&txPacket[3]) = swap_endian<uint16_t>(maxImageW);
    *((uint16_t *)&txPacket[5]) = swap_endian<uint16_t>(maxImageH);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}
