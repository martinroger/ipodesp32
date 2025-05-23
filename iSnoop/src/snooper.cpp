#include "snooper.h"

//-----------------------------------------------------------------------
//|                           Local utilities                           |
//-----------------------------------------------------------------------
#pragma region Local utilities
// ESP32 is Little-Endian, iPod is Big-Endian
template <typename T>
T swap_endian(T u)
{
    static_assert(CHAR_BIT == 8, "CHAR_BIT != 8");

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

#pragma endregion

//-----------------------------------------------------------------------
//|                      Cardinal tasks and Timers                      |
//-----------------------------------------------------------------------
#pragma region Tasks and Timers
/// @brief RX Task, sifts through the incoming serial data and compiles packets that pass the checksum and passes them to the processing Queue _cmdQueue. Also handles timeouts and can trigger state resets.
/// @param pvParameters Unused
void snooper::_rxTask(void *pvParameters)
{
    snooper *snooperInstance = static_cast<snooper *>(pvParameters);

    byte prevByte = 0x00;
    byte incByte = 0x00;
    byte buf[MAX_PACKET_SIZE] = {0x00};
    uint32_t expLength = 0;
    uint32_t cursor = 0;

    unsigned long lastByteRX = millis();   // Last time a byte was RXed in a packet
    unsigned long lastActivity = millis(); // Last time any RX activity was detected

    aapCommand cmd;

    while (true)
    {
        {
            // Use of while instead of if()
            while (snooperInstance->_targetSerial.available())
            {
                // Timestamping the last activity on RX
                lastActivity = millis();
                incByte = snooperInstance->_targetSerial.read();
                // If we are not in the middle of a RX, and we receive a 0xFF 0x55, start sequence, reset expected length and position cursor
                if (prevByte == 0xFF && incByte == 0x55 && !snooperInstance->_rxIncomplete)
                {
                    lastByteRX = millis();
                    snooperInstance->_rxIncomplete = true;
                    expLength = 0;
                    cursor = 0;
                }
                else if (snooperInstance->_rxIncomplete)
                {
                    // Timestamping the last byte received
                    lastByteRX = millis();
                    // Expected length has not been received yet
                    if (expLength == 0 && cursor == 0)
                    {
                        expLength = incByte; // First byte after 0xFF 0x55
                        if (expLength > MAX_PACKET_SIZE)
                        {
                            ESP_LOGW(__func__, "Expected length is too long, discarding packet");
                            snooperInstance->_rxIncomplete = false;
                            // TODO: Send a NACK to the Accessory
                        }
                        else if (expLength == 0)
                        {
                            ESP_LOGW(__func__, "Expected length is 0, discarding packet");
                            snooperInstance->_rxIncomplete = false;
                            // TODO: Send a NACK to the Accessory
                        }
                    }
                    else // Length is already received
                    {
                        buf[cursor++] = incByte;
                        if (cursor == expLength + 1)
                        {
                            // We have received the expected length + checksum
                            snooperInstance->_rxIncomplete = false;
                            // Check the checksum
                            byte calcChecksum = snooper::_checksum(buf, expLength);
                            if (calcChecksum == incByte)
                            {
                                // Checksum is correct, send the packet to the processing queue
                                // Allocate memory for the payload so it doesn't become out of scope
                                cmd.payload = new byte[expLength];
                                cmd.length = expLength;
                                memcpy(cmd.payload, buf, expLength);
                                if (xQueueSend(snooperInstance->_cmdQueue, &cmd, pdMS_TO_TICKS(5)) == pdTRUE)
                                {
                                    ESP_LOGD(__func__, "Packet received and sent to processing queue");
                                }
                                else
                                {
                                    ESP_LOGW(__func__, "Packet received but could not be sent to processing queue. Discarding");
                                    delete[] cmd.payload;
                                    cmd.payload = nullptr;
                                    cmd.length = 0;
                                }
                            }
                            else // Checksum mismatch
                            {
                                ESP_LOGW(__func__, "Checksum mismatch, discarding packet");
                                // TODO: Send a NACK to the Accessory
                            }
                        }
                    }
                }
                // Always update the previous byte
                prevByte = incByte;
            }
            if (snooperInstance->_rxIncomplete && millis() - lastByteRX > INTERBYTE_TIMEOUT) // If we are in the middle of a packet and we haven't received a byte in 1s, discard the packet
            {
                ESP_LOGW(__func__, "Packet incomplete, discarding");
                snooperInstance->_rxIncomplete = false;
                // cmd.payload = nullptr;
                // cmd.length = 0;
                // TODO: Send a NACK to the Accessory
            }
            if (millis() - lastActivity > SERIAL_TIMEOUT) // If we haven't received any byte in 30s, reset the RX state
            {
                ESP_LOGW(__func__, "No activity in %lu ms, resetting RX state", SERIAL_TIMEOUT);
                // Reset the timestamp for next Serial timeout
                lastActivity = millis();
                snooperInstance->resetState();
            }
            vTaskDelay(pdMS_TO_TICKS(RX_TASK_INTERVAL_MS));
        }
    }
}

/// @brief Processor task retrieving from the cmdQueue and processing the commands
/// @param pvParameters
void snooper::_processTask(void *pvParameters)
{
    snooper *snooperInstance = static_cast<snooper *>(pvParameters);
    aapCommand incCmd;


    while (true)
    {


        if (xQueueReceive(snooperInstance->_cmdQueue, &incCmd, 0) == pdTRUE) // Non blocking receive
        {
            // Process the command
            snooperInstance->_processPacket(incCmd.payload, incCmd.length);
            // Free the memory allocated for the payload
            delete[] incCmd.payload;
            incCmd.payload = nullptr;
            incCmd.length = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(PROCESS_INTERVAL_MS));
    }
}

/// @brief Transmit task, retrieves from the txQueue and sends the packets over Serial at high priority but wider timing
/// @param pvParameters
void snooper::_txTask(void *pvParameters)
{
    snooper *snooperInstance = static_cast<snooper *>(pvParameters);
    aapCommand txCmd;


    while (true)
    {

        if (!snooperInstance->_rxIncomplete) //_rxTask is not in the middle of a packet
        {
            // Retrieve from the queue and send the packet
            if (xQueueReceive(snooperInstance->_txQueue, &txCmd, 0) == pdTRUE)
            {
                // vTaskDelay(pdMS_TO_TICKS(TX_INTERVAL_MS));
                // Send the packet
                snooperInstance->_sendPacket(txCmd.payload, txCmd.length);
                // Free the memory allocated for the payload
                delete[] txCmd.payload;
                txCmd.payload = nullptr;
                txCmd.length = 0;
            }
            vTaskDelay(pdMS_TO_TICKS(TX_INTERVAL_MS));
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(RX_TASK_INTERVAL_MS));
        }
    }
}

