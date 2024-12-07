#include "esPod.h"
#ifdef IPOD_TAG
    #undef IPOD_TAG
#endif
#define IPOD_TAG "esPod"

/*Heavily adapted from :
https://github.com/chemicstry/A2DP_iPod
*/

//ESP32 is Little-Endian, iPod is Big-Endian
template <typename T>
T swap_endian(T u)
{
    static_assert (CHAR_BIT == 8, "CHAR_BIT != 8");

    union
    {
        T u;
        unsigned char u8[sizeof(T)];
    } source, dest;

    source.u = u;

    for (size_t k = 0; k < sizeof(T); k++)
        dest.u8[k] = source.u8[sizeof(T) - k - 1];

    return dest.u;
}


//-----------------------------------------------------------------------
//|         Constructor, reset, attachCallback, packet utilities        |
//-----------------------------------------------------------------------
#pragma region UTILS
esPod::esPod(Stream& targetSerial) 
    :
        _targetSerial(targetSerial)
{
}

void esPod::resetState(){

    ESP_LOGW(IPOD_TAG,"esPod resetState called");
    //State variables
    extendedInterfaceModeActive = false;
    lastConnected = millis();
    //disabled = true;  //Not for now, might need to reenable or rethink the concept later

    //metadata variables
    trackDuration = 1;
    prevTrackDuration = 1;
    playPosition = 0;

    //Playback Engine
    playStatus = PB_STATE_PAUSED;
    playStatusNotificationState = NOTIF_OFF;
    trackChangeAckPending = 0x00;
    shuffleStatus = 0x00;
    repeatStatus = 0x02;

    //TrackList variables
    currentTrackIndex = 0;
    prevTrackIndex = TOTAL_NUM_TRACKS-1;
    for (uint16_t i = 0; i < TOTAL_NUM_TRACKS; i++) trackList[i] = 0;
    trackListPosition = 0;

    //Packet-related
    _prevRxByte = 0x00;
    for (uint16_t i = 0; i < 1024; i++) _rxBuf[i] = 0x00;
    _rxLen = 0;
    _rxCounter = 0;
    _rxInProgress = false;

    //Mini metadata
    _accessoryCapabilitiesRequested =   false;
    _accessoryFirmwareRequested     =   false;
    _accessoryHardwareRequested     =   false;
    _accessoryManufRequested        =   false;
    _accessoryModelRequested        =   false;
    _accessoryNameRequested         =   false;
}

void esPod::attachPlayControlHandler(playStatusHandler_t playHandler)
{
    _playStatusHandler = playHandler;
    ESP_LOGD(IPOD_TAG,"PlayControlHandler attached.");
    //Experimental, maybe is doing more harm than good
/*     for (uint32_t i = 0; i < TOTAL_NUM_TRACKS; i++)
    {
        trackList[i] = i;
    }
    trackListPosition = 0;
    currentTrackIndex = trackList[trackListPosition]; */
    
}

//Calculates the checksum of a packet that starts from i=0 ->Lingo to i=len -> Checksum
byte esPod::checksum(const byte* byteArray, uint32_t len)
{
    uint32_t tempChecksum = len;
    for (int i=0;i<len;i++) {
        tempChecksum += byteArray[i];
    }
    tempChecksum = 0x100 -(tempChecksum & 0xFF);
    return (byte)tempChecksum;
}


void esPod::sendPacket(const byte* byteArray, uint32_t len)
{
    uint32_t finalLength = len + 4;
    byte tempBuf[finalLength] = {0x00};
    
    tempBuf[0] = 0xFF;
    tempBuf[1] = 0x55;
    tempBuf[2] = (byte)len;
    for (uint32_t i = 0; i < len; i++)
    {
        tempBuf[3+i] = byteArray[i];
    }
    tempBuf[3+len] = esPod::checksum(byteArray,len);
    
    _targetSerial.write(tempBuf,finalLength);
    // _targetSerial.write(0xFF);
    // _targetSerial.write(0x55);
    // _targetSerial.write((byte)len);
    // _targetSerial.write(byteArray,len);
    // _targetSerial.write(esPod::checksum(byteArray,len));
    // ESP_LOG_BUFFER_HEX_LEVEL(IPOD_TAG,byteArray,len,ESP_LOG_VERBOSE);
}

#pragma endregion

//-----------------------------------------------------------------------
//|                     Lingo 0x00 subfunctions                         |
//-----------------------------------------------------------------------
#pragma region LINGO 0x00

/// @brief General response command for Lingo 0x00
/// @param cmdStatus Has to obey to iPodAck_xxx format as defined in L0x00.h
/// @param cmdID ID (single byte) of the Lingo 0x00 command replied to
void esPod::L0x00_0x02_iPodAck(byte cmdStatus,byte cmdID) {
    ESP_LOGI(IPOD_TAG,"Ack 0x%02x to command 0x%02x",cmdStatus,cmdID);
    const byte txPacket[] = {
        0x00,
        0x02,
        cmdStatus,
        cmdID
    };
    sendPacket(txPacket,sizeof(txPacket));
}

/// @brief General response command for Lingo 0x00 with numerical field (used for Ack Pending). Has to be followed up with a normal iPodAck
/// @param cmdStatus Unprotected, but should only be iPodAck_CmdPending
/// @param cmdID Single byte ID of the command being acknowledged with Pending
/// @param numField Pending delay in milliseconds
void esPod::L0x00_0x02_iPodAck(byte cmdStatus,byte cmdID, uint32_t numField) {
    ESP_LOGI(IPOD_TAG,"Ack 0x%02x to command 0x%02x Numfield: %d",cmdStatus,cmdID,numField);
    const byte txPacket[20] = {
        0x00,
        0x02,
        cmdStatus,
        cmdID
    };
    *((uint32_t*)&txPacket[4]) = swap_endian<uint32_t>(numField);
    sendPacket(txPacket,4+4);
}


/// @brief Returns 0x01 if the iPod is in extendedInterfaceMode, or 0x00 if not
/// @param extendedModeByte Direct value of the extendedInterfaceMode boolean
void esPod::L0x00_0x04_ReturnExtendedInterfaceMode(byte extendedModeByte) {
    ESP_LOGI(IPOD_TAG,"Extended Interface mode: 0x%02x",extendedModeByte);
    const byte txPacket[] = {
        0x00,
        0x04,
        extendedModeByte
    };
    sendPacket(txPacket,sizeof(txPacket));
}

/// @brief Returns as a UTF8 null-ended char array, the _name of the iPod (not changeable during runtime)
void esPod::L0x00_0x08_ReturniPodName() {
    ESP_LOGI(IPOD_TAG,"Name: %s",_name);
    byte txPacket[255] = { //Prealloc to len = FF
        0x00,
        0x08
    };
    strcpy((char*)&txPacket[2],_name);
    sendPacket(txPacket,3+strlen(_name));
}

/// @brief Returns the iPod Software Version
void esPod::L0x00_0x0A_ReturniPodSoftwareVersion() {
    ESP_LOGI(IPOD_TAG,"SW version: %d.%d.%d",_SWMajor,_SWMinor,_SWrevision);
    byte txPacket[] = {
        0x00,
        0x0A,
        (byte)_SWMajor,
        (byte)_SWMinor,
        (byte)_SWrevision
    };
    sendPacket(txPacket,sizeof(txPacket));
}

