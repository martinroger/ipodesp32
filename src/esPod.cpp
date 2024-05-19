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


esPod::esPod(Stream& targetSerial) 
    :
        #ifdef DEBUG_MODE
        _debugSerial(Serial),
        #endif
        _targetSerial(targetSerial),
        _rxLen(0),
        _rxCounter(0),
        _extendedInterfaceModeActive(false),
        _name("ipodESP32"),
        _SWMajor(0x01),
        _SWMinor(0x03),
        _SWrevision(0x00),
        _serialNumber("AB345F7HIJK"),
        _playStatusHandler(0)
{
    //Setup the metadata
    //_SWMajor = 0x01;
    //_SWMinor = 0x03;
    //_SWrevision = 0x00;
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

void esPod::L0x00_0x02_iPodAck(byte cmdStatus,byte cmdID) {
    #ifdef DEBUG_MODE
    _debugSerial.print("L0x00 0x02 iPodAck: ");
    _debugSerial.print(cmdStatus,HEX);
    _debugSerial.print(" cmd: ");
    _debugSerial.println(cmdID,HEX);
    #endif
    const byte txPacket[] = {
        0x00,
        0x02,
        cmdStatus,
        cmdID
    };

    sendPacket(txPacket,sizeof(txPacket));
}

void esPod::L0x00_0x02_iPodAck_pending(uint32_t pendingDelayMS,byte cmdID) {
    #ifdef DEBUG_MODE
    _debugSerial.print("L0x00 0x02 iPodAck pending: ");
    _debugSerial.print(pendingDelayMS);
    _debugSerial.print(" cmd: ");
    _debugSerial.println(cmdID,HEX);
    #endif
    byte txPacket[20] = {
        0x00,
        0x02,
        iPodAck_CmdPending,
        cmdID
    };

    *((uint32_t*)&txPacket[4]) = swap_endian<uint32_t>(pendingDelayMS);
    sendPacket(txPacket,4+4);
}

void esPod::L0x00_0x04_ReturnExtendedInterfaceMode(byte extendedModeByte) {
    #ifdef DEBUG_MODE
    _debugSerial.print("L0x00 0x04 ReturnExtendedInterfaceMode: ");
    _debugSerial.println(extendedModeByte,HEX);
    #endif
    const byte txPacket[] = {
        0x00,
        0x04,
        extendedModeByte
    };

    sendPacket(txPacket,sizeof(txPacket));
}

void esPod::L0x00_0x08_ReturniPodName() {
    #ifdef DEBUG_MODE
    _debugSerial.print("L0x00 0x08 ReturniPodName: ");
    _debugSerial.println(_name);
    //_debugSerial.println(sizeof(_name));
    _debugSerial.println(strlen(_name));
    #endif
    //char _nameProxy[] = "stuff";
    byte txPacket[255] = { //Prealloc to len = FF
        0x00,
        0x08
    };
    //*((char*)&txPacket[2]) = *_name; //BOH
    //strncpy((char*)&txPacket[2],_name,strlen(_name));
    strcpy((char*)&txPacket[2],_name);
    sendPacket(txPacket,3+strlen(_name));
}

void esPod::L0x00_0x0A_ReturniPodSoftwareVersion() {
    #ifdef DEBUG_MODE
    _debugSerial.println("L0x00 0x0A ReturniPodSoftwareVersion: 1.3.0 ");
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

void esPod::L0x00_0x0C_ReturniPodSerialNum() {
    #ifdef DEBUG_MODE
    _debugSerial.print("L0x00 0x0C ReturniPodSerialNum: ");
    _debugSerial.println(_serialNumber);
    _debugSerial.println(strlen(_serialNumber));
    #endif
    byte txPacket[255] = { //Prealloc to len = FF
        0x00,
        0x0C
    };
    strcpy((char*)&txPacket[2],_serialNumber);
    sendPacket(txPacket,3+strlen(_serialNumber));
}

void esPod::L0x00_0x0E_ReturniPodModelNum() {
    #ifdef DEBUG_MODE
    _debugSerial.println("L0x00 0x0E ReturniPodModelNumber PA146FD 720901 ");
    #endif
    byte txPacket[] = {
        0x00,
        0x0E,
        0x00,0x0B,0x00,0x05,0x50,0x41,0x31,0x34,0x36,0x46,0x44,0x00
    };
    sendPacket(txPacket,sizeof(txPacket));
}

void esPod::L0x00_0x10_ReturnLingoProtocolVersion(byte targetLingo)
{
    #ifdef DEBUG_MODE
    _debugSerial.print("L0x00 0x10 ReturnLingoProtocolVersion for: ");
    _debugSerial.println(targetLingo,HEX);
    #endif
    byte txPacket[] = {
        0x00, 0x10,
        targetLingo,
        0x01,0x00
    };
    switch (targetLingo)
    {
    case 0x00:
        txPacket[4] = 0x06;
        break;
    case 0x03:
        txPacket[4] = 0x05;
        break;
    case 0x04:
        txPacket[4] = 0x0C;
        break;
    default:
        break;
    }
    sendPacket(txPacket,sizeof(txPacket));
}

void esPod::L0x00_0x27_GetAccessoryInfo(byte desiredInfo)
{
    #ifdef DEBUG_MODE
    _debugSerial.print("L0x00 0x27 GetAccessoryInfo type: ");
    _debugSerial.println(desiredInfo,HEX);
    #endif
    byte txPacket[] = {
        0x00, 0x27,
        desiredInfo
    };
    sendPacket(txPacket,sizeof(txPacket));
}

//Moving on to L0x04

void esPod::L0x04_0x01_iPodAck(byte cmdStatus, byte cmdID)
{
    #ifdef DEBUG_MODE
    _debugSerial.print("L0x04 0x01 iPodAck: ");
    _debugSerial.print(cmdStatus,HEX);
    _debugSerial.print(" cmd: ");
    _debugSerial.println(cmdID,HEX);
    #endif
    const byte txPacket[] = {
        0x04,
        0x00,0x01,
        cmdStatus,
        0x00,cmdID
    };
    sendPacket(txPacket,sizeof(txPacket));
}

/// @brief Returns the pseudo-UTF8 string for the track info types 01/05/06
/// @param trackInfoType 
/// @param trackInfoChars 
void esPod::L0x04_0x0D_ReturnIndexedPlayingTrackInfo(byte trackInfoType, char* trackInfoChars)
{
    #ifdef DEBUG_MODE
    _debugSerial.print("L0x04 0x0D ReturnIndexedPlayingTrackInfo type: ");
    _debugSerial.println(trackInfoType,HEX);
    #endif
    byte txPacket[255] = {
        0x04,
        0x00,0x0D,
        trackInfoType
    };
    strcpy((char*)&txPacket[4],trackInfoChars);
    sendPacket(txPacket,4+strlen(trackInfoChars)+1);
}

/// @brief Works only for case 0x00 and 0x02
/// @param trackInfoType 
void esPod::L0x04_0x0D_ReturnIndexedPlayingTrackInfo(byte trackInfoType)
{
    #ifdef DEBUG_MODE
    _debugSerial.print("L0x04 0x0D ReturnIndexedPlayingTrackInfo type: ");
    _debugSerial.println(trackInfoType,HEX);
    #endif
    byte txPacket_00[14] = {
        0x04,
        0x00,0x0D,
        trackInfoType,
        0x00,0x00,0x00,0x00, //This says it does not have artwork etc
        0x00,0x00,0x00,0x00, //Track length in ms
        0x00,0x00 //Chapter count (none)
    };
    *((uint32_t*)&txPacket_00[8]) = swap_endian<uint32_t>(123456);

    byte txPacket_02[12] = {
        0x04,
        0x00,0x0D,
        trackInfoType,
        0x00,0x00,0x00,0x01,0x01, //First of Jan at 00:00:00
        0x00,0x00, //year goes here
        0x01 //it was a Monday
    };
    *((uint16_t*)&txPacket_02[9]) = swap_endian<uint16_t>(2001);
    
    switch (trackInfoType)
    {
    case 0x00:
        
        sendPacket(txPacket_00,sizeof(txPacket_00));
        break;
    
    case 0x02:

        sendPacket(txPacket_02,sizeof(txPacket_02));
        break;
    default:
        break;
    }
}

void esPod::L0x04_0x13_ReturnProtocolVersion()
{
    #ifdef DEBUG_MODE
    _debugSerial.println("L0x04 0x13 ReturnProtocolVersion");
    #endif
    byte txPacket[] = {
        0x04,
        0x00,0x13,
        0x01,0x0C //Protocol version 1.12
    };
    sendPacket(txPacket,sizeof(txPacket));
}

void esPod::L0x04_0x19_ReturnNumberCategorizedDBRecords(uint32_t categoryDBRecords)
{
    #ifdef DEBUG_MODE
    _debugSerial.print("L0x04 0x19 ReturnNumberCategorizedDBRecords: ");
    _debugSerial.println(categoryDBRecords);
    #endif
    byte txPacket[7] = {
        0x04,
        0x00,0x19,
        0x00,0x00,0x00,0x00
    };
    *((uint32_t*)&txPacket[3]) = swap_endian<uint32_t>(categoryDBRecords);
    sendPacket(txPacket,sizeof(txPacket));

}

void esPod::L0x04_0x1B_ReturnCategorizedDatabaseRecord(uint32_t index, char *recordString)
{
    #ifdef DEBUG_MODE
    _debugSerial.print("L0x04 0x1B ReturnCategorizedDatabaseRecord at index: ");
    _debugSerial.print(index); _debugSerial.print(" record: ");
    _debugSerial.println(recordString);
    #endif
    byte txPacket[255] = {
        0x04,
        0x00,0x1B,
        0x00,0x00,0x00,0x00 //Index
    };
    *((uint32_t*)&txPacket[3]) = swap_endian<uint32_t>(index);
    strcpy((char*)&txPacket[7],recordString);
    sendPacket(txPacket,7+strlen(recordString)+1);
}

void esPod::L0x04_0x1D_ReturnPlayStatus(uint32_t position, uint32_t duration, byte playStatus)
{
    #ifdef DEBUG_MODE
    _debugSerial.print("L0x04 0x1D ReturnPlayStatus: ");_debugSerial.print(playStatus,HEX);_debugSerial.print(" at pos: ");
    _debugSerial.print(position); _debugSerial.print(" of ");
    _debugSerial.println(duration);
    #endif
    byte txPacket[] = {
        0x04,
        0x00,0x1D,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        playStatus
    };
    *((uint32_t*)&txPacket[3]) = swap_endian<uint32_t>(duration);
    *((uint32_t*)&txPacket[7]) = swap_endian<uint32_t>(position);
    sendPacket(txPacket,sizeof(txPacket));
}

void esPod::L0x04_0x1F_ReturnCurrentPlayingTrackIndex(uint32_t trackIndex)
{
    #ifdef DEBUG_MODE
    _debugSerial.print("L0x04 0x1F ReturnCurrentPlayingTrackIndex: ");
    _debugSerial.println(trackIndex);
    #endif
    byte txPacket[] = {
        0x04,
        0x00,0x1F,
        0x00,0x00,0x00,0x00
    };
    *((uint32_t*)&txPacket[3]) = swap_endian<uint32_t>(trackIndex);
    sendPacket(txPacket,sizeof(txPacket));
}

void esPod::L0x04_0x21_ReturnIndexedPlayingTrackTitle(char *trackTitle)
{
    #ifdef DEBUG_MODE
    _debugSerial.print("L0x04 0x21 ReturnIndexedPlayingTrackTitle: ");
    _debugSerial.println(trackTitle);
    #endif
    byte txPacket[255] = {
        0x04,
        0x00,0x21
    };
    strcpy((char*)&txPacket[3],trackTitle);
    sendPacket(txPacket,3+strlen(trackTitle)+1);
}

void esPod::L0x04_0x23_ReturnIndexedPlayingTrackArtistName(char *trackArtistName)
{
    #ifdef DEBUG_MODE
    _debugSerial.print("L0x04 0x23 ReturnIndexedPlayingTrackArtistName: ");
    _debugSerial.println(trackArtistName);
    #endif
    byte txPacket[255] = {
        0x04,
        0x00,0x23
    };
    strcpy((char*)&txPacket[3],trackArtistName);
    sendPacket(txPacket,3+strlen(trackArtistName)+1);
}

void esPod::L0x04_0x25_ReturnIndexedPlayingTrackAlbumName(char *trackAlbumName)
{
    #ifdef DEBUG_MODE
    _debugSerial.print("L0x04 0x23 ReturnIndexedPlayingTrackArtistName: ");
    _debugSerial.println(trackAlbumName);
    #endif
    byte txPacket[255] = {
        0x04,
        0x00,0x25
    };
    strcpy((char*)&txPacket[3],trackAlbumName);
    sendPacket(txPacket,3+strlen(trackAlbumName)+1);
}

/// @brief Only supports currently three types : 0x00 Stopped, 0x01 Track index, 0x04 Track offset
/// @param notification 
/// @param numField 
void esPod::L0x04_0x27_PlayStatusNotification(byte notification, uint32_t numField)
{
    #ifdef DEBUG_MODE
    _debugSerial.print("L0x04 0x27 PlayStatusNotification: ");
    _debugSerial.println(notification,HEX);
    #endif
    byte txPacket[] = {
        0x04,
        0x00,0x27,
        notification,
        0x00,0x00,0x00,0x00
    };
    switch (notification)
    {
    case 0x00:
        sendPacket(txPacket,4);
        break;
    case 0x01:
        *((uint32_t*)&txPacket[4]) = swap_endian<uint32_t>(numField);
        sendPacket(txPacket,sizeof(txPacket));
        break;
    case 0x04:
        *((uint32_t*)&txPacket[4]) = swap_endian<uint32_t>(numField);
        sendPacket(txPacket,sizeof(txPacket));
        break;
    default:
        break;
    }
}

void esPod::L0x04_0x2D_ReturnShuffle(byte shuffleStatus)
{
    #ifdef DEBUG_MODE
    _debugSerial.print("L0x04 0x2D ReturnShuffle: ");
    _debugSerial.println(shuffleStatus);
    #endif
    byte txPacket[] = {
        0x04,
        0x00,0x2D,
        shuffleStatus
    };
    sendPacket(txPacket,sizeof(txPacket));
}

void esPod::L0x04_0x30_ReturnRepeat(byte repeatStatus)
{
    #ifdef DEBUG_MODE
    _debugSerial.print("L0x04 0x30 ReturnRepeat: ");
    _debugSerial.println(repeatStatus);
    #endif
    byte txPacket[] = {
        0x04,
        0x00,0x30,
        repeatStatus
    };
    sendPacket(txPacket,sizeof(txPacket));
}

void esPod::L0x04_0x36_ReturnNumPlayingTracks(uint32_t numPlayingTracks)
{
    #ifdef DEBUG_MODE
    _debugSerial.print("L0x04 0x36 ReturnNumPlayingTracks: ");
    _debugSerial.println(numPlayingTracks);
    #endif
    byte txPacket[] = {
        0x04,
        0x00,0x36,
        0x00,0x00,0x00,0x00
    };
    *((uint32_t*)&txPacket[3]) = swap_endian<uint32_t>(numPlayingTracks);
    sendPacket(txPacket,sizeof(txPacket));
}

void esPod::processLingo0x00(const byte *byteArray, uint32_t len)
{
    byte cmdID = byteArray[0];
    #ifdef DEBUG_MODE
    _debugSerial.print("cmd ");
    _debugSerial.print(cmdID,HEX);
    _debugSerial.print(" ");
    #endif
    switch (cmdID)
    {
    case L0x00_RequestExtendedInterfaceMode: //Mini requests extended interface mode
        #ifdef DEBUG_MODE
        _debugSerial.println("RequestExtendedInterfaceMode");
        #endif
        if(_extendedInterfaceModeActive) {
            L0x00_0x04_ReturnExtendedInterfaceMode(0x01); //Report that extended interface mode is active
        }
        else
        {
            L0x00_0x04_ReturnExtendedInterfaceMode(0x00); //Report that extended interface mode is not active
        }
        break;

    case L0x00_EnterExtendedInterfaceMode: //Mini forces extended interface mode
        #ifdef DEBUG_MODE
        _debugSerial.println("EnterExtendedInterfaceMode");
        #endif
        if(!_extendedInterfaceModeActive) {
            //Send a first iPodAck Command pending with a certain time delay
            L0x00_0x02_iPodAck_pending(1000,cmdID);
            //Send a second iPodAck Command with Success
            _extendedInterfaceModeActive = true;
        }
        L0x00_0x02_iPodAck(iPodAck_OK,cmdID);
        break;
    
    case L0x00_RequestiPodName: //Mini requests ipod name
        #ifdef DEBUG_MODE
        _debugSerial.println("RequestiPodName");
        #endif
        L0x00_0x08_ReturniPodName();
        break;
    
    case L0x00_RequestiPodSoftwareVersion: //Mini requests ipod software version
        #ifdef DEBUG_MODE
        _debugSerial.println("RequestiPodSoftwareVersion");
        #endif
        L0x00_0x0A_ReturniPodSoftwareVersion();
        break;
    
    case L0x00_RequestiPodSerialNum: //Mini requests ipod Serial Num
        #ifdef DEBUG_MODE
        _debugSerial.println("RequestiPodSerialNum");
        #endif
        L0x00_0x0C_ReturniPodSerialNum();
        break;
    
    case L0x00_RequestiPodModelNum: //Mini requests ipod Model Num
        #ifdef DEBUG_MODE
        _debugSerial.println("RequestiPodModelNum");
        #endif
        L0x00_0x0E_ReturniPodModelNum();
        break;
    
    case L0x00_RequestLingoProtocolVersion: //Mini requestsLingo Protocol Version
        #ifdef DEBUG_MODE
        _debugSerial.println("RequestLingoProtocolVersion");
        #endif
        L0x00_0x10_ReturnLingoProtocolVersion(byteArray[1]);
        break;
    
    case L0x00_IdentifyDeviceLingoes: //Mini identifies its lingoes
        #ifdef DEBUG_MODE
        _debugSerial.println("IdentifyDeviceLingoes");
        #endif
        L0x00_0x02_iPodAck(iPodAck_OK,cmdID);//Not really relevant to do more
        break;
    
    case L0x00_RetAccessoryInfo: //Mini returns meta info
        #ifdef DEBUG_MODE
        _debugSerial.print("RetAccessoryInfo: ");
        _debugSerial.println(byteArray[1],HEX);
        #endif
        switch (byteArray[1])
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
            L0x00_0x27_GetAccessoryInfo(0x05); //Request the hardware
            break;
        case 0x05:
            _accessoryHardwareRequested = true;
            L0x00_0x27_GetAccessoryInfo(0x06); //Request the manufacturer
            break;
        case 0x06:
            _accessoryManufRequested = true;
            L0x00_0x27_GetAccessoryInfo(0x07); //Request the model
            break;
        case 0x07:
            _accessoryModelRequested = true; //End of the reactionchain
            break;
        default:
            break;
        }
        break;
    
    default:
        #ifdef DEBUG_MODE
        _debugSerial.println("CMD UNKNOWN");
        #endif
        break;
    }
}

void esPod::processLingo0x04(const byte *byteArray, uint32_t len)
{
    byte cmdID = byteArray[1];
    
    #ifdef DEBUG_MODE
    _debugSerial.print("cmd ");
    _debugSerial.print(cmdID,HEX);
    _debugSerial.print(" ");
    #endif

    byte category;
    uint32_t startIndex, counts;

    switch (cmdID)
    {
    case L0x04_GetIndexedPlayingTrackInfo:
        #ifdef DEBUG_MODE
        _debugSerial.println("GetIndexedPlayingTrackInfo");
        #endif
        switch (byteArray[2])
        {
        case 0x00: //TrackCapabilities and Information
            L0x04_0x0D_ReturnIndexedPlayingTrackInfo(byteArray[2]);
            break;
        case 0x02: //Track Release Date
            L0x04_0x0D_ReturnIndexedPlayingTrackInfo(byteArray[2]);
            break;
        case 0x01:
            L0x04_0x0D_ReturnIndexedPlayingTrackInfo(byteArray[2],"PodcastName");
            break;
        case 0x05:
            L0x04_0x0D_ReturnIndexedPlayingTrackInfo(byteArray[2],_trackGenre);
            break; 
        case 0x06:
            L0x04_0x0D_ReturnIndexedPlayingTrackInfo(byteArray[2],"Composer");
            break; 
        default:
            L0x04_0x01_iPodAck(iPodAck_BadParam,cmdID);
            break;
        }
        break;
    
    case L0x04_RequestProtocolVersion:
        #ifdef DEBUG_MODE
        _debugSerial.println("RequestProtocolVersion");
        #endif
        L0x04_0x13_ReturnProtocolVersion();
        break;

    case L0x04_ResetDBSelection:
        #ifdef DEBUG_MODE
        _debugSerial.println("ResetDBSelection");
        #endif
        L0x04_0x01_iPodAck(iPodAck_OK,cmdID);
        break;

    case L0x04_SelectDBRecord:
        #ifdef DEBUG_MODE
        _debugSerial.println("SelectDBRecord");
        #endif
        L0x04_0x01_iPodAck(iPodAck_OK,cmdID);
        break;
    
    case L0x04_GetNumberCategorizedDBRecords:
        #ifdef DEBUG_MODE
        _debugSerial.println("GetNumberCategorizedDBRecords");
        #endif
        L0x04_0x19_ReturnNumberCategorizedDBRecords(1);
        break;
    
    case L0x04_RetrieveCategorizedDatabaseRecords:
        #ifdef DEBUG_MODE
        _debugSerial.println("RetrieveCategorizedDatabaseRecords");
        #endif
        category = byteArray[2];
        startIndex = swap_endian<uint32_t>(*(uint32_t*)&byteArray[3]);
        counts = swap_endian<uint32_t>(*(uint32_t*)&byteArray[7]);
        switch (category)
        {
        case 0x01:
            for (uint32_t i = startIndex; i < startIndex +counts; i++)
            {
                L0x04_0x1B_ReturnCategorizedDatabaseRecord(i,_playList);
            }
            break;
        case 0x02:
            for (uint32_t i = startIndex; i < startIndex +counts; i++)
            {
                L0x04_0x1B_ReturnCategorizedDatabaseRecord(i,_artistName);
            }
            break;
        case 0x03:
            for (uint32_t i = startIndex; i < startIndex +counts; i++)
            {
                L0x04_0x1B_ReturnCategorizedDatabaseRecord(i,_albumName);
            }
            break;
        case 0x04:
            for (uint32_t i = startIndex; i < startIndex +counts; i++)
            {
                L0x04_0x1B_ReturnCategorizedDatabaseRecord(i,_trackGenre);
            }
            break;
        case 0x05:
            for (uint32_t i = startIndex; i < startIndex +counts; i++)
            {
                L0x04_0x1B_ReturnCategorizedDatabaseRecord(i,_trackTitle);
            }
            break;
        case 0x06:
            for (uint32_t i = startIndex; i < startIndex +counts; i++)
            {
                L0x04_0x1B_ReturnCategorizedDatabaseRecord(i,_composer);
            }
            break;
        default:
            break;
        }
        break;

    case L0x04_GetPlayStatus:
        #ifdef DEBUG_MODE
        _debugSerial.println("GetPlayStatus");
        #endif
        L0x04_0x1D_ReturnPlayStatus(60000,120000,_playStatus);
        break;

    case L0x04_GetCurrentPlayingTrackIndex:
        #ifdef DEBUG_MODE
        _debugSerial.println("GetCurrentPlayingTrackIndex");
        #endif
        L0x04_0x1F_ReturnCurrentPlayingTrackIndex(0);
        break;

    case L0x04_GetIndexedPlayingTrackTitle:
        #ifdef DEBUG_MODE
        _debugSerial.println("GetIndexedPlayingTrackTitle");
        #endif
        L0x04_0x21_ReturnIndexedPlayingTrackTitle(_trackTitle);
        break;

    case L0x04_GetIndexedPlayingTrackArtistName:
        #ifdef DEBUG_MODE
        _debugSerial.println("GetIndexedPlayingTrackArtistName");
        #endif
        L0x04_0x23_ReturnIndexedPlayingTrackArtistName(_artistName);
        break;

    case L0x04_GetIndexedPlayingTrackAlbumName:
        #ifdef DEBUG_MODE
        _debugSerial.println("GetIndexedPlayingTrackAlbumName");
        #endif
        L0x04_0x25_ReturnIndexedPlayingTrackAlbumName(_albumName);
        break;

    case L0x04_SetPlayStatusChangeNotification: //iPod seems to only send track index and track time offset
        #ifdef DEBUG_MODE
        _debugSerial.println("SetPlayStatusChangeNotification");
        #endif
        _playStatusNotifications = byteArray[2];
        L0x04_0x01_iPodAck(iPodAck_OK,cmdID);
        break;

    case L0x04_PlayControl: //Will require a callback for interfacing externally for next/prev
        #ifdef DEBUG_MODE
        _debugSerial.println("PlayControl");
        #endif
        switch (byteArray[2])
        {
        case 0x01:
            if(_playStatus==0x01) _playStatus=0x02;
            else _playStatus = 0x01;
            //call PlayControlHandler()
            if(_playStatusHandler) {
                _playStatusHandler(byteArray[2]);
            }
            break;
        case 0x02: //Stop
            _playStatus = 0x00;
            break;
        case 0x03: //Next track
            break;
        case 0x04: //Prev track
            break;
        case 0x08: //Next track
            break;
        case 0x09: //Prev track
            break;
        case 0x0A: //Play
            _playStatus = 0x01;
            break;
        case 0x0B: //Pause
            _playStatus = 0x02;
            break;
        default:
            break;
        }
        L0x04_0x01_iPodAck(iPodAck_OK,cmdID);
        break;

    case L0x04_GetShuffle:
        #ifdef DEBUG_MODE
        _debugSerial.println("GetShuffle");
        #endif
        L0x04_0x2D_ReturnShuffle(_shuffleStatus);
        break;

    case L0x04_SetShuffle:
        #ifdef DEBUG_MODE
        _debugSerial.println("SetShuffle");
        #endif
        _shuffleStatus = byteArray[2];
        L0x04_0x01_iPodAck(iPodAck_OK,cmdID);
        break;
    
    case L0x04_GetRepeat:
        #ifdef DEBUG_MODE
        _debugSerial.println("GetRepeat");
        #endif
        L0x04_0x30_ReturnRepeat(_repeatStatus);
        break;

    case L0x04_SetRepeat:
        #ifdef DEBUG_MODE
        _debugSerial.println("SetRepeat");
        #endif
        _repeatStatus = byteArray[2];
        L0x04_0x01_iPodAck(iPodAck_OK,cmdID);
        break;

    case L0x04_GetNumPlayingTracks:
        #ifdef DEBUG_MODE
        _debugSerial.println("GetNumPlayingTracks");
        #endif
        L0x04_0x36_ReturnNumPlayingTracks(1);
        break;

    case L0x04_SetCurrentPlayingTrack:
        #ifdef DEBUG_MODE
        _debugSerial.println("SetCurrentPlayingTrack");
        #endif
        L0x04_0x01_iPodAck(iPodAck_OK,cmdID);
        break;

    default:
        break;
    }
}

void esPod::processPacket(const byte *byteArray, uint32_t len)
{
    byte rxLingoID = byteArray[0];
    const byte* subPayload = byteArray+1; //Squeeze the Lingo out
    uint32_t subPayloadLen = len-1;
    switch (rxLingoID) //0x00 is general Lingo and 0x04 is extended Lingo. Nothing else is expected from the Mini
    {
    case 0x00:
        #ifdef DEBUG_MODE
        _debugSerial.print("Lingo 0x00 ");
        #endif
        processLingo0x00(subPayload,subPayloadLen);
        break;
    
    case 0x04:
        #ifdef DEBUG_MODE
        _debugSerial.print("Lingo 0x04 ");
        #endif
        processLingo0x04(subPayload,subPayloadLen);
        break;
    
    default:
        #ifdef DEBUG_MODE
        _debugSerial.print("Unknown Lingo : ");
        _debugSerial.println(rxLingoID,HEX);
        #endif
        break;
    }
}

void esPod::refresh()
{
    //Check for a new packet and update the buffer
    while(_targetSerial.available()) {
        byte incomingByte = _targetSerial.read();

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

    //Do some setup routine for handshake when not in DEBUG
    #ifndef DEBUG_MODE
    if(!_accessoryCapabilitiesRequested) {
         L0x00_0x27_GetAccessoryInfo(0x00); //Start accessory handshake
         esPod::refresh();
    }
    #endif


}

void esPod::cyclicNotify()
{
    if((_playStatus == 0x01) && (_playStatusNotifications == 0x01) && (_extendedInterfaceModeActive)) {
        L0x04_0x27_PlayStatusNotification(0x04,60000);
    }
}