#pragma endregion

//-----------------------------------------------------------------------
//|                          Packet management                          |
//-----------------------------------------------------------------------
#pragma region Packet management
/// @brief //Calculates the checksum of a packet that starts from i=0 ->Lingo to i=len -> Checksum
/// @param byteArray Array from Lingo byte to Checksum byte
/// @param len Length of array (Lingo byte to Checksum byte)
/// @return Calculated checksum for comparison
byte snooper::_checksum(const byte *byteArray, uint32_t len)
{
    uint32_t tempChecksum = len;
    for (int i = 0; i < len; i++)
    {
        tempChecksum += byteArray[i];
    }
    tempChecksum = 0x100 - (tempChecksum & 0xFF);
    return (byte)tempChecksum;
}

/// @brief Composes and sends a packet over to the console
/// @param byteArray Byte array to send to the console
/// @param len Length of the array to send
void snooper::_sendPacket(const byte *byteArray, uint32_t len)
{
    Serial.printf("%s: ", snooperName);
    Serial.write(byteArray, len);
    Serial.println();
}

/// @brief Adds a packet to the transmit queue
/// @param byteArray Array of bytes to add to the queue
/// @param len Length of data in the array
void snooper::_queuePacket(const byte *byteArray, uint32_t len)
{
    aapCommand cmdToQueue;
    cmdToQueue.payload = new byte[len];
    cmdToQueue.length = len;
    memcpy(cmdToQueue.payload, byteArray, len);
    if (xQueueSend(_txQueue, &cmdToQueue, pdMS_TO_TICKS(5)) != pdTRUE)
    {
        ESP_LOGW(__func__, "Could not queue packet");
        delete[] cmdToQueue.payload;
        cmdToQueue.payload = nullptr;
        cmdToQueue.length = 0;
    }
}



/// @brief Processes a valid packet and calls the relevant Lingo processor
/// @param byteArray Checksum-validated packet starting at LingoID
/// @param len Length of valid data in the packet
void snooper::_processPacket(const byte *byteArray, uint32_t len)
{
    byte rxLingoID = byteArray[0];
    const byte *subPayload = byteArray + 1; // Squeeze the Lingo out
    uint32_t subPayloadLen = len - 1;
    switch (rxLingoID) // 0x00 is general Lingo and 0x04 is extended Lingo. Nothing else is expected from the Mini
    {
    case 0x00: // General Lingo
        ESP_LOGD(snooperName,"Lingo 0x00 Packet in processor,payload length: %d", subPayloadLen);
        processLingo0x00(subPayload, subPayloadLen);
        break;

    case 0x04: // Extended Interface Lingo
        ESP_LOGD(snooperName,"Lingo 0x04 Packet in processor,payload length: %d", subPayloadLen);
        processLingo0x04(subPayload, subPayloadLen);
        break;

    default:
        ESP_LOGW(snooperName,"Unknown Lingo packet : L0x%x", rxLingoID);
        break;
    }
}
#pragma endregion