/// @brief Returns the iPod Serial Number (which is a string)
void esPod::L0x00_0x0C_ReturniPodSerialNum() {
    ESP_LOGI(IPOD_TAG,"Serial number: %s",_serialNumber);
    byte txPacket[255] = { //Prealloc to len = FF
        0x00,
        0x0C
    };
    strcpy((char*)&txPacket[2],_serialNumber);
    sendPacket(txPacket,3+strlen(_serialNumber));
}

/// @brief Returns the iPod model number PA146FD 720901, which corresponds to an iPod 5.5G classic
void esPod::L0x00_0x0E_ReturniPodModelNum() {
    ESP_LOGI(IPOD_TAG,"Model number : PA146FD 720901");
    byte txPacket[] = {
        0x00,
        0x0E,
        0x00,0x0B,0x00,0x05,0x50,0x41,0x31,0x34,0x36,0x46,0x44,0x00
    };
    sendPacket(txPacket,sizeof(txPacket));
}

/// @brief Returns a preprogrammed value for the Lingo Protocol Version for 0x00, 0x03 or 0x04
/// @param targetLingo Target Lingo for which the Protocol Version is desired.
void esPod::L0x00_0x10_ReturnLingoProtocolVersion(byte targetLingo)
{
    byte txPacket[] = {
        0x00, 0x10,
        targetLingo,
        0x01,0x00
    };
    switch (targetLingo)
    {
    case 0x00: //For General Lingo 0x00, version 1.6
        txPacket[4] = 0x06;
        break;
    case 0x03: //For Lingo 0x03, version 1.5
        txPacket[4] = 0x05;
        break;
    case 0x04: //For Lingo 0x04 (Extended Interface), version 1.12
        txPacket[4] = 0x0C;
        break;
    }
    ESP_LOGI(IPOD_TAG,"Lingo 0x%02x protocol version: 1.%d",targetLingo,txPacket[4]);
    sendPacket(txPacket,sizeof(txPacket));
}

/// @brief Query the accessory Info (model number, manufacturer, firmware version ...) using the target Info category hex
/// @param desiredInfo Hex code for the type of information that is desired
void esPod::L0x00_0x27_GetAccessoryInfo(byte desiredInfo)
{
    ESP_LOGI(IPOD_TAG,"Req'd info type: 0x%02x",desiredInfo);
    byte txPacket[] = {
        0x00, 0x27,
        desiredInfo
    };
    sendPacket(txPacket,sizeof(txPacket));
}


#pragma endregion

//-----------------------------------------------------------------------
//|                     Lingo 0x04 subfunctions                         |
//-----------------------------------------------------------------------
#pragma region LINGO 0x04

/// @brief General response command for Lingo 0x04
/// @param cmdStatus Has to obey to iPodAck_xxx format as defined in L0x00.h
/// @param cmdID last two ID bytes of the Lingo 0x04 command replied to
void esPod::L0x04_0x01_iPodAck(byte cmdStatus, byte cmdID)
{
    ESP_LOGI(IPOD_TAG,"Ack 0x%02x to command 0x%04x",cmdStatus,cmdID);
    const byte txPacket[] = {
        0x04,
        0x00,0x01,
        cmdStatus,
        0x00,cmdID
    };
    sendPacket(txPacket,sizeof(txPacket));
}

/// @brief General response command for Lingo 0x04 with numerical field (used for Ack Pending). Has to be followed up with a normal iPodAck
/// @param cmdStatus Unprotected, but should only be iPodAck_CmdPending
/// @param cmdID Single end-byte ID of the command being acknowledged with Pending
/// @param numField Pending delay in milliseconds
void esPod::L0x04_0x01_iPodAck(byte cmdStatus, byte cmdID, uint32_t numField)
{
    ESP_LOGI(IPOD_TAG,"Ack 0x%02x to command 0x%04x Numfield: %d",cmdStatus,cmdID,numField);
    const byte txPacket[20] = {
        0x04,
        0x00,0x01,
        cmdStatus,
        cmdID
    };
    *((uint32_t*)&txPacket[5]) = swap_endian<uint32_t>(numField);
    sendPacket(txPacket,5+4);
}

/// @brief Returns the pseudo-UTF8 string for the track info types 01/05/06
/// @param trackInfoType 0x01 : Title / 0x05 : Genre / 0x06 : Composer
/// @param trackInfoChars Character array to pass and package in the tail of the message
void esPod::L0x04_0x0D_ReturnIndexedPlayingTrackInfo(byte trackInfoType, char* trackInfoChars)
{
    ESP_LOGI(IPOD_TAG,"Req'd track info type: 0x%02x",trackInfoType);
    byte txPacket[255] = {
        0x04,
        0x00,0x0D,
        trackInfoType
    };
    strcpy((char*)&txPacket[4],trackInfoChars);
    sendPacket(txPacket,4+strlen(trackInfoChars)+1);
}

/// @brief Returns the playing track total duration in milliseconds (Implicit track info 0x00)
/// @param trackDuration_ms trackduration in ms
void esPod::L0x04_0x0D_ReturnIndexedPlayingTrackInfo(uint32_t trackDuration_ms)
{
    ESP_LOGI(IPOD_TAG,"Track duration: %d",trackDuration_ms);
    byte txPacket[14] = {
        0x04,
        0x00,0x0D,
        0x00,
        0x00,0x00,0x00,0x00, //This says it does not have artwork etc
        0x00,0x00,0x00,0x01, //Track length in ms
        0x00,0x00 //Chapter count (none)
    };
    *((uint32_t*)&txPacket[8]) = swap_endian<uint32_t>(trackDuration_ms);
    sendPacket(txPacket,sizeof(txPacket));
}

/// @brief Overloaded return of the playing track info : release year
/// @param trackInfoType Can only be 0x02
/// @param releaseYear Fictional release year of the song
void esPod::L0x04_0x0D_ReturnIndexedPlayingTrackInfo(byte trackInfoType, uint16_t releaseYear)
{
    ESP_LOGI(IPOD_TAG,"Track info: 0x%02x Release Year: %d",trackInfoType,releaseYear);
    byte txPacket[12] = {
        0x04,
        0x00,0x0D,
        trackInfoType,
        0x00,0x00,0x00,0x01,0x01, //First of Jan at 00:00:00
        0x00,0x00, //year goes here
        0x01 //it was a Monday
    };
    *((uint16_t*)&txPacket[9]) = swap_endian<uint16_t>(releaseYear);
    sendPacket(txPacket,sizeof(txPacket));
}

/// @brief Returns the L0x04 Lingo Protocol Version : hardcoded to 1.12, consistent with an iPod Classic 5.5G
void esPod::L0x04_0x13_ReturnProtocolVersion()
{
    ESP_LOGI(IPOD_TAG,"Lingo protocol version 1.12");
    byte txPacket[] = {
        0x04,
        0x00,0x13,
        0x01,0x0C //Protocol version 1.12
    };
    sendPacket(txPacket,sizeof(txPacket));
}

