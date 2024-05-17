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
        _SWrevision(0x00)
{
    //Setup the metadata
    //_SWMajor = 0x01;
    //_SWMinor = 0x03;
    //_SWrevision = 0x00;
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
        // (byte)((pendingDelayMS >>24)&0xFF),
        // (byte)((pendingDelayMS >>16)&0xFF),
        // (byte)((pendingDelayMS >>8)&0xFF),
        // (byte)((pendingDelayMS)&0xFF)
    };

    //_debugSerial.println(swap_endian<uint32_t>(pendingDelayMS));
    //*(txPacket+3) = swap_endian<uint32_t>(pendingDelayMS); //Point to the value contained 3 address intervals down from txPacket
    *((uint32_t*)&txPacket[4]) = swap_endian<uint32_t>(pendingDelayMS);
    //txPacket[4] = swap_endian<uint32_t>(pendingDelayMS); //Trying things
    // for (int i = 0; i < 20; i++)
    // {
    //     _debugSerial.println(txPacket[i],HEX);
    // }
    _debugSerial.println();
    
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
        break;
    
    case L0x00_RequestiPodModelNum: //Mini requests ipod Model Num
        #ifdef DEBUG_MODE
        _debugSerial.println("RequestiPodModelNum");
        #endif
        break;
    
    case L0x00_RequestLingoProtocolVersion: //Mini Lingo Protocol Version
        #ifdef DEBUG_MODE
        _debugSerial.println("RequestLingoProtocolVersion");
        #endif
        break;
    
    case L0x00_IdentifyDeviceLingoes: //Mini identifies its lingoes
        #ifdef DEBUG_MODE
        _debugSerial.println("IdentifyDeviceLingoes");
        #endif
        break;
    
    case L0x00_RetAccessoryInfo: //Mini returns meta info
        #ifdef DEBUG_MODE
        _debugSerial.println("RetAccessoryInfo");
        #endif
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
    switch (cmdID)
    {
    case L0x04_GetIndexedPlayingTrackInfo:
        #ifdef DEBUG_MODE
        _debugSerial.println("GetIndexedPlayingTrackInfo");
        #endif
        break;
    
    case L0x04_RequestProtocolVersion:
        #ifdef DEBUG_MODE
        _debugSerial.println("RequestProtocolVersion");
        #endif
        break;

    case L0x04_ResetDBSelection:
        #ifdef DEBUG_MODE
        _debugSerial.println("ResetDBSelection");
        #endif
        break;

    case L0x04_SelectDBRecord:
        #ifdef DEBUG_MODE
        _debugSerial.println("SelectDBRecord");
        #endif
        break;
    
    case L0x04_GetNumberCategorizedDBRecords:
        #ifdef DEBUG_MODE
        _debugSerial.println("GetNumberCategorizedDBRecords");
        #endif
        break;
    
    case L0x04_RetrieveCategorizedDatabaseRecords:
        #ifdef DEBUG_MODE
        _debugSerial.println("RetrieveCategorizedDatabaseRecords");
        #endif
        break;

    case L0x04_GetPlayStatus:
        #ifdef DEBUG_MODE
        _debugSerial.println("GetPlayStatus");
        #endif
        break;

    case L0x04_GetCurrentPlayingTrackIndex:
        #ifdef DEBUG_MODE
        _debugSerial.println("GetCurrentPlayingTrackIndex");
        #endif
        break;

    case L0x04_GetIndexedPlayingTrackTitle:
        #ifdef DEBUG_MODE
        _debugSerial.println("GetIndexedPlayingTrackTitle");
        #endif
        break;

    case L0x04_GetIndexedPlayingTrackArtistName:
        #ifdef DEBUG_MODE
        _debugSerial.println("GetIndexedPlayingTrackArtistName");
        #endif
        break;

    case L0x04_GetIndexedPlayingTrackAlbumName:
        #ifdef DEBUG_MODE
        _debugSerial.println("GetIndexedPlayingTrackAlbumName");
        #endif
        break;

    case L0x04_SetPlayStatusChangeNotification:
        #ifdef DEBUG_MODE
        _debugSerial.println("SetPlayStatusChangeNotification");
        #endif
        break;

    case L0x04_PlayControl:
        #ifdef DEBUG_MODE
        _debugSerial.println("PlayControl");
        #endif
        break;

    case L0x04_GetShuffle:
        #ifdef DEBUG_MODE
        _debugSerial.println("GetShuffle");
        #endif
        break;

    case L0x04_SetShuffle:
        #ifdef DEBUG_MODE
        _debugSerial.println("SetShuffle");
        #endif
        break;
    
    case L0x04_GetRepeat:
        #ifdef DEBUG_MODE
        _debugSerial.println("GetRepeat");
        #endif
        break;

    case L0x04_SetRepeat:
        #ifdef DEBUG_MODE
        _debugSerial.println("SetRepeat");
        #endif
        break;

    case L0x04_GetNumPlayingTracks:
        #ifdef DEBUG_MODE
        _debugSerial.println("GetNumPlayingTracks");
        #endif
        break;

    case L0x04_SetCurrentPlayingTrack:
        #ifdef DEBUG_MODE
        _debugSerial.println("SetCurrentPlayingTrack");
        #endif
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
}