//-----------------------------------------------------------------------
//|         Constructor, reset, attachCallback for PB control           |
//-----------------------------------------------------------------------
#pragma region Constructor, destructor, reset and external PB Contoller attach
/// @brief Constructor for the snooper class
/// @param targetSerial (Serial) stream on which the snooper will be communicating
snooper::snooper(Stream &targetSerial, const char *name)
    : _targetSerial(targetSerial), snooperName(name)
{
    // Create queues with pointer structures to byte arrays
    _cmdQueue = xQueueCreate(CMD_QUEUE_SIZE, sizeof(aapCommand));
    _txQueue = xQueueCreate(TX_QUEUE_SIZE, sizeof(aapCommand));


    if (_cmdQueue == NULL || _txQueue == NULL ) // Add _timerQueue check
    {
        ESP_LOGE(snooperName,"Could not create queues");
    }

    // Create FreeRTOS tasks for compiling incoming commands, processing commands and transmitting commands
    if (_cmdQueue != NULL && _txQueue != NULL ) // Add _timerQueue check
    {
        xTaskCreatePinnedToCore(_rxTask, "RX Task", RX_TASK_STACK_SIZE, this, RX_TASK_PRIORITY, &_rxTaskHandle, 1);
        xTaskCreatePinnedToCore(_processTask, "Processor Task", PROCESS_TASK_STACK_SIZE, this, PROCESS_TASK_PRIORITY, &_processTaskHandle, 1);
        xTaskCreatePinnedToCore(_txTask, "Transmit Task", TX_TASK_STACK_SIZE, this, TX_TASK_PRIORITY, &_txTaskHandle, 1);

        if (_rxTaskHandle == NULL || _processTaskHandle == NULL || _txTaskHandle == NULL)
        {
            ESP_LOGE(snooperName,"Could not create tasks");
        }
    }
    else
    {
        ESP_LOGE(snooperName,"Could not create tasks, queues not created");
    }
}

/// @brief Destructor for the snooper class. Normally not used.
snooper::~snooper()
{
    aapCommand tempCmd;
    vTaskDelete(_rxTaskHandle);
    vTaskDelete(_processTaskHandle);
    vTaskDelete(_txTaskHandle);
    // Remember to deallocate memory
    while (xQueueReceive(_cmdQueue, &tempCmd, 0) == pdTRUE)
    {
        delete[] tempCmd.payload;
        tempCmd.payload = nullptr;
        tempCmd.length = 0;
    }
    while (xQueueReceive(_txQueue, &tempCmd, 0) == pdTRUE)
    {
        delete[] tempCmd.payload;
        tempCmd.payload = nullptr;
        tempCmd.length = 0;
    }
    vQueueDelete(_cmdQueue);
    vQueueDelete(_txQueue);
}

void snooper::resetState()
{

    ESP_LOGW(snooperName,"snooper resetState called");

    // Reset the queues
    aapCommand tempCmd;

    // Remember to deallocate memory
    while (xQueueReceive(_cmdQueue, &tempCmd, 0) == pdTRUE)
    {
        delete[] tempCmd.payload;
        tempCmd.payload = nullptr;
        tempCmd.length = 0;
    }
    while (xQueueReceive(_txQueue, &tempCmd, 0) == pdTRUE)
    {
        delete[] tempCmd.payload;
        tempCmd.payload = nullptr;
        tempCmd.length = 0;
    }
    xQueueReset(_cmdQueue);
    xQueueReset(_txQueue);
}

#pragma endregion

//-----------------------------------------------------------------------
//|                     Process Lingo 0x00 Requests                     |
//-----------------------------------------------------------------------
#pragma region 0x00 Processor

/// @brief This function processes a shortened byteArray packet identified as a valid Lingo 0x00 request
/// @param byteArray Shortened packet, with byteArray[0] being the Lingo 0x00 command ID byte
/// @param len Length of valid data in the byteArray
void snooper::processLingo0x00(const byte *byteArray, uint32_t len)
{
    byte cmdID = byteArray[0];
    // Switch through expected commandIDs
    switch (cmdID)
    {
    case L0x00_Identify: // Deprecated command observed on Audi by @BluCobalt
    {
        ESP_LOGI(snooperName,"CMD: 0x%02x Identify with Lingo 0x%02x", cmdID, byteArray[1]);
    }
    break;

    case L0x00_RequestExtendedInterfaceMode: // Mini requests extended interface mode status
    {
        ESP_LOGI(snooperName,"CMD: 0x%02x RequestExtendedInterfaceMode", cmdID);
    }
    break;

    case L0x00_EnterExtendedInterfaceMode: // Mini forces extended interface mode
    {
        ESP_LOGI(snooperName,"CMD: 0x%02x EnterExtendedInterfaceMode", cmdID);
    }
    break;

    case L0x00_ExitExtendedInterfaceMode: // Mini exits extended interface mode
    {
        ESP_LOGI(snooperName,"CMD: 0x%02x ExitExtendedInterfaceMode", cmdID);
    }
    break;

    case L0x00_RequestiPodName: // Mini requests ipod name
    {
        ESP_LOGI(snooperName,"CMD: 0x%02x RequestiPodName", cmdID);
    }
    break;

    case L0x00_RequestiPodSoftwareVersion: // Mini requests ipod software version
    {
        ESP_LOGI(snooperName,"CMD: 0x%02x RequestiPodSoftwareVersion", cmdID);
    }
    break;

    case L0x00_RequestiPodSerialNum: // Mini requests ipod Serial Num
    {
        ESP_LOGI(snooperName,"CMD: 0x%02x RequestiPodSerialNum", cmdID);
    }
    break;

    case L0x00_RequestiPodModelNum: // Mini requests ipod Model Num
    {
        ESP_LOGI(snooperName,"CMD: 0x%02x RequestiPodModelNum", cmdID);
    }
    break;

    case L0x00_RequestLingoProtocolVersion: // Mini requestsLingo Protocol Version
    {
        ESP_LOGI(snooperName,"CMD: 0x%02x RequestLingoProtocolVersion for Lingo 0x%02x", cmdID, byteArray[1]);
    }
    break;

    case L0x00_IdentifyDeviceLingoes: // Mini identifies its lingoes, used as an ice-breaker
    {
        ESP_LOGI(snooperName,"CMD: 0x%02x IdentifyDeviceLingoes", cmdID);
    }
    break;

    case L0x00_RetAccessoryInfo: // Mini returns info after L0x00_0x27
    {
        ESP_LOGI(snooperName,"CMD: 0x%02x RetAccessoryInfo: 0x%02x", cmdID, byteArray[1]);
    }
    break;

    default: // In case the command is not known
    {
        ESP_LOGW(snooperName,"CMD 0x%02x not recognized.", cmdID);
    }
    break;
    }

    //Queue the packet anyways
    _queuePacket(byteArray, len);
}
#pragma endregion

