#include "esPod.h"

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
#pragma region
esPod::esPod(Stream& targetSerial) 
    :
        #ifdef DEBUG_MODE
        _debugSerial(Serial),
        #endif
        _targetSerial(targetSerial)
{
}

void esPod::resetState(){

    #ifdef DEBUG_MODE
        _debugSerial.println("espod.resetState() triggered");
    #endif

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
    playStatusNotificationsPaused = false; //To Deprecate
    trackChangeAckPending = 0x00;
    waitMetadataUpdate = false;  //To Deprecate, probably
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
    _targetSerial.write(0xFF);
    _targetSerial.write(0x55);
    _targetSerial.write((byte)len);
    _targetSerial.write(byteArray,len);
    _targetSerial.write(esPod::checksum(byteArray,len));
}

#pragma endregion

//-----------------------------------------------------------------------
//|                     Lingo 0x00 subfunctions                         |
//-----------------------------------------------------------------------
#pragma region 

/// @brief General response command for Lingo 0x00
/// @param cmdStatus Has to obey to iPodAck_xxx format as defined in L0x00.h
/// @param cmdID ID (single byte) of the Lingo 0x00 command replied to
void esPod::L0x00_0x02_iPodAck(byte cmdStatus,byte cmdID) {
    #ifdef DEBUG_MODE
        _debugSerial.printf("TX: L0x00 0x02 iPodAck: %x CMD: %x \n",cmdStatus,cmdID);
    #endif
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
    #ifdef DEBUG_MODE
    _debugSerial.printf("TX: L0x00 0x02 iPodAck: %x CMD: %x NumField: %d\n",cmdStatus,cmdID,numField);
    #endif
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
    #ifdef DEBUG_MODE
    _debugSerial.printf("TX: L0x00 0x04 ReturnExtendedInterfaceMode: 0x%x\n",extendedModeByte);
    #endif
    const byte txPacket[] = {
        0x00,
        0x04,
        extendedModeByte
    };
    sendPacket(txPacket,sizeof(txPacket));
}

/// @brief Returns as a UTF8 null-ended char array, the _name of the iPod (not changeable during runtime)
void esPod::L0x00_0x08_ReturniPodName() {
    #ifdef DEBUG_MODE
    _debugSerial.printf("TX: L0x00 0x08 ReturniPodName: %s\n",_name);
    #endif
    byte txPacket[255] = { //Prealloc to len = FF
        0x00,
        0x08
    };
    strcpy((char*)&txPacket[2],_name);
    sendPacket(txPacket,3+strlen(_name));
}

/// @brief Returns the iPod Software Version
void esPod::L0x00_0x0A_ReturniPodSoftwareVersion() {
    #ifdef DEBUG_MODE
    _debugSerial.printf("TX: L0x00 0x0A ReturniPodSoftwareVersion: %d.%d.%d \n",_SWMajor,_SWMinor,_SWrevision);
    #endif
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
    #ifdef DEBUG_MODE
    _debugSerial.printf("TX: L0x00 0x0C ReturniPodSerialNum: %s\n",_serialNumber);
    #endif
    byte txPacket[255] = { //Prealloc to len = FF
        0x00,
        0x0C
    };
    strcpy((char*)&txPacket[2],_serialNumber);
    sendPacket(txPacket,3+strlen(_serialNumber));
}

/// @brief Returns the iPod model number PA146FD 720901, which corresponds to an iPod 5.5G classic
void esPod::L0x00_0x0E_ReturniPodModelNum() {
    #ifdef DEBUG_MODE
    _debugSerial.println("TX: L0x00 0x0E ReturniPodModelNumber PA146FD 720901 ");
    #endif
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
    #ifdef DEBUG_MODE
    _debugSerial.printf("TX: L0x00 0x10 ReturnLingoProtocolVersion for Lingo 0x%x\n",targetLingo);
    #endif
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
    sendPacket(txPacket,sizeof(txPacket));
}

