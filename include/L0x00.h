#pragma once
#include "Arduino.h"
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
    
};