/// @brief Return the number of records of a given DBCategory (Playlist, tracks...)
/// @param categoryDBRecords The number of records to return
void esPod::L0x04_0x19_ReturnNumberCategorizedDBRecords(uint32_t categoryDBRecords)
{
    ESP_LOGI(IPOD_TAG,"Category DB Records: %d",categoryDBRecords);
    byte txPacket[7] = {
        0x04,
        0x00,0x19,
        0x00,0x00,0x00,0x00
    };
    *((uint32_t*)&txPacket[3]) = swap_endian<uint32_t>(categoryDBRecords);
    sendPacket(txPacket,sizeof(txPacket));
}

/// @brief Returns the metadata for a certain database record. Category is implicit
/// @param index Index of the DBRecord
/// @param recordString Metadata to include in the return
void esPod::L0x04_0x1B_ReturnCategorizedDatabaseRecord(uint32_t index, char *recordString)
{
    ESP_LOGI(IPOD_TAG,"Database record at index %d : %s",index,recordString);
    byte txPacket[255] = {
        0x04,
        0x00,0x1B,
        0x00,0x00,0x00,0x00 //Index goes here
    };
    *((uint32_t*)&txPacket[3]) = swap_endian<uint32_t>(index);
    strcpy((char*)&txPacket[7],recordString);
    sendPacket(txPacket,7+strlen(recordString)+1);
}

/// @brief Returns the current playback status, indicating the position out of the duration
/// @param position Playing position in ms in the track
/// @param duration Total duration of the track in ms
/// @param playStatusArg Playback status (0x00 Stopped, 0x01 Playing, 0x02 Paused, 0xFF Error)
void esPod::L0x04_0x1D_ReturnPlayStatus(uint32_t position, uint32_t duration, byte playStatusArg)
{
    ESP_LOGI(IPOD_TAG,"Play status 0x%02x at pos. %d / %d ms",playStatusArg,position,duration);
    byte txPacket[] = {
        0x04,
        0x00,0x1D,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        playStatusArg
    };
    *((uint32_t*)&txPacket[3]) = swap_endian<uint32_t>(duration);
    *((uint32_t*)&txPacket[7]) = swap_endian<uint32_t>(position);
    sendPacket(txPacket,sizeof(txPacket));
}

/// @brief Returns the "Index" of the currently playing track, useful for pulling matching metadata
/// @param trackIndex The trackIndex to return. This is different from the position in the tracklist when Shuffle is ON
void esPod::L0x04_0x1F_ReturnCurrentPlayingTrackIndex(uint32_t trackIndex)
{
    ESP_LOGI(IPOD_TAG,"Track index: %d",trackIndex);
    byte txPacket[] = {
        0x04,
        0x00,0x1F,
        0x00,0x00,0x00,0x00
    };
    *((uint32_t*)&txPacket[3]) = swap_endian<uint32_t>(trackIndex);
    sendPacket(txPacket,sizeof(txPacket));
}

/// @brief Returns the track Title after an implicit call for it
/// @param trackTitle Character array to return
void esPod::L0x04_0x21_ReturnIndexedPlayingTrackTitle(char *trackTitle)
{
    ESP_LOGI(IPOD_TAG,"Track title: %s",trackTitle);
    byte txPacket[255] = {
        0x04,
        0x00,0x21
    };
    strcpy((char*)&txPacket[3],trackTitle);
    sendPacket(txPacket,3+strlen(trackTitle)+1);
}

/// @brief Returns the track Artist Name after an implicit call for it
/// @param trackArtistName Character array to return
void esPod::L0x04_0x23_ReturnIndexedPlayingTrackArtistName(char *trackArtistName)
{
    ESP_LOGI(IPOD_TAG,"Track artist: %s",trackArtistName);
    byte txPacket[255] = {
        0x04,
        0x00,0x23
    };
    strcpy((char*)&txPacket[3],trackArtistName);
    sendPacket(txPacket,3+strlen(trackArtistName)+1);
}

/// @brief Returns the track Album Name after an implicit call for it
/// @param trackAlbumName Character array to return
void esPod::L0x04_0x25_ReturnIndexedPlayingTrackAlbumName(char *trackAlbumName)
{
    ESP_LOGI(IPOD_TAG,"Track album: %s",trackAlbumName);
    byte txPacket[255] = {
        0x04,
        0x00,0x25
    };
    strcpy((char*)&txPacket[3],trackAlbumName);
    sendPacket(txPacket,3+strlen(trackAlbumName)+1);
}

/// @brief Only supports currently two types : 0x00 Playback Stopped, 0x01 Track index, 0x04 Track offset
/// @param notification Notification type that can be returned : 0x01 for track index change, 0x04 for the track offset
/// @param numField For 0x01 this is the new Track index, for 0x04 this is the current Track offset in ms
void esPod::L0x04_0x27_PlayStatusNotification(byte notification, uint32_t numField)
{
    ESP_LOGI(IPOD_TAG,"Play status 0x%02x Numfield: %d",notification,numField);
    byte txPacket[] = {
        0x04,
        0x00,0x27,
        notification,
        0x00,0x00,0x00,0x00
    };
    *((uint32_t*)&txPacket[4]) = swap_endian<uint32_t>(numField);
    sendPacket(txPacket,sizeof(txPacket));
}

/// @brief Returns the PlayStatusNotification when it is STOPPED (0x00)
/// @param notification Can only be 0x00
void esPod::L0x04_0x27_PlayStatusNotification(byte notification)
{
    ESP_LOGI(IPOD_TAG,"Play status 0x%02x STOPPED",notification);
    byte txPacket[] = {
        0x04,
        0x00,0x27,
        notification
    };
    sendPacket(txPacket,sizeof(txPacket));
}

/// @brief Returns the current shuffle status of the PBEngine
/// @param currentShuffleStatus 0x00 No Shuffle, 0x01 Tracks, 0x02 Albums
void esPod::L0x04_0x2D_ReturnShuffle(byte currentShuffleStatus)
{
    ESP_LOGI(IPOD_TAG,"Shuffle status: 0x%02x",currentShuffleStatus);
    byte txPacket[] = {
        0x04,
        0x00,0x2D,
        currentShuffleStatus
    };
    sendPacket(txPacket,sizeof(txPacket));
}

/// @brief Returns the current repeat status of the PBEngine
/// @param currentRepeatStatus 0x00 No Repeat, 0x01 One Track, 0x02 All tracks
void esPod::L0x04_0x30_ReturnRepeat(byte currentRepeatStatus)
{
    ESP_LOGI(IPOD_TAG,"Repeat status: 0x%02x",currentRepeatStatus);
    byte txPacket[] = {
        0x04,
        0x00,0x30,
        currentRepeatStatus
    };
    sendPacket(txPacket,sizeof(txPacket));
}

/// @brief Returns the number of playing tracks in the current selection (here it is all the tracks)
/// @param numPlayingTracks Number of playing tracks to return
void esPod::L0x04_0x36_ReturnNumPlayingTracks(uint32_t numPlayingTracks)
{
    ESP_LOGI(IPOD_TAG,"Playing tracks: %d",numPlayingTracks);
    byte txPacket[] = {
        0x04,
        0x00,0x36,
        0x00,0x00,0x00,0x00
    };
    *((uint32_t*)&txPacket[3]) = swap_endian<uint32_t>(numPlayingTracks);
    sendPacket(txPacket,sizeof(txPacket));
}

