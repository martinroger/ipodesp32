#pragma once
#include "Arduino.h"
#include "L0x00.h"
#include "L0x04.h"

class esPod
{
private:
    //Serial to the listening device
    Stream& _targetSerial;
    #ifdef DEBUG_MODE
    HardwareSerial& _debugSerial;
    #endif
    byte _prevRxByte;
    
    //State variables
    bool _extendedInterfaceModeActive;
    
    //Packet-related 
    byte _rxBuf[1024];
    uint32_t _rxLen;
    uint32_t _rxCounter;
public:
    esPod(Stream& targetSerial);
    
    //Packet handling
    static byte checksum(const byte* byteArray, uint32_t len);
    void sendPacket(const byte* byteArray, uint32_t len);
    void processLingo0x00(const byte* byteArray, uint32_t len);
    void processLingo0x04(const byte* byteArray, uint32_t len);
    void processPacket(const byte* byteArray,uint32_t len);

    void refresh();
};