//-----------------------------------------------------------------------
//|                     Process Lingo 0x04 Requests                     |
//-----------------------------------------------------------------------
#pragma region 0x04 Processor

/// @brief This function processes a shortened byteArray packet identified as a valid Lingo 0x04 request
/// @param byteArray Shortened packet, with byteArray[1] being the last byte of the Lingo 0x04 command
/// @param len Length of valid data in the byteArray
void snooper::processLingo0x04(const byte *byteArray, uint32_t len)
{
    byte cmdID = byteArray[1];
    // Initialising handlers to understand what is happening in some parts of the switch. They cannot be initialised in the switch-case scope
    byte category;
    uint32_t startIndex, counts, tempTrackIndex;
    {
        switch (cmdID) // Reminder : we are technically switching on byteArray[1] now
        {
        case L0x04_GetIndexedPlayingTrackInfo:
        {
            tempTrackIndex = swap_endian<uint32_t>(*((uint32_t *)&byteArray[3]));
            switch (byteArray[2]) // Switch on the type of track info requested (careful with overloads)
            {
            case 0x00: // General track Capabilities and Information
                ESP_LOGI(snooperName,"CMD 0x%04x GetIndexedPlayingTrackInfo 0x%02x for index %d : Duration", cmdID, byteArray[2], tempTrackIndex);
                break;
            case 0x02: // Track Release Date (fictional)
                ESP_LOGI(snooperName,"CMD 0x%04x GetIndexedPlayingTrackInfo 0x%02x for index %d : Release date", cmdID, byteArray[2], tempTrackIndex);
                break;
            case 0x01: // Track Title
                ESP_LOGI(snooperName,"CMD 0x%04x GetIndexedPlayingTrackInfo 0x%02x for index %d : Title", cmdID, byteArray[2], tempTrackIndex);
                break;
            case 0x05: // Track Genre
                ESP_LOGI(snooperName,"CMD 0x%04x GetIndexedPlayingTrackInfo 0x%02x for index %d : Genre", cmdID, byteArray[2], tempTrackIndex);
                break;
            case 0x06: // Track Composer
                ESP_LOGI(snooperName,"CMD 0x%04x GetIndexedPlayingTrackInfo 0x%02x for index %d : Composer", cmdID, byteArray[2], tempTrackIndex);
                break;
            default: // In case the request is beyond the track capabilities
                ESP_LOGW(snooperName,"CMD 0x%04x GetIndexedPlayingTrackInfo 0x%02x for index %d : Type not recognised!", cmdID, byteArray[2], tempTrackIndex);
                break;
            }
        }
        break;

        case L0x04_RequestProtocolVersion: // Hardcoded return for L0x04
        {
            ESP_LOGI(snooperName,"CMD 0x%04x RequestProtocolVersion", cmdID);
        }
        break;

        case L0x04_ResetDBSelection: // Not sure what to do here. Reset Current Track Index ?
        {
            ESP_LOGI(snooperName,"CMD 0x%04x ResetDBSelection", cmdID);
        }
        break;

        case L0x04_SelectDBRecord: // Used for browsing ?
        {
            ESP_LOGI(snooperName,"CMD 0x%04x SelectDBRecord", cmdID);
        }
        break;

        case L0x04_GetNumberCategorizedDBRecords: // Mini requests the number of records for a specific DB_CAT
        {
            category = byteArray[2];
            ESP_LOGI(snooperName,"CMD 0x%04x GetNumberCategorizedDBRecords category: 0x%02x", cmdID, category);
        }
        break;

        case L0x04_RetrieveCategorizedDatabaseRecords: // Loops through the desired records for a given DB_CAT
        {
            category = byteArray[2]; // DBCat
            startIndex = swap_endian<uint32_t>(*(uint32_t *)&byteArray[3]);
            counts = swap_endian<uint32_t>(*(uint32_t *)&byteArray[7]);

            ESP_LOGI(snooperName,"CMD 0x%04x RetrieveCategorizedDatabaseRecords category: 0x%02x from %d for %d counts", cmdID, category, startIndex, counts);
        }
        break;

        case L0x04_GetPlayStatus: // Returns the current playStatus and the position/duration of the current track
        {
            ESP_LOGI(snooperName,"CMD 0x%04x GetPlayStatus", cmdID);
        }
        break;

        case L0x04_GetCurrentPlayingTrackIndex: // Get the uint32 index of the currently playing song
        {
            ESP_LOGI(snooperName,"CMD 0x%04x GetCurrentPlayingTrackIndex", cmdID);
        }
        break;

        case L0x04_GetIndexedPlayingTrackTitle:
        {
            tempTrackIndex = swap_endian<uint32_t>(*((uint32_t *)&byteArray[2]));
            ESP_LOGI(snooperName,"CMD 0x%04x GetIndexedPlayingTrackTitle for index %d", cmdID, tempTrackIndex);

        }
        break;

        case L0x04_GetIndexedPlayingTrackArtistName:
        {
            tempTrackIndex = swap_endian<uint32_t>(*((uint32_t *)&byteArray[2]));

            ESP_LOGI(snooperName,"CMD 0x%04x GetIndexedPlayingTrackArtistName for index %d", cmdID, tempTrackIndex);
        }
        break;

        case L0x04_GetIndexedPlayingTrackAlbumName:
        {
            tempTrackIndex = swap_endian<uint32_t>(*((uint32_t *)&byteArray[2]));
            ESP_LOGI(snooperName,"CMD 0x%04x GetIndexedPlayingTrackAlbumName for index %d", cmdID, tempTrackIndex);
        }
        break;

        case L0x04_SetPlayStatusChangeNotification: // Turns on basic notifications
        {
            ESP_LOGI(snooperName,"CMD 0x%04x SetPlayStatusChangeNotification 0x%02x", cmdID, byteArray[2]);
        }
        break;

        case L0x04_PlayCurrentSelection: // Used to play a specific index, usually for "next" commands, but may be used to actually jump anywhere
        {
            tempTrackIndex = swap_endian<uint32_t>(*((uint32_t *)&byteArray[2]));
            ESP_LOGI(snooperName,"CMD 0x%04x PlayCurrentSelection index %d", cmdID, tempTrackIndex);
        }
        break;

        case L0x04_PlayControl: // Basic play control. Used for Prev, pause and play
        {
            ESP_LOGI(snooperName,"CMD 0x%04x PlayControl req: 0x%02x", cmdID, byteArray[2]);
        }
        break;

        case L0x04_GetShuffle: // Get Shuffle state from the PB Engine
        {
            ESP_LOGI(snooperName,"CMD 0x%04x GetShuffle", cmdID);
        }
        break;

        case L0x04_SetShuffle: // Set Shuffle state
        {
            ESP_LOGI(snooperName,"CMD 0x%04x SetShuffle req: 0x%02x", cmdID, byteArray[2]);
        }
        break;

        case L0x04_GetRepeat: // Get Repeat state
        {
            ESP_LOGI(snooperName,"CMD 0x%04x GetRepeat", cmdID);
        }
        break;

        case L0x04_SetRepeat: // Set Repeat state
        {
            ESP_LOGI(snooperName,"CMD 0x%04x SetRepeat req: 0x%02x", cmdID, byteArray[2]);
        }
        break;

        case L0x04_GetNumPlayingTracks: // Systematically return TOTAL_NUM_TRACKS
        {
            ESP_LOGI(snooperName,"CMD 0x%04x GetNumPlayingTracks", cmdID);
        }
        break;

        case L0x04_SetCurrentPlayingTrack: // Basically identical to PlayCurrentSelection
        {
            tempTrackIndex = swap_endian<uint32_t>(*((uint32_t *)&byteArray[2]));
            ESP_LOGI(snooperName,"CMD 0x%04x SetCurrentPlayingTrack index %d", cmdID, tempTrackIndex);
        }
        break;
        }
    }
}
#pragma endregion
/*
//-----------------------------------------------------------------------
//|                     Lingo 0x00 subfunctions                         |
//-----------------------------------------------------------------------
#pragma region LINGO 0x00

/// @brief Deprecated function to force the Accessory to restart Identify with L0x00_IdentifyDeviceLingoes
void snooper::L0x00_0x00_RequestIdentify()
{
    ESP_LOGI(snooperName,"iPod: RequestIdentify");
    const byte txPacket[] = {
        0x00,
        0x00};
    _queuePacket(txPacket, sizeof(txPacket));
}

/// @brief General response command for Lingo 0x00
/// @param cmdStatus Has to obey to iPodAck_xxx format as defined in L0x00.h
/// @param cmdID ID (single byte) of the Lingo 0x00 command replied to
void snooper::L0x00_0x02_iPodAck(byte cmdStatus, byte cmdID)
{
    ESP_LOGI(snooperName,"Ack 0x%02x to command 0x%02x", cmdStatus, cmdID);
    // Queue the packet
    const byte txPacket[] = {
        0x00,
        0x02,
        cmdStatus,
        cmdID};
    // Stop the timer if the same command is acknowledged before the elapsed time
    if (cmdID == _pendingCmdId_0x00) // If the command ID is the same as the pending one
    {
        stopTimer(_pendingTimer_0x00);                   // Stop the timer
        _pendingCmdId_0x00 = 0x00;                       // Reset the pending command
        _queuePacketToFront(txPacket, sizeof(txPacket)); // Jump the queue
    }
    else
        _queuePacket(txPacket, sizeof(txPacket));
}

/// @brief General response command for Lingo 0x00 with numerical field (used for Ack Pending). Has to be followed up with a normal iPodAck
/// @param cmdStatus Unprotected, but should only be iPodAck_CmdPending
/// @param cmdID Single byte ID of the command being acknowledged with Pending
/// @param numField Pending delay in milliseconds
void snooper::L0x00_0x02_iPodAck(byte cmdStatus, byte cmdID, uint32_t numField)
{
    ESP_LOGI(snooperName,"Ack 0x%02x to command 0x%02x Numfield: %d", cmdStatus, cmdID, numField);
    const byte txPacket[20] = {
        0x00,
        0x02,
        cmdStatus,
        cmdID};
    *((uint32_t *)&txPacket[4]) = swap_endian<uint32_t>(numField);
    _queuePacket(txPacket, 4 + 4);
    // Starting delayed timer for the iPodAck
    _pendingCmdId_0x00 = cmdID;
    startTimer(_pendingTimer_0x00, numField);
}

/// @brief Returns 0x01 if the iPod is in extendedInterfaceMode, or 0x00 if not
/// @param extendedModeByte Direct value of the extendedInterfaceMode boolean
void snooper::L0x00_0x04_ReturnExtendedInterfaceMode(byte extendedModeByte)
{
    ESP_LOGI(snooperName,"Extended Interface mode: 0x%02x", extendedModeByte);
    const byte txPacket[] = {
        0x00,
        0x04,
        extendedModeByte};
    _queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Returns as a UTF8 null-ended char array, the _name of the iPod (not changeable during runtime)
void snooper::L0x00_0x08_ReturniPodName()
{
    ESP_LOGI(snooperName,"Name: %s", _name);
    byte txPacket[255] = {// Prealloc to len = FF
                          0x00,
                          0x08};
    strcpy((char *)&txPacket[2], _name);
    _queuePacket(txPacket, 3 + strlen(_name));
}

/// @brief Returns the iPod Software Version
void snooper::L0x00_0x0A_ReturniPodSoftwareVersion()
{
    ESP_LOGI(snooperName,"SW version: %d.%d.%d", _SWMajor, _SWMinor, _SWrevision);
    byte txPacket[] = {
        0x00,
        0x0A,
        (byte)_SWMajor,
        (byte)_SWMinor,
        (byte)_SWrevision};
    _queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Returns the iPod Serial Number (which is a string)
void snooper::L0x00_0x0C_ReturniPodSerialNum()
{
    ESP_LOGI(snooperName,"Serial number: %s", _serialNumber);
    byte txPacket[255] = {// Prealloc to len = FF
                          0x00,
                          0x0C};
    strcpy((char *)&txPacket[2], _serialNumber);
    _queuePacket(txPacket, 3 + strlen(_serialNumber));
}

/// @brief Returns the iPod model number PA146FD 720901, which corresponds to an iPod 5.5G classic
void snooper::L0x00_0x0E_ReturniPodModelNum()
{
    ESP_LOGI(snooperName,"Model number : PA146FD 720901");
    byte txPacket[] = {
        0x00,
        0x0E,
        0x00, 0x0B, 0x00, 0x05, 0x50, 0x41, 0x31, 0x34, 0x36, 0x46, 0x44, 0x00};
    _queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Returns a preprogrammed value for the Lingo Protocol Version for 0x00, 0x03 or 0x04
/// @param targetLingo Target Lingo for which the Protocol Version is desired.
void snooper::L0x00_0x10_ReturnLingoProtocolVersion(byte targetLingo)
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
    }
    ESP_LOGI(snooperName,"Lingo 0x%02x protocol version: 1.%d", targetLingo, txPacket[4]);
    _queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Query the accessory Info (model number, manufacturer, firmware version ...) using the target Info category hex
/// @param desiredInfo Hex code for the type of information that is desired
void snooper::L0x00_0x27_GetAccessoryInfo(byte desiredInfo)
{
    ESP_LOGI(snooperName,"Req'd info type: 0x%02x", desiredInfo);
    byte txPacket[] = {
        0x00, 0x27,
        desiredInfo};
    _queuePacket(txPacket, sizeof(txPacket));
}

#pragma endregion

//-----------------------------------------------------------------------
//|                     Lingo 0x04 subfunctions                         |
//-----------------------------------------------------------------------
#pragma region LINGO 0x04

/// @brief General response command for Lingo 0x04
/// @param cmdStatus Has to obey to iPodAck_xxx format as defined in L0x00.h
/// @param cmdID last two ID bytes of the Lingo 0x04 command replied to
void snooper::L0x04_0x01_iPodAck(byte cmdStatus, byte cmdID)
{
    ESP_LOGI(snooperName,"Ack 0x%02x to command 0x%04x", cmdStatus, cmdID);
    // Queue the ack packet
    const byte txPacket[] = {
        0x04,
        0x00, 0x01,
        cmdStatus,
        0x00, cmdID};
    // Stop the timer if the same command is acknowledged before the elapsed time
    if (cmdID == _pendingCmdId_0x04) // If the pending command is the one being acknowledged
    {
        stopTimer(_pendingTimer_0x04);
        _pendingCmdId_0x04 = 0x00;
        _queuePacketToFront(txPacket, sizeof(txPacket)); // Jump the queue
    }
    else
        _queuePacket(txPacket, sizeof(txPacket)); // Queue at the back
}

/// @brief General response command for Lingo 0x04 with numerical field (used for Ack Pending). Has to be followed up with a normal iPodAck
/// @param cmdStatus Unprotected, but should only be iPodAck_CmdPending
/// @param cmdID Single end-byte ID of the command being acknowledged with Pending
/// @param numField Pending delay in milliseconds
void snooper::L0x04_0x01_iPodAck(byte cmdStatus, byte cmdID, uint32_t numField)
{
    ESP_LOGI(snooperName,"Ack 0x%02x to command 0x%04x Numfield: %d", cmdStatus, cmdID, numField);
    const byte txPacket[20] = {
        0x04,
        0x00, 0x01,
        cmdStatus,
        cmdID};
    *((uint32_t *)&txPacket[5]) = swap_endian<uint32_t>(numField);
    _queuePacket(txPacket, 5 + 4);
    // Starting delayed timer for the iPodAck
    _pendingCmdId_0x04 = cmdID;
    startTimer(_pendingTimer_0x04, numField);
}

/// @brief Returns the pseudo-UTF8 string for the track info types 01/05/06
/// @param trackInfoType 0x01 : Title / 0x05 : Genre / 0x06 : Composer
/// @param trackInfoChars Character array to pass and package in the tail of the message
void snooper::L0x04_0x0D_ReturnIndexedPlayingTrackInfo(byte trackInfoType, char *trackInfoChars)
{
    ESP_LOGI(snooperName,"Req'd track info type: 0x%02x", trackInfoType);
    byte txPacket[255] = {
        0x04,
        0x00, 0x0D,
        trackInfoType};
    strcpy((char *)&txPacket[4], trackInfoChars);
    _queuePacket(txPacket, 4 + strlen(trackInfoChars) + 1);
}

/// @brief Returns the playing track total duration in milliseconds (Implicit track info 0x00)
/// @param trackDuration_ms trackduration in ms
void snooper::L0x04_0x0D_ReturnIndexedPlayingTrackInfo(uint32_t trackDuration_ms)
{
    ESP_LOGI(snooperName,"Track duration: %d", trackDuration_ms);
    byte txPacket[14] = {
        0x04,
        0x00, 0x0D,
        0x00,
        0x00, 0x00, 0x00, 0x00, // This says it does not have artwork etc
        0x00, 0x00, 0x00, 0x01, // Track length in ms
        0x00, 0x00              // Chapter count (none)
    };
    *((uint32_t *)&txPacket[8]) = swap_endian<uint32_t>(trackDuration_ms);
    _queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Overloaded return of the playing track info : release year
/// @param trackInfoType Can only be 0x02
/// @param releaseYear Fictional release year of the song
void snooper::L0x04_0x0D_ReturnIndexedPlayingTrackInfo(byte trackInfoType, uint16_t releaseYear)
{
    ESP_LOGI(snooperName,"Track info: 0x%02x Release Year: %d", trackInfoType, releaseYear);
    byte txPacket[12] = {
        0x04,
        0x00, 0x0D,
        trackInfoType,
        0x00, 0x00, 0x00, 0x01, 0x01, // First of Jan at 00:00:00
        0x00, 0x00,                   // year goes here
        0x01                          // it was a Monday
    };
    *((uint16_t *)&txPacket[9]) = swap_endian<uint16_t>(releaseYear);
    _queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Returns the L0x04 Lingo Protocol Version : hardcoded to 1.12, consistent with an iPod Classic 5.5G
void snooper::L0x04_0x13_ReturnProtocolVersion()
{
    ESP_LOGI(snooperName,"Lingo protocol version 1.12");
    byte txPacket[] = {
        0x04,
        0x00, 0x13,
        0x01, 0x0C // Protocol version 1.12
    };
    _queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Return the number of records of a given DBCategory (Playlist, tracks...)
/// @param categoryDBRecords The number of records to return
void snooper::L0x04_0x19_ReturnNumberCategorizedDBRecords(uint32_t categoryDBRecords)
{
    ESP_LOGI(snooperName,"Category DB Records: %d", categoryDBRecords);
    byte txPacket[7] = {
        0x04,
        0x00, 0x19,
        0x00, 0x00, 0x00, 0x00};
    *((uint32_t *)&txPacket[3]) = swap_endian<uint32_t>(categoryDBRecords);
    _queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Returns the metadata for a certain database record. Category is implicit
/// @param index Index of the DBRecord
/// @param recordString Metadata to include in the return
void snooper::L0x04_0x1B_ReturnCategorizedDatabaseRecord(uint32_t index, char *recordString)
{
    ESP_LOGI(snooperName,"Database record at index %d : %s", index, recordString);
    byte txPacket[255] = {
        0x04,
        0x00, 0x1B,
        0x00, 0x00, 0x00, 0x00 // Index goes here
    };
    *((uint32_t *)&txPacket[3]) = swap_endian<uint32_t>(index);
    strcpy((char *)&txPacket[7], recordString);
    _queuePacket(txPacket, 7 + strlen(recordString) + 1);
}

/// @brief Returns the current playback status, indicating the position out of the duration
/// @param position Playing position in ms in the track
/// @param duration Total duration of the track in ms
/// @param playStatusArg Playback status (0x00 Stopped, 0x01 Playing, 0x02 Paused, 0xFF Error)
void snooper::L0x04_0x1D_ReturnPlayStatus(uint32_t position, uint32_t duration, byte playStatusArg)
{
    ESP_LOGI(snooperName,"Play status 0x%02x at pos. %d / %d ms", playStatusArg, position, duration);
    byte txPacket[] = {
        0x04,
        0x00, 0x1D,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        playStatusArg};
    *((uint32_t *)&txPacket[3]) = swap_endian<uint32_t>(duration);
    *((uint32_t *)&txPacket[7]) = swap_endian<uint32_t>(position);
    _queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Returns the "Index" of the currently playing track, useful for pulling matching metadata
/// @param trackIndex The trackIndex to return. This is different from the position in the tracklist when Shuffle is ON
void snooper::L0x04_0x1F_ReturnCurrentPlayingTrackIndex(uint32_t trackIndex)
{
    ESP_LOGI(snooperName,"Track index: %d", trackIndex);
    byte txPacket[] = {
        0x04,
        0x00, 0x1F,
        0x00, 0x00, 0x00, 0x00};
    *((uint32_t *)&txPacket[3]) = swap_endian<uint32_t>(trackIndex);
    _queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Returns the track Title after an implicit call for it
/// @param trackTitle Character array to return
void snooper::L0x04_0x21_ReturnIndexedPlayingTrackTitle(char *trackTitle)
{
    ESP_LOGI(snooperName,"Track title: %s", trackTitle);
    byte txPacket[255] = {
        0x04,
        0x00, 0x21};
    strcpy((char *)&txPacket[3], trackTitle);
    _queuePacket(txPacket, 3 + strlen(trackTitle) + 1);
}

/// @brief Returns the track Artist Name after an implicit call for it
/// @param trackArtistName Character array to return
void snooper::L0x04_0x23_ReturnIndexedPlayingTrackArtistName(char *trackArtistName)
{
    ESP_LOGI(snooperName,"Track artist: %s", trackArtistName);
    byte txPacket[255] = {
        0x04,
        0x00, 0x23};
    strcpy((char *)&txPacket[3], trackArtistName);
    _queuePacket(txPacket, 3 + strlen(trackArtistName) + 1);
}

/// @brief Returns the track Album Name after an implicit call for it
/// @param trackAlbumName Character array to return
void snooper::L0x04_0x25_ReturnIndexedPlayingTrackAlbumName(char *trackAlbumName)
{
    ESP_LOGI(snooperName,"Track album: %s", trackAlbumName);
    byte txPacket[255] = {
        0x04,
        0x00, 0x25};
    strcpy((char *)&txPacket[3], trackAlbumName);
    _queuePacket(txPacket, 3 + strlen(trackAlbumName) + 1);
}

/// @brief Only supports currently two types : 0x00 Playback Stopped, 0x01 Track index, 0x04 Track offset
/// @param notification Notification type that can be returned : 0x01 for track index change, 0x04 for the track offset
/// @param numField For 0x01 this is the new Track index, for 0x04 this is the current Track offset in ms
void snooper::L0x04_0x27_PlayStatusNotification(byte notification, uint32_t numField)
{
    ESP_LOGI(snooperName,"Play status 0x%02x Numfield: %d", notification, numField);
    byte txPacket[] = {
        0x04,
        0x00, 0x27,
        notification,
        0x00, 0x00, 0x00, 0x00};
    *((uint32_t *)&txPacket[4]) = swap_endian<uint32_t>(numField);
    _queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Returns the PlayStatusNotification when it is STOPPED (0x00)
/// @param notification Can only be 0x00
void snooper::L0x04_0x27_PlayStatusNotification(byte notification)
{
    ESP_LOGI(snooperName,"Play status 0x%02x STOPPED", notification);
    byte txPacket[] = {
        0x04,
        0x00, 0x27,
        notification};
    _queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Returns the current shuffle status of the PBEngine
/// @param currentShuffleStatus 0x00 No Shuffle, 0x01 Tracks, 0x02 Albums
void snooper::L0x04_0x2D_ReturnShuffle(byte currentShuffleStatus)
{
    ESP_LOGI(snooperName,"Shuffle status: 0x%02x", currentShuffleStatus);
    byte txPacket[] = {
        0x04,
        0x00, 0x2D,
        currentShuffleStatus};
    _queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Returns the current repeat status of the PBEngine
/// @param currentRepeatStatus 0x00 No Repeat, 0x01 One Track, 0x02 All tracks
void snooper::L0x04_0x30_ReturnRepeat(byte currentRepeatStatus)
{
    ESP_LOGI(snooperName,"Repeat status: 0x%02x", currentRepeatStatus);
    byte txPacket[] = {
        0x04,
        0x00, 0x30,
        currentRepeatStatus};
    _queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Returns the number of playing tracks in the current selection (here it is all the tracks)
/// @param numPlayingTracks Number of playing tracks to return
void snooper::L0x04_0x36_ReturnNumPlayingTracks(uint32_t numPlayingTracks)
{
    ESP_LOGI(snooperName,"Playing tracks: %d", numPlayingTracks);
    byte txPacket[] = {
        0x04,
        0x00, 0x36,
        0x00, 0x00, 0x00, 0x00};
    *((uint32_t *)&txPacket[3]) = swap_endian<uint32_t>(numPlayingTracks);
    _queuePacket(txPacket, sizeof(txPacket));
}

#pragma endregion
*/