#pragma endregion

//-----------------------------------------------------------------------
//|                     Process Lingo 0x00 Requests                     |
//-----------------------------------------------------------------------
#pragma region 0x00 Processor

/// @brief This function processes a shortened byteArray packet identified as a valid Lingo 0x00 request
/// @param byteArray Shortened packet, with byteArray[0] being the Lingo 0x00 command ID byte
/// @param len Length of valid data in the byteArray
void esPod::processLingo0x00(const byte *byteArray, uint32_t len)
{
    byte cmdID = byteArray[0];
    //Switch through expected commandIDs
    switch (cmdID)
    { 
    case L0x00_RequestExtendedInterfaceMode: //Mini requests extended interface mode status
        {
            ESP_LOGI(IPOD_TAG,"CMD: 0x%02x RequestExtendedInterfaceMode",cmdID);
            if(extendedInterfaceModeActive) {
                L0x00_0x04_ReturnExtendedInterfaceMode(0x01); //Report that extended interface mode is active
            }
            else
        {
            L0x00_0x04_ReturnExtendedInterfaceMode(0x00); //Report that extended interface mode is not active
        }
        }
        break;

    case L0x00_EnterExtendedInterfaceMode: //Mini forces extended interface mode
        {
            ESP_LOGI(IPOD_TAG,"CMD: 0x%02x EnterExtendedInterfaceMode",cmdID);
            if(!extendedInterfaceModeActive) {
                //Send a first iPodAck Command pending with a 1000ms timeout
                L0x00_0x02_iPodAck(iPodAck_CmdPending,cmdID,1000);
                extendedInterfaceModeActive = true;
            }
            L0x00_0x02_iPodAck(iPodAck_OK,cmdID);
        }
        break;
    
    case L0x00_RequestiPodName: //Mini requests ipod name
        {
            ESP_LOGI(IPOD_TAG,"CMD: 0x%02x RequestiPodName",cmdID);
            L0x00_0x08_ReturniPodName();
        }
        break;
    
    case L0x00_RequestiPodSoftwareVersion: //Mini requests ipod software version
        {
            ESP_LOGI(IPOD_TAG,"CMD: 0x%02x RequestiPodSoftwareVersion",cmdID);
            L0x00_0x0A_ReturniPodSoftwareVersion();
        }
        break;
    
    case L0x00_RequestiPodSerialNum: //Mini requests ipod Serial Num
        {
            ESP_LOGI(IPOD_TAG,"CMD: 0x%02x RequestiPodSerialNum",cmdID);
            L0x00_0x0C_ReturniPodSerialNum();
        }
        break;
    
    case L0x00_RequestiPodModelNum: //Mini requests ipod Model Num
        {
            ESP_LOGI(IPOD_TAG,"CMD: 0x%02x RequestiPodModelNum",cmdID);
            L0x00_0x0E_ReturniPodModelNum();
        }
        break;
    
    case L0x00_RequestLingoProtocolVersion: //Mini requestsLingo Protocol Version
        {
            ESP_LOGI(IPOD_TAG,"CMD: 0x%02x RequestLingoProtocolVersion for Lingo 0x%02x",cmdID,byteArray[1]);
            L0x00_0x10_ReturnLingoProtocolVersion(byteArray[1]);
        }
        break;
    
    case L0x00_IdentifyDeviceLingoes: //Mini identifies its lingoes, used as an ice-breaker
        {
            ESP_LOGI(IPOD_TAG,"CMD: 0x%02x IdentifyDeviceLingoes",cmdID);
            L0x00_0x02_iPodAck(iPodAck_OK,cmdID);//Acknowledge, start capabilities pingpong
            if(!_accessoryCapabilitiesRequested)
            {
                L0x00_0x27_GetAccessoryInfo(0x00); //Immediately request general capabilities
            }
            
        }
        break;
    
    case L0x00_RetAccessoryInfo: //Mini returns info after L0x00_0x27
        {
            ESP_LOGI(IPOD_TAG,"CMD: 0x%02x RetAccessoryInfo: 0x%02x",cmdID,byteArray[1]);
            switch (byteArray[1]) //Ping-pong the next request based on the current response
            {
            case 0x00:
                _accessoryCapabilitiesRequested = true;
                if (!_accessoryNameRequested)
                {
                    L0x00_0x27_GetAccessoryInfo(0x01); //Request the name
                }
                break;

            case 0x01:
                _accessoryNameRequested = true;
                if (!_accessoryFirmwareRequested)
                {
                    L0x00_0x27_GetAccessoryInfo(0x04); //Request the firmware version
                }
                break;

            case 0x04:
                _accessoryFirmwareRequested = true;
                if (!_accessoryHardwareRequested)
                {
                    L0x00_0x27_GetAccessoryInfo(0x05); //Request the hardware number
                }
                break;

            case 0x05:
                _accessoryHardwareRequested = true;
                if (!_accessoryManufRequested)
                {
                    L0x00_0x27_GetAccessoryInfo(0x06); //Request the manufacturer name
                }
                break;

            case 0x06:
                _accessoryManufRequested = true;
                if (!_accessoryModelRequested)
                {
                    L0x00_0x27_GetAccessoryInfo(0x07); //Request the model number
                }
                break;

            case 0x07:
                _accessoryModelRequested = true; //End of the reactionchain
                ESP_LOGI(IPOD_TAG,"Handshake complete.");
                break;
            }
        }
        break;
    
    default: //In case the command is not known
        {
            ESP_LOGW(IPOD_TAG,"CMD 0x%02x not recognized.",cmdID);
            L0x00_0x02_iPodAck(cmdID,iPodAck_CmdFailed);
        }
        break;
    }
}
#pragma endregion

//-----------------------------------------------------------------------
//|                     Process Lingo 0x04 Requests                     |
//-----------------------------------------------------------------------
#pragma region 0x04 Processor

