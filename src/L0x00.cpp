#include "Arduino.h"
#include "L0x00.h"
#include "esPod.h"

/// @brief
/// @param esp Pointer to the esPod instance
/// @param byteArray
/// @param len
void L0x00::processLingo(esPod *esp, const byte *byteArray, uint32_t len)
{
    byte cmdID = byteArray[0];
    // Switch through expected commandIDs
    switch (cmdID)
    {
    case L0x00_Identify: // Deprecated command observed on Audi by @BluCobalt
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x Identify with Lingo 0x%02x", cmdID, byteArray[1]);
        // switch (byteArray[1])
        // {
        // case 0x04:
        //     esp->extendedInterfaceModeActive = true; // Pre-empt ?
        //     break;
        // default:
        //     break;
        // }
    }
    break;

    case L0x00_RequestExtendedInterfaceMode: // Mini requests extended interface mode status
    {
        ESP_LOGD(IPOD_TAG, "CMD: 0x%02x RequestExtendedInterfaceMode", cmdID);
        if (esp->extendedInterfaceModeActive)
        {
            L0x00::_0x04_ReturnExtendedInterfaceMode(esp, 0x01); // Report that extended interface mode is active
        }
        else
        {
            L0x00::_0x04_ReturnExtendedInterfaceMode(esp, 0x00); // Report that extended interface mode is not active
        }
    }
    break;

    case L0x00_EnterExtendedInterfaceMode: // Mini forces extended interface mode
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x EnterExtendedInterfaceMode", cmdID);
        esp->extendedInterfaceModeActive = true;
        L0x00::_0x02_iPodAck(esp, iPodAck_OK, cmdID);
    }
    break;

    case L0x00_ExitExtendedInterfaceMode: // Mini exits extended interface mode
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x ExitExtendedInterfaceMode", cmdID);
        if (esp->extendedInterfaceModeActive)
        {
            L0x00::_0x02_iPodAck(esp, iPodAck_OK, cmdID);
            esp->extendedInterfaceModeActive = false;
            esp->playStatusNotificationState = NOTIF_OFF;
        }
        else
        {
            L0x00::_0x02_iPodAck(esp, iPodAck_BadParam, cmdID);
        }
    }
    break;

    case L0x00_RequestiPodName: // Mini requests ipod name
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x RequestiPodName", cmdID);
        L0x00::_0x08_ReturniPodName(esp);
    }
    break;

    case L0x00_RequestiPodSoftwareVersion: // Mini requests ipod software version
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x RequestiPodSoftwareVersion", cmdID);
        L0x00::_0x0A_ReturniPodSoftwareVersion(esp);
    }
    break;

    case L0x00_RequestiPodSerialNum: // Mini requests ipod Serial Num
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x RequestiPodSerialNum", cmdID);
        L0x00::_0x0C_ReturniPodSerialNum(esp);
    }
    break;

    case L0x00_RequestiPodModelNum: // Mini requests ipod Model Num
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x RequestiPodModelNum", cmdID);
        L0x00::_0x0E_ReturniPodModelNum(esp);
    }
    break;

    case L0x00_RequestLingoProtocolVersion: // Mini requestsLingo Protocol Version
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x RequestLingoProtocolVersion for Lingo 0x%02x", cmdID, byteArray[1]);
        L0x00::_0x10_ReturnLingoProtocolVersion(esp, byteArray[1]);
    }
    break;

    case L0x00_IdentifyDeviceLingoes: // Mini identifies its lingoes, used as an ice-breaker
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x IdentifyDeviceLingoes : L 0x%02x - Opt 0x%02x - ID 0x%02x", cmdID, byteArray[1], byteArray[2], byteArray[3]);
        L0x00::_0x02_iPodAck(esp, iPodAck_OK, cmdID); // Acknowledge, start capabilities pingpong
        // A bit spam-ish ?
        L0x00::_0x27_GetAccessoryInfo(esp, 0x00); // Immediately request general capabilities
        L0x00::_0x27_GetAccessoryInfo(esp, 0x01); // Request the name
        L0x00::_0x27_GetAccessoryInfo(esp, 0x04); // Request the firmware version
        L0x00::_0x27_GetAccessoryInfo(esp, 0x05); // Request the hardware number
        L0x00::_0x27_GetAccessoryInfo(esp, 0x06); // Request the manufacturer name
        L0x00::_0x27_GetAccessoryInfo(esp, 0x07); // Request the model number
    }
    break;

    case L0x00_GetiPodOptions: // Mini requests iPod options
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x GetiPodOptions", cmdID);
        L0x00::_0x25_RetiPodOptions(esp);
    }

    case L0x00_RetAccessoryInfo: // Mini returns info after L0x00::_0x27
    {
        ESP_LOGI(IPOD_TAG, "CMD: 0x%02x RetAccessoryInfo: 0x%02x", cmdID, byteArray[1]);
        switch (byteArray[1]) // Ping-pong the next request based on the current response
        {
        case 0x00:
            ESP_LOGI(IPOD_TAG, "\tAccessory Capabilities : 0x%02x", byteArray[2]);
            break;

        case 0x01:
            ESP_LOGI(IPOD_TAG, "\tAccessory Name : %s", &byteArray[2]);
            break;

        case 0x04:
            ESP_LOGI(IPOD_TAG, "\tAccessory Firmware : %d.%d.%d", byteArray[2], byteArray[3], byteArray[4]);
            break;

        case 0x05:
            ESP_LOGI(IPOD_TAG, "\tAccessory Hardware : %d.%d.%d", byteArray[2], byteArray[3], byteArray[4]);
            break;

        case 0x06:
            ESP_LOGI(IPOD_TAG, "\tAccessory Manufacturer : %s", &byteArray[2]);
            break;

        case 0x07:
            ESP_LOGI(IPOD_TAG, "\tAccessory Model : %s", &byteArray[2]);
            break;

        default:
            L0x00::_0x02_iPodAck(esp, iPodAck_OK, cmdID);
            break;
        }
    }
    break;

    default: // In case the command is not known
    {
        ESP_LOGW(IPOD_TAG, "CMD 0x%02x not recognized.", cmdID);
        L0x00::_0x02_iPodAck(esp, iPodAck_CmdFailed, cmdID);
    }
    break;
    }
}

