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
    

    //metadata variables
    uint8_t *trackTitle;
    uint8_t *artistName;
    uint8_t *albumName;
    
    //Packet-related 
    byte _rxBuf[1024];
    uint32_t _rxLen;
    uint32_t _rxCounter;

    //Device metadata
    const char* _name;
    const uint8_t _SWMajor;
    const uint8_t _SWMinor;
    const uint8_t _SWrevision;
    const char* _serialNumber;

    //Mini metadata
    bool _accessoryNameRequested = false;
    bool _accessoryCapabilitiesRequested = false;
    bool _accessoryFirmwareRequested = false;
    bool _accessoryManufRequested = false;
    bool _accessoryModelRequested = false;
    bool _accessoryHardwareRequested = false;

public:
    esPod(Stream& targetSerial);
    
    //Packet handling
    static byte checksum(const byte* byteArray, uint32_t len);
    void sendPacket(const byte* byteArray, uint32_t len);
    void L0x00_0x02_iPodAck(byte cmdStatus, byte cmdID);
    void L0x00_0x02_iPodAck_pending(uint32_t pendingDelayMS, byte cmdID);
    void L0x00_0x04_ReturnExtendedInterfaceMode(byte extendedModeByte);
    void L0x00_0x08_ReturniPodName();
    void L0x00_0x0A_ReturniPodSoftwareVersion();
    void L0x00_0x0C_ReturniPodSerialNum();
    void L0x00_0x0E_ReturniPodModelNum();
    void L0x00_0x10_ReturnLingoProtocolVersion(byte targetLingo);
    void L0x00_0x27_GetAccessoryInfo(byte desiredInfo);

    void processLingo0x00(const byte *byteArray, uint32_t len);
    void processLingo0x04(const byte* byteArray, uint32_t len);
    void processPacket(const byte* byteArray,uint32_t len);

    void refresh();
};