/// @brief This function processes a shortened byteArray packet identified as a valid Lingo 0x04 request
/// @param byteArray Shortened packet, with byteArray[1] being the last byte of the Lingo 0x04 command
/// @param len Length of valid data in the byteArray
void esPod::processLingo0x04(const byte *byteArray, uint32_t len)
{
    byte cmdID = byteArray[1];
    //Initialising handlers to understand what is happening in some parts of the switch. They cannot be initialised in the switch-case scope
    byte category;
    uint32_t startIndex, counts, tempTrackIndex;

    if(!extendedInterfaceModeActive)   { //Complain if not in extended interface mode
        ESP_LOGW(IPOD_TAG,"CMD 0x%04x not executed : Not in extendedInterfaceMode!",cmdID);
        L0x04_0x01_iPodAck(iPodAck_BadParam,cmdID);
    }
    //Good to go if in Extended Interface mode
    else    {
        switch (cmdID) // Reminder : we are technically switching on byteArray[1] now
        {
        case L0x04_GetIndexedPlayingTrackInfo:
            {
                tempTrackIndex = swap_endian<uint32_t>(*((uint32_t*)&byteArray[3]));
                switch (byteArray[2]) //Switch on the type of track info requested (careful with overloads)
                {
                case 0x00: //General track Capabilities and Information
                    ESP_LOGI(IPOD_TAG,"CMD 0x%04x GetIndexedPlayingTrackInfo 0x%02x for index %d (previous %d) : Duration",cmdID,byteArray[2],tempTrackIndex,prevTrackIndex);
                    if(tempTrackIndex==prevTrackIndex) 
                    {
                        L0x04_0x0D_ReturnIndexedPlayingTrackInfo((uint32_t)prevTrackDuration);
                    }
                    else 
                    {
                        L0x04_0x0D_ReturnIndexedPlayingTrackInfo((uint32_t)trackDuration);
                    }
                    break;
                case 0x02: //Track Release Date (fictional)
                    ESP_LOGI(IPOD_TAG,"CMD 0x%04x GetIndexedPlayingTrackInfo 0x%02x for index %d (previous %d) : Release date",cmdID,byteArray[2],tempTrackIndex,prevTrackIndex);
                    L0x04_0x0D_ReturnIndexedPlayingTrackInfo(byteArray[2],(uint16_t)2001);
                    break;
                case 0x01: //Track Title
                    ESP_LOGI(IPOD_TAG,"CMD 0x%04x GetIndexedPlayingTrackInfo 0x%02x for index %d (previous %d) : Title",cmdID,byteArray[2],tempTrackIndex,prevTrackIndex);
                    if(tempTrackIndex==prevTrackIndex) 
                    {
                        L0x04_0x0D_ReturnIndexedPlayingTrackInfo(byteArray[2],prevTrackTitle);
                    }
                    else 
                    {
                        L0x04_0x0D_ReturnIndexedPlayingTrackInfo(byteArray[2],trackTitle);
                    }
                    break;
                case 0x05: //Track Genre
                    ESP_LOGI(IPOD_TAG,"CMD 0x%04x GetIndexedPlayingTrackInfo 0x%02x for index %d (previous %d) : Genre",cmdID,byteArray[2],tempTrackIndex,prevTrackIndex);
                    L0x04_0x0D_ReturnIndexedPlayingTrackInfo(byteArray[2],trackGenre);
                    break; 
                case 0x06: //Track Composer
                    ESP_LOGI(IPOD_TAG,"CMD 0x%04x GetIndexedPlayingTrackInfo 0x%02x for index %d (previous %d) : Composer",cmdID,byteArray[2],tempTrackIndex,prevTrackIndex);
                    L0x04_0x0D_ReturnIndexedPlayingTrackInfo(byteArray[2],composer);
                    break; 
                default: //In case the request is beyond the track capabilities
                    ESP_LOGW(IPOD_TAG,"CMD 0x%04x GetIndexedPlayingTrackInfo 0x%02x for index %d (previous %d) : Type not recognised!",cmdID,byteArray[2],tempTrackIndex,prevTrackIndex);
                    L0x04_0x01_iPodAck(iPodAck_BadParam,cmdID);
                    break;
                }
            }
            break;
        
        case L0x04_RequestProtocolVersion: //Hardcoded return for L0x04
            {
                ESP_LOGI(IPOD_TAG,"CMD 0x%04x RequestProtocolVersion",cmdID);
                L0x04_0x13_ReturnProtocolVersion();
            }
            break;

        case L0x04_ResetDBSelection: //Not sure what to do here. Reset Current Track Index ?
            {
                ESP_LOGI(IPOD_TAG,"CMD 0x%04x ResetDBSelection",cmdID);
                L0x04_0x01_iPodAck(iPodAck_OK,cmdID);
            }
            break;

        case L0x04_SelectDBRecord: //Used for browsing ?
            {
                ESP_LOGI(IPOD_TAG,"CMD 0x%04x SelectDBRecord",cmdID);
                L0x04_0x01_iPodAck(iPodAck_OK,cmdID);
            }
            break;
        
        case L0x04_GetNumberCategorizedDBRecords: //Mini requests the number of records for a specific DB_CAT
            {
                category = byteArray[2];
                ESP_LOGI(IPOD_TAG,"CMD 0x%04x GetNumberCategorizedDBRecords category: 0x%02x",cmdID,category);
                if(category == DB_CAT_TRACK) 
                { 
                    L0x04_0x19_ReturnNumberCategorizedDBRecords(totalNumberTracks);// Say there are fixed, large amount of tracks
                }
                else 
                { //And only one of anything else (Playlist, album, artist etc...)
                    L0x04_0x19_ReturnNumberCategorizedDBRecords(1);
                }
            }
            break;
        
        case L0x04_RetrieveCategorizedDatabaseRecords: //Loops through the desired records for a given DB_CAT
            {
                category = byteArray[2]; //DBCat
                startIndex = swap_endian<uint32_t>(*(uint32_t*)&byteArray[3]);
                counts = swap_endian<uint32_t>(*(uint32_t*)&byteArray[7]);

                ESP_LOGI(IPOD_TAG,"CMD 0x%04x RetrieveCategorizedDatabaseRecords category: 0x%02x from %d for %d counts",cmdID,category,startIndex,counts);
                switch (category)
                {
                case DB_CAT_PLAYLIST:
                    for (uint32_t i = startIndex; i < startIndex +counts; i++)
                    {
                        L0x04_0x1B_ReturnCategorizedDatabaseRecord(i,playList);
                    }
                    break;
                case DB_CAT_ARTIST:
                    for (uint32_t i = startIndex; i < startIndex +counts; i++)
                    {
                        L0x04_0x1B_ReturnCategorizedDatabaseRecord(i,artistName);
                    }
                    break;
                case DB_CAT_ALBUM:
                    for (uint32_t i = startIndex; i < startIndex +counts; i++)
                    {
                        L0x04_0x1B_ReturnCategorizedDatabaseRecord(i,albumName);
                    }
                    break;
                case DB_CAT_GENRE:
                    for (uint32_t i = startIndex; i < startIndex +counts; i++)
                    {
                        L0x04_0x1B_ReturnCategorizedDatabaseRecord(i,trackGenre);
                    }
                    break;
                case DB_CAT_TRACK: //Will sometimes return twice
                    for (uint32_t i = startIndex; i < startIndex +counts; i++)
                    {
                        L0x04_0x1B_ReturnCategorizedDatabaseRecord(i,trackTitle);
                    }
                    break;
                case DB_CAT_COMPOSER:
                    for (uint32_t i = startIndex; i < startIndex +counts; i++)
                    {
                        L0x04_0x1B_ReturnCategorizedDatabaseRecord(i,composer);
                    }
                    break;
                }
            }
            break;

        case L0x04_GetPlayStatus: //Returns the current playStatus and the position/duration of the current track
            {   
                ESP_LOGI(IPOD_TAG,"CMD 0x%04x GetPlayStatus",cmdID);
                L0x04_0x1D_ReturnPlayStatus(playPosition,trackDuration,playStatus);
            }
            break;

        case L0x04_GetCurrentPlayingTrackIndex: //Get the uint32 index of the currently playing song
            {
                ESP_LOGI(IPOD_TAG,"CMD 0x%04x GetCurrentPlayingTrackIndex",cmdID);
                L0x04_0x1F_ReturnCurrentPlayingTrackIndex(currentTrackIndex);
            }
            break;

        case L0x04_GetIndexedPlayingTrackTitle: 
            {
                tempTrackIndex = swap_endian<uint32_t>(*((uint32_t*)&byteArray[2]));
                ESP_LOGI(IPOD_TAG,"CMD 0x%04x GetIndexedPlayingTrackTitle for index %d (previous %d)",cmdID,tempTrackIndex,prevTrackIndex);
                if(tempTrackIndex==prevTrackIndex) 
                {
                    L0x04_0x21_ReturnIndexedPlayingTrackTitle(prevTrackTitle);
                }
                else 
                {
                    L0x04_0x21_ReturnIndexedPlayingTrackTitle(trackTitle);
                }     
            }
            break;

        case L0x04_GetIndexedPlayingTrackArtistName: 
            {
                tempTrackIndex = swap_endian<uint32_t>(*((uint32_t*)&byteArray[2]));

                ESP_LOGI(IPOD_TAG,"CMD 0x%04x GetIndexedPlayingTrackArtistName for index %d (previous %d)",cmdID,tempTrackIndex,prevTrackIndex);
                if(tempTrackIndex==prevTrackIndex) 
                {
                    L0x04_0x23_ReturnIndexedPlayingTrackArtistName(prevArtistName);
                }
                else 
                {
                    L0x04_0x23_ReturnIndexedPlayingTrackArtistName(artistName);
                }  
            }
            break;

        case L0x04_GetIndexedPlayingTrackAlbumName: 
            {
                tempTrackIndex = swap_endian<uint32_t>(*((uint32_t*)&byteArray[2]));
                ESP_LOGI(IPOD_TAG,"CMD 0x%04x GetIndexedPlayingTrackAlbumName for index %d (previous %d)",cmdID,tempTrackIndex,prevTrackIndex);
                if(tempTrackIndex==prevTrackIndex) 
                {
                    L0x04_0x25_ReturnIndexedPlayingTrackAlbumName(prevAlbumName);
                }
                else 
                {
                    L0x04_0x25_ReturnIndexedPlayingTrackAlbumName(albumName);
                }
            }
            break;

        case L0x04_SetPlayStatusChangeNotification: //Turns on basic notifications
            {
                playStatusNotificationState = byteArray[2];
                ESP_LOGI(IPOD_TAG,"CMD 0x%04x SetPlayStatusChangeNotification 0x%02x",cmdID,playStatusNotificationState);
                L0x04_0x01_iPodAck(iPodAck_OK,cmdID);
            }
            break;

        case L0x04_PlayCurrentSelection: //Used to play a specific index, usually for "next" commands, but may be used to actually jump anywhere
            {
                tempTrackIndex = swap_endian<uint32_t>(*((uint32_t*)&byteArray[2]));
                ESP_LOGI(IPOD_TAG,"CMD 0x%04x PlayCurrentSelection index %d",cmdID,tempTrackIndex);
                if(playStatus!=PB_STATE_PLAYING) 
                {
                    playStatus = PB_STATE_PLAYING; //Playing status forced 
                    if(_playStatusHandler) 
                    {
                        _playStatusHandler(A2DP_PLAY); //Send play to the a2dp
                    }
                }
                if (tempTrackIndex==trackList[(trackListPosition+TOTAL_NUM_TRACKS-1)%TOTAL_NUM_TRACKS]) //Desired trackIndex is the left entry
                {
                    //This is only for when the system requires the data of the previously active track
                    prevTrackIndex = currentTrackIndex; 
                    strcpy(prevAlbumName,albumName);
                    strcpy(prevArtistName,artistName);
                    strcpy(prevTrackTitle,trackTitle);
                    prevTrackDuration = trackDuration;

                    //Cursor operations for PREV
                    trackListPosition = (trackListPosition+TOTAL_NUM_TRACKS-1)%TOTAL_NUM_TRACKS; //Shift trackListPosition one to the right
                    currentTrackIndex = tempTrackIndex;
                    
                    //Engage the pending ACK for expected metadata
                    trackChangeAckPending = cmdID;
                    trackChangeTimestamp = millis();
                    ESP_LOGI(IPOD_TAG,"Prev. index %d New index %d Tracklist pos. %d Pending Meta %d Timestamp: %d --> PREV ",prevTrackIndex,currentTrackIndex,trackListPosition,(trackChangeAckPending>0x00),trackChangeTimestamp);
                    L0x04_0x01_iPodAck(iPodAck_CmdPending,cmdID,TRACK_CHANGE_TIMEOUT);

                    //Fire the A2DP when ready
                    if(_playStatusHandler) _playStatusHandler(A2DP_PREV); //Fire the metadata trigger indirectly
                }
                else if (tempTrackIndex == currentTrackIndex) //Somehow reselecting the current track
                {
                    ESP_LOGI(IPOD_TAG,"Selected same track as current: %d",tempTrackIndex);
                    L0x04_0x01_iPodAck(iPodAck_OK,cmdID);
                    
                    //Fire the A2DP when ready
                    if(_playStatusHandler) _playStatusHandler(A2DP_PREV); //Fire the metadata trigger indirectly
                }
                else    //If it is not the previous or the current track, it automaticallybecomes a next track
                {
                    //This is only for when the system requires the data of the previously active track
                    prevTrackIndex = currentTrackIndex; 
                    strcpy(prevAlbumName,albumName);
                    strcpy(prevArtistName,artistName);
                    strcpy(prevTrackTitle,trackTitle);
                    prevTrackDuration = trackDuration;

                    //Cursor operations for NEXT
                    trackListPosition = (trackListPosition + 1) % TOTAL_NUM_TRACKS;
                    trackList[trackListPosition] = tempTrackIndex;
                    currentTrackIndex = tempTrackIndex;

                    //Engage the pending ACK for expected metadata
                    trackChangeAckPending = cmdID;
                    trackChangeTimestamp = millis();
                    ESP_LOGI(IPOD_TAG,"Prev. index %d New index %d Tracklist pos. %d Pending Meta %d Timestamp: %d --> NEXT ",prevTrackIndex,currentTrackIndex,trackListPosition,(trackChangeAckPending>0x00),trackChangeTimestamp);
                    L0x04_0x01_iPodAck(iPodAck_CmdPending,cmdID,TRACK_CHANGE_TIMEOUT);

                    //Fire the A2DP when ready
                    if(_playStatusHandler) _playStatusHandler(A2DP_NEXT); //Fire the metadata trigger indirectly
                }
            }
            break;
        
        case L0x04_PlayControl: //Basic play control. Used for Prev, pause and play
            {                   
                ESP_LOGI(IPOD_TAG,"CMD 0x%04x PlayControl req: 0x%02x vs playStatus: 0x%02x",cmdID,byteArray[2],playStatus);
                switch (byteArray[2]) //PlayControl byte
                {
                case PB_CMD_TOGGLE: //Just Toggle or start playing
                    {
                        if(playStatus==PB_STATE_PLAYING) 
                        {
                            playStatus=PB_STATE_PAUSED; //Toggle to paused if playing
                            if(_playStatusHandler) _playStatusHandler(A2DP_PAUSE);
                        }
                        else 
                        {
                            playStatus = PB_STATE_PLAYING; //Switch to playing in any other case
                            if(_playStatusHandler) _playStatusHandler(A2DP_PLAY);
                        }
                        L0x04_0x01_iPodAck(iPodAck_OK,cmdID);
                    }
                    break;
                case PB_CMD_STOP: //Stop
                    {
                        playStatus = PB_STATE_STOPPED;
                        if(_playStatusHandler) _playStatusHandler(A2DP_STOP);
                        L0x04_0x01_iPodAck(iPodAck_OK,cmdID);
                    }
                    break;
                case PB_CMD_NEXT_TRACK: //Next track.. never seems to happen ?
                    {
                        //This is only for when the system requires the data of the previously active track
                        prevTrackIndex = currentTrackIndex; 
                        strcpy(prevAlbumName,albumName);
                        strcpy(prevArtistName,artistName);
                        strcpy(prevTrackTitle,trackTitle);
                        prevTrackDuration = trackDuration;

                        //Cursor operations for NEXT
                        trackListPosition = (trackListPosition + 1) % TOTAL_NUM_TRACKS;
                        currentTrackIndex = trackList[trackListPosition];

                        //Engage the pending ACK for expected metadata
                        trackChangeAckPending = cmdID;
                        trackChangeTimestamp = millis();
                        ESP_LOGI(IPOD_TAG,"Prev. index %d New index %d Tracklist pos. %d Pending Meta %d Timestamp: %d --> EXPLICIT NEXT TRACK",prevTrackIndex,currentTrackIndex,trackListPosition,(trackChangeAckPending>0x00),trackChangeTimestamp);
                        L0x04_0x01_iPodAck(iPodAck_CmdPending,cmdID,TRACK_CHANGE_TIMEOUT);

                        //Fire the A2DP when ready
                        if(_playStatusHandler) _playStatusHandler(A2DP_NEXT); //Fire the metadata trigger indirectly
                    }
                    break;
                case PB_CMD_PREVIOUS_TRACK: //Prev track
                    {
                        ESP_LOGI(IPOD_TAG,"Current index %d Tracklist pos. %d --> EXPLICIT SINGLE PREV TRACK",currentTrackIndex,trackListPosition);
                        L0x04_0x01_iPodAck(iPodAck_OK,cmdID);

                        //Fire the A2DP when ready
                        if(_playStatusHandler) _playStatusHandler(A2DP_PREV); //Fire the metadata trigger indirectly
                    }
                    break;
                case PB_CMD_NEXT: //Next track
                    {
                        //This is only for when the system requires the data of the previously active track
                        prevTrackIndex = currentTrackIndex; 
                        strcpy(prevAlbumName,albumName);
                        strcpy(prevArtistName,artistName);
                        strcpy(prevTrackTitle,trackTitle);
                        prevTrackDuration = trackDuration;

                        //Cursor operations for NEXT
                        trackListPosition = (trackListPosition + 1) % TOTAL_NUM_TRACKS;
                        currentTrackIndex = trackList[trackListPosition];

                        //Engage the pending ACK for expected metadata
                        trackChangeAckPending = cmdID;
                        trackChangeTimestamp = millis();
                        ESP_LOGI(IPOD_TAG,"Prev. index %d New index %d Tracklist pos. %d Pending Meta %d Timestamp: %d --> EXPLICIT NEXT",prevTrackIndex,currentTrackIndex,trackListPosition,(trackChangeAckPending>0x00),trackChangeTimestamp);
                        L0x04_0x01_iPodAck(iPodAck_CmdPending,cmdID,TRACK_CHANGE_TIMEOUT);

                        //Fire the A2DP when ready
                        if(_playStatusHandler) _playStatusHandler(A2DP_NEXT); //Fire the metadata trigger indirectly
                    }
                    break;
                case PB_CMD_PREV: //Prev track
                    {
                        ESP_LOGI(IPOD_TAG,"Current index %d Tracklist pos. %d --> EXPLICIT SINGLE PREV",currentTrackIndex,trackListPosition);
                        L0x04_0x01_iPodAck(iPodAck_OK,cmdID);

                        //Fire the A2DP when ready
                        if(_playStatusHandler) _playStatusHandler(A2DP_PREV); //Fire the metadata trigger indirectly
                    }
                    break;
                case PB_CMD_PLAY: //Play... do we need to have an ack pending ?
                    {
                        playStatus = PB_STATE_PLAYING;
                        
                        //Engage the pending ACK for expected metadata
                        trackChangeAckPending = cmdID;
                        trackChangeTimestamp = millis();
                        L0x04_0x01_iPodAck(iPodAck_CmdPending,cmdID,TRACK_CHANGE_TIMEOUT);
                        
                        if(_playStatusHandler) _playStatusHandler(A2DP_PLAY);
                    }
                    break;
                case PB_CMD_PAUSE: //Pause
                    {
                        playStatus = PB_STATE_PAUSED;
                        L0x04_0x01_iPodAck(iPodAck_OK,cmdID);
                        if(_playStatusHandler) _playStatusHandler(A2DP_PAUSE);
                    }
                    break;
                }
                if((playStatus == PB_STATE_STOPPED)&&(playStatusNotificationState==NOTIF_ON)) L0x04_0x27_PlayStatusNotification(0x00); //Notify successful stop
            }   
            break;

        case L0x04_GetShuffle: //Get Shuffle state from the PB Engine
            {
                ESP_LOGI(IPOD_TAG,"CMD 0x%04x GetShuffle",cmdID);
                L0x04_0x2D_ReturnShuffle(shuffleStatus);
            }
            break;

        case L0x04_SetShuffle: //Set Shuffle state
            {
                ESP_LOGI(IPOD_TAG,"CMD 0x%04x SetShuffle req: 0x%02x vs shuffleStatus: 0x%02x",cmdID,byteArray[2],shuffleStatus);
                shuffleStatus = byteArray[2];
                L0x04_0x01_iPodAck(iPodAck_OK,cmdID);
            }
            break;
        
        case L0x04_GetRepeat: //Get Repeat state
            {
                ESP_LOGI(IPOD_TAG,"CMD 0x%04x GetRepeat",cmdID);
                L0x04_0x30_ReturnRepeat(repeatStatus);
            }
            break;

        case L0x04_SetRepeat: //Set Repeat state
            {
                ESP_LOGI(IPOD_TAG,"CMD 0x%04x SetRepeat req: 0x%02x vs repeatStatus: 0x%02x",cmdID,byteArray[2],repeatStatus);
                repeatStatus = byteArray[2];
                L0x04_0x01_iPodAck(iPodAck_OK,cmdID);
            }
            break;

        case L0x04_GetNumPlayingTracks: //Systematically return TOTAL_NUM_TRACKS
            {
                ESP_LOGI(IPOD_TAG,"CMD 0x%04x GetNumPlayingTracks",cmdID);
                L0x04_0x36_ReturnNumPlayingTracks(totalNumberTracks);
            }
            break;

        case L0x04_SetCurrentPlayingTrack: //Basically identical to PlayCurrentSelection
                {
                tempTrackIndex = swap_endian<uint32_t>(*((uint32_t*)&byteArray[2]));
                ESP_LOGI(IPOD_TAG,"CMD 0x%04x SetCurrentPlayingTrack index %d",cmdID,tempTrackIndex);
                if(playStatus!=PB_STATE_PLAYING) {
                    playStatus = PB_STATE_PLAYING; //Playing status forced 
                    if(_playStatusHandler) 
                    {
                        _playStatusHandler(A2DP_PLAY); //Send play to the a2dp
                    }
                }
                if (tempTrackIndex==trackList[(trackListPosition+TOTAL_NUM_TRACKS-1)%TOTAL_NUM_TRACKS]) //Desired trackIndex is the left entry
                {
                    //This is only for when the system requires the data of the previously active track
                    prevTrackIndex = currentTrackIndex; 
                    strcpy(prevAlbumName,albumName);
                    strcpy(prevArtistName,artistName);
                    strcpy(prevTrackTitle,trackTitle);
                    prevTrackDuration = trackDuration;

                    //Cursor operations for PREV
                    trackListPosition = (trackListPosition+TOTAL_NUM_TRACKS-1)%TOTAL_NUM_TRACKS; //Shift trackListPosition one to the right
                    currentTrackIndex = tempTrackIndex;
                    
                    //Engage the pending ACK for expected metadata
                    trackChangeAckPending = cmdID;
                    trackChangeTimestamp = millis();
                    ESP_LOGI(IPOD_TAG,"Prev. index %d New index %d Tracklist pos. %d Pending Meta %d Timestamp: %d --> PREV ",prevTrackIndex,currentTrackIndex,trackListPosition,(trackChangeAckPending>0x00),trackChangeTimestamp);
                    L0x04_0x01_iPodAck(iPodAck_CmdPending,cmdID,TRACK_CHANGE_TIMEOUT);

                    //Fire the A2DP when ready
                    if(_playStatusHandler) _playStatusHandler(A2DP_PREV); //Fire the metadata trigger indirectly
                }
                else if (tempTrackIndex == currentTrackIndex) //Somehow reselecting the current track
                {
                    ESP_LOGI(IPOD_TAG,"Selected same track as current: %d",tempTrackIndex);
                    L0x04_0x01_iPodAck(iPodAck_OK,cmdID);
                    
                    //Fire the A2DP when ready
                    if(_playStatusHandler) _playStatusHandler(A2DP_PREV); //Fire the metadata trigger indirectly
                }
                else    //If it is not the previous or the current track, it automaticallybecomes a next track
                {
                    //This is only for when the system requires the data of the previously active track
                    prevTrackIndex = currentTrackIndex; 
                    strcpy(prevAlbumName,albumName);
                    strcpy(prevArtistName,artistName);
                    strcpy(prevTrackTitle,trackTitle);
                    prevTrackDuration = trackDuration;

                    //Cursor operations for NEXT
                    trackListPosition = (trackListPosition + 1) % TOTAL_NUM_TRACKS;
                    trackList[trackListPosition] = tempTrackIndex;
                    currentTrackIndex = tempTrackIndex;

                    //Engage the pending ACK for expected metadata
                    trackChangeAckPending = cmdID;
                    trackChangeTimestamp = millis();
                    ESP_LOGI(IPOD_TAG,"Prev. index %d New index %d Tracklist pos. %d Pending Meta %d Timestamp: %d --> NEXT ",prevTrackIndex,currentTrackIndex,trackListPosition,(trackChangeAckPending>0x00),trackChangeTimestamp);
                    L0x04_0x01_iPodAck(iPodAck_CmdPending,cmdID,TRACK_CHANGE_TIMEOUT);

                    //Fire the A2DP when ready
                    if(_playStatusHandler) _playStatusHandler(A2DP_NEXT); //Fire the metadata trigger indirectly
                }
            }
            break;
        }
    }
}