/// @brief Query the accessory Info (model number, manufacturer, firmware version ...) using the target Info category hex
/// @param desiredInfo Hex code for the type of information that is desired
void esPod::L0x00_0x27_GetAccessoryInfo(byte desiredInfo)
{
    #ifdef DEBUG_MODE
    _debugSerial.printf("TX: L0x00 0x27 GetAccessoryInfo of type 0x%x\n",desiredInfo);
    #endif
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
#pragma region 

/// @brief General response command for Lingo 0x04
/// @param cmdStatus Has to obey to iPodAck_xxx format as defined in L0x00.h
/// @param cmdID last two ID bytes of the Lingo 0x04 command replied to
void esPod::L0x04_0x01_iPodAck(byte cmdStatus, byte cmdID)
{
    #ifdef DEBUG_MODE
    _debugSerial.printf("TX: L0x04 0x01 iPodAck: %x CMD %x\n",cmdStatus,cmdID);
    #endif
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
    #ifdef DEBUG_MODE
    _debugSerial.printf("TX: L0x04 0x01 iPodAck: %x CMD: 0x00%x NumField: %d\n",cmdStatus,cmdID,numField);
    #endif
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
    #ifdef DEBUG_MODE
    _debugSerial.printf("TX: L0x04 0x0D ReturnIndexedPlayingTrackInfo type: 0x%x\n",trackInfoType);
    #endif
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
    #ifdef DEBUG_MODE
    _debugSerial.printf("TX: L0x04 0x0D ReturnIndexedPlayingTrackInfo total duration: %d\n",trackDuration_ms);
    #endif
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
    #ifdef DEBUG_MODE
    _debugSerial.printf("TX: L0x04 0x0D ReturnIndexedPlayingTrackInfo release year: %d\n",releaseYear);
    #endif
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
    #ifdef DEBUG_MODE
    _debugSerial.println("TX: L0x04 0x13 ReturnProtocolVersion: 1.12");
    #endif
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
    #ifdef DEBUG_MODE
    _debugSerial.printf("TX: L0x04 0x19 ReturnNumberCategorizedDBRecords: %d\n",categoryDBRecords);
    #endif
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
    #ifdef DEBUG_MODE
    _debugSerial.printf("TX: L0x04 0x1B ReturnCategorizedDatabaseRecord at index %d : %s\n",index,recordString);
    #endif
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
    #ifdef DEBUG_MODE
    _debugSerial.printf("TX: L0x04 0x1D ReturnPlayStatus: 0x%x at %d / %d ms\n",playStatusArg,position,duration);
    #endif
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
    #ifdef DEBUG_MODE
    _debugSerial.printf("TX: L0x04 0x1F ReturnCurrentPlayingTrackIndex: %d\n",trackIndex);
    #endif
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
    #ifdef DEBUG_MODE
    _debugSerial.printf("TX: L0x04 0x21 ReturnIndexedPlayingTrackTitle: %s\n",trackTitle);
    #endif
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
    #ifdef DEBUG_MODE
    _debugSerial.printf("TX: L0x04 0x23 ReturnIndexedPlayingTrackArtistName: %s\n",trackArtistName);
    #endif
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
    #ifdef DEBUG_MODE
    _debugSerial.printf("TX: L0x04 0x23 ReturnIndexedPlayingTrackArtistName: %s\n",trackAlbumName);
    #endif
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
    #ifdef DEBUG_MODE
    _debugSerial.printf("TX: L0x04 0x27 PlayStatusNotification: 0x%x with numField %d\n",notification,numField);
    #endif
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
    #ifdef DEBUG_MODE
    _debugSerial.printf("TX: L0x04 0x27 PlayStatusNotification: 0x%x\n",notification);
    #endif
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
    #ifdef DEBUG_MODE
    _debugSerial.printf("TX: L0x04 0x2D ReturnShuffle: 0x%x\n",currentShuffleStatus);
    #endif
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
    #ifdef DEBUG_MODE
    _debugSerial.printf("TX: L0x04 0x30 ReturnRepeat: 0x%x\n",currentRepeatStatus);
    #endif
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
    #ifdef DEBUG_MODE
    _debugSerial.printf("TX: L0x04 0x36 ReturnNumPlayingTracks: %d\n",numPlayingTracks);
    #endif
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

/// @brief This function processes a shortened byteArray packet identified as a valid Lingo 0x00 request
/// @param byteArray Shortened packet, with byteArray[0] being the Lingo 0x00 command ID byte
/// @param len Length of valid data in the byteArray
void esPod::processLingo0x00(const byte *byteArray, uint32_t len)
{
    byte cmdID = byteArray[0];
    #ifdef DEBUG_MODE
    _debugSerial.printf("RX: L0x00 CMD 0x%x\t",cmdID);
    #endif
    //Switch through expected commandIDs
    switch (cmdID)
    { 
    case L0x00_RequestExtendedInterfaceMode: //Mini requests extended interface mode status
        {
            #ifdef DEBUG_MODE
            _debugSerial.println("RequestExtendedInterfaceMode");
            #endif
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
            #ifdef DEBUG_MODE
            _debugSerial.println("EnterExtendedInterfaceMode");
            #endif
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
            #ifdef DEBUG_MODE
            _debugSerial.println("RequestiPodName");
            #endif
            L0x00_0x08_ReturniPodName();
        }
        break;
    
    case L0x00_RequestiPodSoftwareVersion: //Mini requests ipod software version
        {
            #ifdef DEBUG_MODE
            _debugSerial.println("RequestiPodSoftwareVersion");
            #endif
            L0x00_0x0A_ReturniPodSoftwareVersion();
        }
        break;
    
    case L0x00_RequestiPodSerialNum: //Mini requests ipod Serial Num
        {
            #ifdef DEBUG_MODE
            _debugSerial.println("RequestiPodSerialNum");
            #endif
            L0x00_0x0C_ReturniPodSerialNum();
        }
        break;
    
    case L0x00_RequestiPodModelNum: //Mini requests ipod Model Num
        {
            #ifdef DEBUG_MODE
            _debugSerial.println("RequestiPodModelNum");
            #endif
            L0x00_0x0E_ReturniPodModelNum();
        }
        break;
    
    case L0x00_RequestLingoProtocolVersion: //Mini requestsLingo Protocol Version
        {
            #ifdef DEBUG_MODE
            _debugSerial.println("RequestLingoProtocolVersion");
            #endif
            L0x00_0x10_ReturnLingoProtocolVersion(byteArray[1]);
        }
        break;
    
    case L0x00_IdentifyDeviceLingoes: //Mini identifies its lingoes, used as an ice-breaker
        {
            #ifdef DEBUG_MODE
            _debugSerial.println("IdentifyDeviceLingoes");
            #endif
            L0x00_0x02_iPodAck(iPodAck_OK,cmdID);//Acknowledge, start capabilities pingpong
            L0x00_0x27_GetAccessoryInfo(0x00); //Immediately request general capabilities
        }
        break;
    
    case L0x00_RetAccessoryInfo: //Mini returns info after L0x00_0x27
        {
            #ifdef DEBUG_MODE
            _debugSerial.printf("RetAccessoryInfo: 0x%x\n",byteArray[1]);
            #endif
            switch (byteArray[1]) //Ping-pong the next request based on the current response
        {
        case 0x00:
            _accessoryCapabilitiesRequested = true;
            L0x00_0x27_GetAccessoryInfo(0x01); //Request the name
            break;

        case 0x01:
            _accessoryNameRequested = true;
            L0x00_0x27_GetAccessoryInfo(0x04); //Request the firmware version
            break;

        case 0x04:
            _accessoryFirmwareRequested = true;
            L0x00_0x27_GetAccessoryInfo(0x05); //Request the hardware number
            break;

        case 0x05:
            _accessoryHardwareRequested = true;
            L0x00_0x27_GetAccessoryInfo(0x06); //Request the manufacturer name
            break;

        case 0x06:
            _accessoryManufRequested = true;
            L0x00_0x27_GetAccessoryInfo(0x07); //Request the model number
            break;

        case 0x07:
            _accessoryModelRequested = true; //End of the reactionchain
            #ifdef DEBUG_MODE
            _debugSerial.println("Handshake complete");
            #endif
            break;

        }
        }
        break;
    
    default: //In case the command is not known
        {
            #ifdef DEBUG_MODE
        _debugSerial.println("CMD ID UNKNOWN");
            #endif
            L0x00_0x02_iPodAck(byteArray[0],iPodAck_CmdFailed);
        }
        break;
    }
}

//-----------------------------------------------------------------------
//|                     Process Lingo 0x04 Requests                     |
//-----------------------------------------------------------------------

/// @brief This function processes a shortened byteArray packet identified as a valid Lingo 0x04 request
/// @param byteArray Shortened packet, with byteArray[1] being the last byte of the Lingo 0x04 command
/// @param len Length of valid data in the byteArray
void esPod::processLingo0x04(const byte *byteArray, uint32_t len)
{
    byte cmdID = byteArray[1];
    #ifdef DEBUG_MODE
    _debugSerial.printf("RX: CMD 0x00%x\t",cmdID);
    #endif
    //Initialising handlers to understand what is happening in some parts of the switch. They cannot be initialised in the switch-case scope
    byte category;
    uint32_t startIndex, counts, tempTrackIndex;

    if(!extendedInterfaceModeActive)   { //Complain if not in extended interface mode
        #ifdef DEBUG_MODE
        _debugSerial.println("Device NOT in extendedInterfaceMode!");
        #endif
        L0x04_0x01_iPodAck(iPodAck_BadParam,cmdID);
    }
    //Good to go if in Extended Interface mode
    else    {
        switch (cmdID) // Reminder : we are technically switching on byteArray[1] now
        {
        case L0x04_GetIndexedPlayingTrackInfo:
            {
                tempTrackIndex = swap_endian<uint32_t>(*((uint32_t*)&byteArray[3]));
                #ifdef DEBUG_MODE
                _debugSerial.printf("GetIndexedPlayingTrackInfo Index %d Info %x\n",tempTrackIndex,byteArray[2]);
                #endif
                switch (byteArray[2]) //Switch on the type of track info requested (careful with overloads)
                {
                case 0x00: //General track Capabilities and Information
                    if(tempTrackIndex==prevTrackIndex) {
                        #ifdef DEBUG_MODE
                        _debugSerial.println("\tIndex==prevTrackIndex");
                        #endif
                        L0x04_0x0D_ReturnIndexedPlayingTrackInfo((uint32_t)prevTrackDuration);
                    }
                    else {
                        #ifdef DEBUG_MODE
                        _debugSerial.println("\tIndex!=prevTrackIndex");
                        #endif
                        L0x04_0x0D_ReturnIndexedPlayingTrackInfo((uint32_t)trackDuration);
                    }
                    break;
                case 0x02: //Track Release Date (fictional)
                    L0x04_0x0D_ReturnIndexedPlayingTrackInfo(byteArray[2],(uint16_t)2001);
                    break;
                case 0x01: //Track Title
                    if(tempTrackIndex==prevTrackIndex) {
                        #ifdef DEBUG_MODE
                        _debugSerial.println("\tIndex==prevTrackIndex");
                        #endif
                        L0x04_0x0D_ReturnIndexedPlayingTrackInfo(byteArray[2],prevTrackTitle);
                    }
                    else {
                        #ifdef DEBUG_MODE
                        _debugSerial.println("\tIndex!=prevTrackIndex");
                        #endif
                        L0x04_0x0D_ReturnIndexedPlayingTrackInfo(byteArray[2],trackTitle);
                    }
                    break;
                case 0x05: //Track Genre
                    L0x04_0x0D_ReturnIndexedPlayingTrackInfo(byteArray[2],trackGenre);
                    break; 
                case 0x06: //Track Composer
                    L0x04_0x0D_ReturnIndexedPlayingTrackInfo(byteArray[2],composer);
                    break; 
                default: //In case the request is beyond the track capabilities
                    L0x04_0x01_iPodAck(iPodAck_BadParam,cmdID);
                    break;
                }
            }
            break;
        
        case L0x04_RequestProtocolVersion: //Hardcoded return for L0x04
            {
                #ifdef DEBUG_MODE
                _debugSerial.println("RequestProtocolVersion");
                #endif
                L0x04_0x13_ReturnProtocolVersion();
            }
            break;

        case L0x04_ResetDBSelection: //Not sure what to do here. Reset Current Track Index ?
            {
                #ifdef DEBUG_MODE
                _debugSerial.println("ResetDBSelection");
                #endif
                L0x04_0x01_iPodAck(iPodAck_OK,cmdID);
            }
            break;

        case L0x04_SelectDBRecord: //Used for browsing ?
            {
                #ifdef DEBUG_MODE
                _debugSerial.println("SelectDBRecord");
                #endif
                L0x04_0x01_iPodAck(iPodAck_OK,cmdID);
            }
            break;
        
        case L0x04_GetNumberCategorizedDBRecords: //Mini requests the number of records for a specific DB_CAT
            {
                category = byteArray[2];
                #ifdef DEBUG_MODE
                _debugSerial.printf("GetNumberCategorizedDBRecords category %x\n",category);
                #endif
                if(category == DB_CAT_TRACK) { // Say there are fixed, large amount of tracks
                    L0x04_0x19_ReturnNumberCategorizedDBRecords(totalNumberTracks);
                }
                else { //And only one of anything else (Playlist, album, artist etc...)
                    L0x04_0x19_ReturnNumberCategorizedDBRecords(1);
                }
            }
            break;
        
        case L0x04_RetrieveCategorizedDatabaseRecords: //Loops through the desired records for a given DB_CAT
            {
                category = byteArray[2]; //DBCat
                startIndex = swap_endian<uint32_t>(*(uint32_t*)&byteArray[3]);
                counts = swap_endian<uint32_t>(*(uint32_t*)&byteArray[7]);
                #ifdef DEBUG_MODE
                _debugSerial.printf("RetrieveCategorizedDatabaseRecords DBCat %x start %d counts %d\n",category,startIndex,counts);
                #endif
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
                #ifdef DEBUG_MODE
                _debugSerial.println("GetPlayStatus");
                #endif
                L0x04_0x1D_ReturnPlayStatus(playPosition,trackDuration,playStatus);
            }
            break;

        case L0x04_GetCurrentPlayingTrackIndex: //Get the uint32 index of the currently playing song
            {
                #ifdef DEBUG_MODE
                _debugSerial.println("GetCurrentPlayingTrackIndex");
                #endif
                L0x04_0x1F_ReturnCurrentPlayingTrackIndex(currentTrackIndex);
            }
            break;

        case L0x04_GetIndexedPlayingTrackTitle: 
            {
                tempTrackIndex = swap_endian<uint32_t>(*((uint32_t*)&byteArray[2]));
                #ifdef DEBUG_MODE
                _debugSerial.printf("GetIndexedPlayingTrackTitle Index: %d\n",tempTrackIndex);
                #endif
                if(tempTrackIndex==prevTrackIndex) {
                    #ifdef DEBUG_MODE
                        _debugSerial.println("\tIndex==prevTrackIndex");
                    #endif
                    L0x04_0x21_ReturnIndexedPlayingTrackTitle(prevTrackTitle);
                }
                else {
                    #ifdef DEBUG_MODE
                        _debugSerial.println("\tIndex!=prevTrackIndex");
                    #endif
                    L0x04_0x21_ReturnIndexedPlayingTrackTitle(trackTitle);
                }
                
            }
            break;

        case L0x04_GetIndexedPlayingTrackArtistName: 
            {
                tempTrackIndex = swap_endian<uint32_t>(*((uint32_t*)&byteArray[2]));
                #ifdef DEBUG_MODE
                _debugSerial.printf("GetIndexedPlayingTrackArtistName Index: %d\n",tempTrackIndex);
                #endif
                if(tempTrackIndex==prevTrackIndex) {
                    #ifdef DEBUG_MODE
                        _debugSerial.println("\tIndex==prevTrackIndex");
                    #endif
                    L0x04_0x23_ReturnIndexedPlayingTrackArtistName(prevArtistName);
                }
                else {
                    #ifdef DEBUG_MODE
                        _debugSerial.println("\tIndex!=prevTrackIndex");
                    #endif
                    L0x04_0x23_ReturnIndexedPlayingTrackArtistName(artistName);
                }
                
            }
            break;

        case L0x04_GetIndexedPlayingTrackAlbumName: 
            {
                tempTrackIndex = swap_endian<uint32_t>(*((uint32_t*)&byteArray[2]));
                #ifdef DEBUG_MODE
                _debugSerial.printf("GetIndexedPlayingTrackAlbumNameIndex: %d\n",tempTrackIndex);
                #endif
                if(tempTrackIndex==prevTrackIndex) {
                    #ifdef DEBUG_MODE
                        _debugSerial.println("\tIndex==prevTrackIndex");
                    #endif
                    L0x04_0x25_ReturnIndexedPlayingTrackAlbumName(prevAlbumName);
                }
                else {
                    #ifdef DEBUG_MODE
                        _debugSerial.println("\tIndex!=prevTrackIndex");
                    #endif
                    L0x04_0x25_ReturnIndexedPlayingTrackAlbumName(albumName);
                }
                
            }
            break;

        case L0x04_SetPlayStatusChangeNotification: //Turns on basic notifications
            {
                playStatusNotificationState = byteArray[2];
                #ifdef DEBUG_MODE
                _debugSerial.printf("SetPlayStatusChangeNotification %x\n",playStatusNotificationState);
                #endif
                L0x04_0x01_iPodAck(iPodAck_OK,cmdID);
            }
            break;

        case L0x04_PlayCurrentSelection: //Used to play a specific index, usually for "next" commands, but may be used to actually jump anywhere
            {
                tempTrackIndex = swap_endian<uint32_t>(*((uint32_t*)&byteArray[2]));
                #ifdef DEBUG_MODE
                    _debugSerial.printf("PlayCurrentSelection target %d \n",tempTrackIndex);
                #endif
                if(playStatus!=PB_STATE_PLAYING) {
                    playStatus = PB_STATE_PLAYING; //Playing status forced 
                    if(_playStatusHandler) {
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
                    #ifdef DEBUG_MODE
                        _debugSerial.printf("\t PREV detected, prev Index %d new Index %d pos %d \n",prevTrackIndex,currentTrackIndex,trackListPosition);
                    #endif

                    //Fire the A2DP when ready
                    if(_playStatusHandler) _playStatusHandler(A2DP_PREV); //Fire the metadata trigger indirectly
                    
                    //Engage the pending ACK for expected metadata
                    L0x04_0x01_iPodAck(iPodAck_CmdPending,cmdID,TRACK_CHANGE_TIMEOUT);
                    trackChangeAckPending = cmdID;
                    trackChangeTimestamp = millis();
                }
                else if (tempTrackIndex == currentTrackIndex) //Somehow reselecting the current track
                {
                    #ifdef DEBUG_MODE
                        _debugSerial.printf("\t SAME detected, Index %d \n",currentTrackIndex);
                    #endif
                    //Fire the A2DP when ready
                    if(_playStatusHandler) _playStatusHandler(A2DP_PREV); //Fire the metadata trigger indirectly

                    L0x04_0x01_iPodAck(iPodAck_OK,cmdID);
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
                    #ifdef DEBUG_MODE
                        _debugSerial.printf("\t NEXT detected, prev Index %d new Index %d pos %d\n",prevTrackIndex,currentTrackIndex,trackListPosition);
                    #endif

                    //Fire the A2DP when ready
                    if(_playStatusHandler) _playStatusHandler(A2DP_NEXT); //Fire the metadata trigger indirectly

                    //Engage the pending ACK for expected metadata
                    L0x04_0x01_iPodAck(iPodAck_CmdPending,cmdID,TRACK_CHANGE_TIMEOUT);
                    trackChangeAckPending = cmdID;
                    trackChangeTimestamp = millis();
                }
            }
            break;
        
        case L0x04_PlayControl: //Basic play control. Used for Prev, pause and play
            {                   //TODO : trackIndex trickery and pending metadata update management ?
                #ifdef DEBUG_MODE
                _debugSerial.printf("PlayControl %x\n",byteArray[2]);
                #endif
                switch (byteArray[2]) //PlayControl byte
                {
                case PB_CMD_TOGGLE: //Just Toggle or start playing
                    {
                        if(playStatus==PB_STATE_PLAYING) {
                            playStatus=PB_STATE_PAUSED; //Toggle to paused if playing
                            if(_playStatusHandler) _playStatusHandler(A2DP_PAUSE);
                        }
                        else {
                            playStatus = PB_STATE_PLAYING; //Switch to playing in any other case
                            if(_playStatusHandler) _playStatusHandler(A2DP_PLAY);
                        }
                    }
                    break;
                case PB_CMD_STOP: //Stop
                    {
                        playStatus = PB_STATE_STOPPED;
                        if(_playStatusHandler) _playStatusHandler(A2DP_STOP);
                    }
                    break;
                case PB_CMD_NEXT_TRACK: //Next track.. never seems to happen ?
                    {
                        if(_playStatusHandler) _playStatusHandler(A2DP_NEXT);
                        waitMetadataUpdate = true;//Tells the system we are expecting new metadata (that has to be checked for consistency and duplicates)
                        // Note about manipulation of currentTrackIndex, trackList and trackListPosition : it should happen outside of this case and should provoke the right notification
                    }
                    break;
                case PB_CMD_PREVIOUS_TRACK: //Prev track
                    {
                        if(_playStatusHandler) _playStatusHandler(A2DP_PREV);
                        waitMetadataUpdate = true;//Tells the system we are expecting new metadata (that has to be checked for consistency and duplicates)
                        // Note about manipulation of currentTrackIndex, trackList and trackListPosition : it should happen outside of this case and should provoke the right notification
                    }
                    break;
                case PB_CMD_NEXT: //Next track
                    {
                        if(_playStatusHandler) _playStatusHandler(A2DP_NEXT);
                        waitMetadataUpdate = true;//Tells the system we are expecting new metadata (that has to be checked for consistency and duplicates)
                        // Note about manipulation of currentTrackIndex, trackList and trackListPosition : it should happen outside of this case and should provoke the right notification
                    }
                    break;
                case PB_CMD_PREV: //Prev track
                    {
                        if(_playStatusHandler) _playStatusHandler(A2DP_PREV);
                        waitMetadataUpdate = true;//Tells the system we are expecting new metadata (that has to be checked for consistency and duplicates)
                        // Note about manipulation of currentTrackIndex, trackList and trackListPosition : it should happen outside of this case and should provoke the right notification
                    }
                    break;
                case PB_CMD_PLAY: //Play
                    {
                        playStatus = PB_STATE_PLAYING;
                        if(_playStatusHandler) _playStatusHandler(A2DP_PLAY);
                        waitMetadataUpdate = true;
                    }
                    break;
                case PB_CMD_PAUSE: //Pause
                    {
                        playStatus = PB_STATE_PAUSED;
                        if(_playStatusHandler) _playStatusHandler(A2DP_PAUSE);
                        waitMetadataUpdate = true;
                    }
                    break;
                }
                L0x04_0x01_iPodAck(iPodAck_OK,cmdID);
                if((playStatus = PB_STATE_STOPPED)&&(playStatusNotificationState==NOTIF_ON)) L0x04_0x27_PlayStatusNotification(0x00); //Notify successful stop
            }   
            break;

        case L0x04_GetShuffle: //Get Shuffle state from the PB Engine
            {
                #ifdef DEBUG_MODE
                _debugSerial.println("GetShuffle");
                #endif
                L0x04_0x2D_ReturnShuffle(shuffleStatus);
            }
            break;

        case L0x04_SetShuffle: //Set Shuffle state
            {
                #ifdef DEBUG_MODE
                _debugSerial.println("SetShuffle");
                #endif
                shuffleStatus = byteArray[2];
                L0x04_0x01_iPodAck(iPodAck_OK,cmdID);
            }
            break;
        
        case L0x04_GetRepeat: //Get Repeat state
            {
                #ifdef DEBUG_MODE
                _debugSerial.println("GetRepeat");
                #endif
                L0x04_0x30_ReturnRepeat(repeatStatus);
            }
            break;

        case L0x04_SetRepeat: //Set Repeat state
            {
                #ifdef DEBUG_MODE
                _debugSerial.println("SetRepeat");
                #endif
                repeatStatus = byteArray[2];
                L0x04_0x01_iPodAck(iPodAck_OK,cmdID);
            }
            break;

        case L0x04_GetNumPlayingTracks: //Systematically return TOTAL_NUM_TRACKS
            {
                #ifdef DEBUG_MODE
                _debugSerial.println("GetNumPlayingTracks");
                #endif
                L0x04_0x36_ReturnNumPlayingTracks(totalNumberTracks); //We say there are two playing tracks
            }
            break;

        case L0x04_SetCurrentPlayingTrack: //Basically identical to PlayCurrentSelection
                {
                tempTrackIndex = swap_endian<uint32_t>(*((uint32_t*)&byteArray[2]));
                #ifdef DEBUG_MODE
                    _debugSerial.printf("SetCurrentPlayingTrack target %d \n",tempTrackIndex);
                #endif
                if(playStatus!=PB_STATE_PLAYING) {
                    playStatus = PB_STATE_PLAYING; //Playing status forced 
                    if(_playStatusHandler) {
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
                    #ifdef DEBUG_MODE
                        _debugSerial.printf("\t PREV detected, prev Index %d new Index %d pos %d \n",prevTrackIndex,currentTrackIndex,trackListPosition);
                    #endif

                    //Fire the A2DP when ready
                    if(_playStatusHandler) _playStatusHandler(A2DP_PREV); //Fire the metadata trigger indirectly
                    
                    //Engage the pending ACK for expected metadata
                    L0x04_0x01_iPodAck(iPodAck_CmdPending,cmdID,TRACK_CHANGE_TIMEOUT);
                    trackChangeAckPending = cmdID;
                    trackChangeTimestamp = millis();
                }
                else if (tempTrackIndex == currentTrackIndex) //Somehow reselecting the current track
                {
                    #ifdef DEBUG_MODE
                        _debugSerial.printf("\t SAME detected, Index %d \n",currentTrackIndex);
                    #endif
                    //Fire the A2DP when ready
                    if(_playStatusHandler) _playStatusHandler(A2DP_PREV); //Fire the metadata trigger indirectly

                    L0x04_0x01_iPodAck(iPodAck_OK,cmdID);
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
                    #ifdef DEBUG_MODE
                        _debugSerial.printf("\t NEXT detected, prev Index %d new Index %d pos %d\n",prevTrackIndex,currentTrackIndex,trackListPosition);
                    #endif

                    //Fire the A2DP when ready
                    if(_playStatusHandler) _playStatusHandler(A2DP_NEXT); //Fire the metadata trigger indirectly

                    //Engage the pending ACK for expected metadata
                    L0x04_0x01_iPodAck(iPodAck_CmdPending,cmdID,TRACK_CHANGE_TIMEOUT);
                    trackChangeAckPending = cmdID;
                    trackChangeTimestamp = millis();
                }
            }
            break;
        }
    }
}

//-----------------------------------------------------------------------
//|                            Packet Disneyland                        |
//-----------------------------------------------------------------------

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
        #ifdef DEBUG_MODE
        _debugSerial.print("Lingo 0x00 ");
        #endif
        processLingo0x00(subPayload,subPayloadLen);
        break;
    
    case 0x04: // Extended Interface Lingo
        #ifdef DEBUG_MODE
        _debugSerial.print("Lingo 0x04 ");
        #endif
        processLingo0x04(subPayload,subPayloadLen);
        break;
    
    default:
        #ifdef DEBUG_MODE
        _debugSerial.printf("Unknown Lingo : 0x%x ",rxLingoID);
        #endif
        break;
    }
}

/// @brief Refresh function for the esPod : listens to Serial, assembles packets, or ignores everything if it is disabled.
void esPod::refresh()
{
    //Check for a new packet and update the buffer
    while(_targetSerial.available()) {
        byte incomingByte = _targetSerial.read();
        lastConnected = millis()/1000;
        if(!disabled) {
            //A new 0xFF55 packet starter shows up
            if(_prevRxByte == 0xFF && incomingByte == 0x55) { 
                _rxLen = 0; //Reset the received length
                _rxCounter = 0; //Reset the counter to the end of payload
            }
            // Packet start was detected, but Length was not passed yet (this sort of works for 3-bytes length fields)
            else if(_rxLen == 0 && _rxCounter == 0) {
                _rxLen = incomingByte;
            }
            // Just plop in in the buffer
            else {
                _rxBuf[_rxCounter++] = incomingByte;
                if(_rxCounter == _rxLen+1) { //We are done receiving the packet
                    byte tempChecksum = esPod::checksum(_rxBuf, _rxLen);
                    if (tempChecksum == _rxBuf[_rxLen]) { //Checksum checks out
                        processPacket(_rxBuf,_rxLen);  
                        #ifdef DEBUG_MODE
                            #ifdef DUMP_MODE
                            _debugSerial.print("RX ");
                            for(int i = 0;i<_rxLen;i++) {
                                _debugSerial.print(_rxBuf[i],HEX);
                                _debugSerial.print(" ");
                            }
                            _debugSerial.print(" ");
                            _debugSerial.println(_rxBuf[_rxLen],HEX);
                            #endif
                        #endif
                    }
                    #ifdef DEBUG_MODE
                    else {
                        _debugSerial.println("Checksum failed");
                    }
                    #endif
                }
            }

            //pass to the previous received byte
            _prevRxByte = incomingByte;
        }
    }

    //Reset if no message received in the last 120s
    if((millis()/1000)-lastConnected > 120) {
        resetState();
    }

    //Send the track change Ack Pending if it has not sent already
    if(!disabled && (trackChangeAckPending>0x00) && (millis()>(trackChangeTimestamp+TRACK_CHANGE_TIMEOUT))) {
        L0x04_0x01_iPodAck(iPodAck_OK,trackChangeAckPending);
        trackChangeAckPending = 0x00;
    }

}