/// @brief Deprecated function to force the Accessory to restart Identify with L0x00_IdentifyDeviceLingoes
/// @param esp Pointer to the esPod instance
void L0x00::_0x00_RequestIdentify(esPod *esp)
{
    ESP_LOGI(IPOD_TAG, "iPod: RequestIdentify");
    const byte txPacket[] = {
        0x00,
        0x00};
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

/// @brief General response command for Lingo 0x00
/// @param esp Pointer to the esPod instance
/// @param cmdStatus Has to obey to iPodAck_xxx format as defined in L0x00.h
/// @param cmdID ID (single byte) of the Lingo 0x00 command replied to
void L0x00::_0x02_iPodAck(esPod *esp, IPOD_ACK_CODE ackCode, byte cmdID)
{
    ESP_LOGI(IPOD_TAG, "Ack 0x%02x to command 0x%02x", ackCode, cmdID);
    // Queue the packet
    const byte txPacket[] = {
        0x00,
        0x02,
        ackCode,
        cmdID};
    // Stop the timer if the same command is acknowledged before the elapsed time
    if (cmdID == esp->_pendingCmdId_0x00) // If the command ID is the same as the pending one
    {
        stopTimer(esp->_pendingTimer_0x00);                   // Stop the timer
        esp->_pendingCmdId_0x00 = 0x00;                       // Reset the pending command
        esp->_queuePacketToFront(txPacket, sizeof(txPacket)); // Jump the queue
    }
    else
        esp->_queuePacket(txPacket, sizeof(txPacket));
}

/// @brief General response command for Lingo 0x00 with numerical field (used for Ack Pending). Has to be followed up with a normal iPodAck
/// @param esp Pointer to the esPod instance
/// @param cmdStatus Unprotected, but should only be iPodAck_CmdPending
/// @param cmdID Single byte ID of the command being acknowledged with Pending
/// @param numField Pending delay in milliseconds
void L0x00::_0x02_iPodAck(esPod *esp, IPOD_ACK_CODE ackCode, byte cmdID, uint32_t numField)
{
    ESP_LOGI(IPOD_TAG, "Ack 0x%02x to command 0x%02x Numfield: %d", ackCode, cmdID, numField);
    const byte txPacket[20] = {
        0x00,
        0x02,
        ackCode,
        cmdID};
    *((uint32_t *)&txPacket[4]) = swap_endian<uint32_t>(numField);
    esp->_queuePacket(txPacket, 4 + 4);
    // Starting delayed timer for the iPodAck
    esp->_pendingCmdId_0x00 = cmdID;
    startTimer(esp->_pendingTimer_0x00, numField);
}

/// @brief Returns 0x01 if the iPod is in extendedInterfaceMode, or 0x00 if not
/// @param esp Pointer to the esPod instance
/// @param extendedModeByte Direct value of the extendedInterfaceMode boolean
void L0x00::_0x04_ReturnExtendedInterfaceMode(esPod *esp, byte extendedModeByte)
{
    ESP_LOGD(IPOD_TAG, "Extended Interface mode: 0x%02x", extendedModeByte);
    const byte txPacket[] = {
        0x00,
        0x04,
        extendedModeByte};
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Returns as a UTF8 null-ended char array, the esp->_name of the iPod (not changeable during runtime)
/// @param esp Pointer to the esPod instance
void L0x00::_0x08_ReturniPodName(esPod *esp)
{
    ESP_LOGI(IPOD_TAG, "Name: %s", esp->_name);
    byte txPacket[255] = {// Prealloc to len = FF
                          0x00,
                          0x08};
    strcpy((char *)&txPacket[2], esp->_name);
    esp->_queuePacket(txPacket, 3 + strlen(esp->_name));
}

/// @brief Returns the iPod Software Version
/// @param esp Pointer to the esPod instance
void L0x00::_0x0A_ReturniPodSoftwareVersion(esPod *esp)
{
    ESP_LOGI(IPOD_TAG, "SW version: %d.%d.%d", esp->_SWMajor, esp->_SWMinor, esp->_SWrevision);
    byte txPacket[] = {
        0x00,
        0x0A,
        (byte)esp->_SWMajor,
        (byte)esp->_SWMinor,
        (byte)esp->_SWrevision};
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Returns the iPod Serial Number (which is a string)
/// @param esp Pointer to the esPod instance
void L0x00::_0x0C_ReturniPodSerialNum(esPod *esp)
{
    ESP_LOGI(IPOD_TAG, "Serial number: %s", esp->_serialNumber);
    byte txPacket[255] = {// Prealloc to len = FF
                          0x00,
                          0x0C};
    strcpy((char *)&txPacket[2], esp->_serialNumber);
    esp->_queuePacket(txPacket, 3 + strlen(esp->_serialNumber));
}

/// @brief Returns the iPod model number PA146FD 720901, which corresponds to an iPod 5.5G classic
/// @param esp Pointer to the esPod instance
void L0x00::_0x0E_ReturniPodModelNum(esPod *esp)
{
    ESP_LOGI(IPOD_TAG, "Model number : PA146FD 720901");
    byte txPacket[] = {
        0x00,
        0x0E,
        0x00, 0x0B, 0x00, 0x05, 0x50, 0x41, 0x31, 0x34, 0x36, 0x46, 0x44, 0x00};
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Returns a preprogrammed value for the Lingo Protocol Version for 0x00, 0x03 or 0x04
/// @param esp Pointer to the esPod instance
/// @param targetLingo Target Lingo for which the Protocol Version is desired.
void L0x00::_0x10_ReturnLingoProtocolVersion(esPod *esp, byte targetLingo)
{
    byte txPacket[] = {
        0x00, 0x10,
        targetLingo,
        0x01, 0x00};
    switch (targetLingo)
    {
    case 0x00: // For General Lingo 0x00, version 1.6
        txPacket[4] = 0x06;
        break;
    case 0x03: // For Lingo 0x03, version 1.5
        txPacket[4] = 0x05;
        break;
    case 0x04: // For Lingo 0x04 (Extended Interface), version 1.12
        txPacket[4] = 0x0C;
        break;
    case 0x0A: // Lingo 0x0A, digital audio, need to return 1.0
        txPacket[4] = 0x00;
        break;
    }
    ESP_LOGI(IPOD_TAG, "Lingo 0x%02x protocol version: 1.%d", targetLingo, txPacket[4]);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Query the accessory Info (model number, manufacturer, firmware version ...) using the target Info category hex
/// @param esp Pointer to the esPod instance
/// @param desiredInfo Hex code for the type of information that is desired
void L0x00::_0x27_GetAccessoryInfo(esPod *esp, byte desiredInfo)
{
    ESP_LOGI(IPOD_TAG, "Req'd info type: 0x%02x", desiredInfo);
    byte txPacket[] = {
        0x00, 0x27,
        desiredInfo};
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Return iPod Options. In this case not much.
/// @param esp Pointer to the esPod instance
void L0x00::_0x25_RetiPodOptions(esPod *esp)
{
    ESP_LOGI(IPOD_TAG, "Returning iPod Options");
    byte txPacket[] = {
        0x00, 0x25,
        0x00};
    esp->_queuePacket(txPacket, sizeof(txPacket));
}