#pragma endregion

//-----------------------------------------------------------------------
//|                            Packet Disneyland                        |
//-----------------------------------------------------------------------
#pragma region Packet Disneyland

/// @brief Processes a valid packet and calls the relevant Lingo processor
/// @param byteArray Checksum-validated packet starting at LingoID
/// @param len Length of valid data in the packet
void esPod::processPacket(const byte *byteArray, uint32_t len)
{
    byte rxLingoID = byteArray[0];
    const byte* subPayload = byteArray+1; //Squeeze the Lingo out
    uint32_t subPayloadLen = len-1;
    switch (rxLingoID) //0x00 is general Lingo and 0x04 is extended Lingo. Nothing else is expected from the Mini
    {
    case 0x00: //General Lingo
        ESP_LOGD(IPOD_TAG,"Lingo 0x00 Packet in processor,payload length: %d",subPayloadLen);
        processLingo0x00(subPayload,subPayloadLen);
        break;
    
    case 0x04: // Extended Interface Lingo
        ESP_LOGD(IPOD_TAG,"Lingo 0x04 Packet in processor,payload length: %d",subPayloadLen);
        processLingo0x04(subPayload,subPayloadLen);
        break;
    
    default:
        ESP_LOGW(IPOD_TAG,"Unknown Lingo packet : L0x%x",rxLingoID);
        break;
    }
}

/// @brief Refresh function for the esPod : listens to Serial, assembles packets, or ignores everything if it is disabled.
void esPod::refresh()
{
    ESP_LOGV(IPOD_TAG,"Refresh called");
    //Check for a new packet and update the buffer
    while(_targetSerial.available()) 
    {
        byte incomingByte = _targetSerial.read();
        lastConnected = millis();
        if(!disabled) 
        {
            //A new 0xFF55 packet starter shows up
            if(_prevRxByte == 0xFF && incomingByte == 0x55 && !_rxInProgress) 
            { 
                ESP_LOGD(IPOD_TAG,"Packet starter received");
                _rxLen = 0; //Reset the received length
                _rxCounter = 0; //Reset the counter to the end of payload
                _rxInProgress = true;
            }
            else if(_rxInProgress)
            {
                if(_rxLen == 0 && _rxCounter == 0)
                {
                    _rxLen = incomingByte;
                    ESP_LOGD(IPOD_TAG,"Packet length set: %d",_rxLen);
                }
                else
                {
                    _rxBuf[_rxCounter++] = incomingByte;
                    if(_rxCounter == _rxLen+1) { //We are done receiving the packet
                        _rxInProgress = false;
                        byte tempChecksum = esPod::checksum(_rxBuf, _rxLen);
                        ESP_LOGD(IPOD_TAG,"Packet finished, not validated yet");
                        if (tempChecksum == _rxBuf[_rxLen]) //Checksum checks out
                        { 
                            processPacket(_rxBuf,_rxLen);  
                            break; //This should process messages one by one
                        }
                    }
                }
            }

            //pass to the previous received byte
            _prevRxByte = incomingByte;
        }
        else //If the espod is disabled
        {
            _targetSerial.read();
        }
    }

    //Reset if no message received in the last 120s
    if((millis()-lastConnected > 120000) && !disabled) 
    {
        ESP_LOGW(IPOD_TAG,"Serial comms timed out: %lu ms",millis()-lastConnected);
        resetState();
    }

    //Send the track change Ack Pending if it has not sent already
    if(!disabled && (trackChangeAckPending>0x00) && (millis()>(trackChangeTimestamp+TRACK_CHANGE_TIMEOUT))) 
    {
        ESP_LOGD(IPOD_TAG,"Track change ack pending timed out ! ");        
        L0x04_0x01_iPodAck(iPodAck_OK,trackChangeAckPending);
        trackChangeAckPending = 0x00;
    }

}
#pragma endregion

