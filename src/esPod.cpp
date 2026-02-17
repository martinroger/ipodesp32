#include "esPod.h"

//-----------------------------------------------------------------------
//|                      Cardinal tasks and Timers                      |
//-----------------------------------------------------------------------
#pragma region Tasks

void esPod::_rxTask(void *pvParameters)
{
    esPod *esPodInstance = static_cast<esPod *>(pvParameters);

    byte prevByte = 0x00;
    byte incByte = 0x00;
    byte buf[MAX_PACKET_SIZE] = {0x00};
    uint32_t expLength = 0;
    uint32_t cursor = 0;

    unsigned long lastByteRX = millis();   // Last time a byte was RXed in a packet
    unsigned long lastActivity = millis(); // Last time any RX activity was detected

    aapCommand cmd;

#ifdef STACK_HIGH_WATERMARK_LOG
    UBaseType_t uxHighWaterMark;
    UBaseType_t minHightWaterMark = RX_TASK_STACK_SIZE;
#endif

    while (true)
    {
// Stack high watermark logging
#ifdef STACK_HIGH_WATERMARK_LOG
        uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
        if (uxHighWaterMark < minHightWaterMark)
        {
            minHightWaterMark = uxHighWaterMark;
            ESP_LOGI("HWM", "RX Task High Watermark: %d, used stack: %d", minHightWaterMark, RX_TASK_STACK_SIZE - minHightWaterMark);
        }
#endif

        // If the esPod is disabled, flush the RX buffer and wait for 2*RX_TASK_INTERVAL_MS before checking again
        if (esPodInstance->disabled)
        {
            while (esPodInstance->_targetSerial.available())
            {
                esPodInstance->_targetSerial.read();
            }
            vTaskDelay(pdMS_TO_TICKS(2 * RX_TASK_INTERVAL_MS));
            continue;
        }
        else // esPod is enabled, process away !
        {
            // Use of while instead of if()
            while (esPodInstance->_targetSerial.available())
            {
                // Timestamping the last activity on RX
                lastActivity = millis();
                incByte = esPodInstance->_targetSerial.read();
                // If we are not in the middle of a RX, and we receive a 0xFF 0x55, start sequence, reset expected length and position cursor
                if (prevByte == 0xFF && incByte == 0x55 && !esPodInstance->_rxIncomplete)
                {
                    lastByteRX = millis();
                    esPodInstance->_rxIncomplete = true;
                    expLength = 0;
                    cursor = 0;
                }
                else if (esPodInstance->_rxIncomplete)
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
                            esPodInstance->_rxIncomplete = false;
                            // TODO: Send a NACK to the Accessory
                        }
                        else if (expLength == 0)
                        {
                            ESP_LOGW(__func__, "Expected length is 0, discarding packet");
                            esPodInstance->_rxIncomplete = false;
                            // TODO: Send a NACK to the Accessory
                        }
                    }
                    else // Length is already received
                    {
                        buf[cursor++] = incByte;
                        if (cursor == expLength + 1)
                        {
                            // We have received the expected length + checksum
                            esPodInstance->_rxIncomplete = false;
                            // Check the checksum
                            byte calcChecksum = esPod::_checksum(buf, expLength);
                            if (calcChecksum == incByte)
                            {
                                // Checksum is correct, send the packet to the processing queue
                                // Allocate memory for the payload so it doesn't become out of scope
                                cmd.payload = new byte[expLength];
                                cmd.length = expLength;
                                memcpy(cmd.payload, buf, expLength);
                                if (xQueueSend(esPodInstance->_cmdQueue, &cmd, pdMS_TO_TICKS(5)) == pdTRUE)
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
                else // We are not in the middle of a packet, but we received a byte
                {
                    ESP_LOGD(__func__, "Received byte 0x%02X outside of a packet, discarding", incByte);
                }
                // Always update the previous byte
                prevByte = incByte;
            }
            if (esPodInstance->_rxIncomplete && millis() - lastByteRX > INTERBYTE_TIMEOUT) // If we are in the middle of a packet and we haven't received a byte in 1s, discard the packet
            {
                ESP_LOGW(__func__, "Packet incomplete, discarding");
                esPodInstance->_rxIncomplete = false;
                // cmd.payload = nullptr;
                // cmd.length = 0;
                // TODO: Send a NACK to the Accessory
            }
            if (millis() - lastActivity > SERIAL_TIMEOUT) // If we haven't received any byte in 30s, reset the RX state
            {
                // Reset the timestamp for next Serial timeout
                lastActivity = millis();
#ifndef NO_RESET_ON_SERIAL_TIMEOUT
                ESP_LOGW(__func__, "No activity in %lu ms, resetting RX state", SERIAL_TIMEOUT);
                esPodInstance->resetState();
#endif
            }
            vTaskDelay(pdMS_TO_TICKS(RX_TASK_INTERVAL_MS));
        }
    }
}

void esPod::_processTask(void *pvParameters)
{
    esPod *esPodInstance = static_cast<esPod *>(pvParameters);
    aapCommand incCmd;

#ifdef STACK_HIGH_WATERMARK_LOG
    UBaseType_t uxHighWaterMark;
    UBaseType_t minHightWaterMark = PROCESS_TASK_STACK_SIZE;
#endif

    while (true)
    {
// Stack high watermark logging
#ifdef STACK_HIGH_WATERMARK_LOG
        uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
        if (uxHighWaterMark < minHightWaterMark)
        {
            minHightWaterMark = uxHighWaterMark;
            ESP_LOGI("HWM", "Process Task High Watermark: %d, used stack: %d", minHightWaterMark, PROCESS_TASK_STACK_SIZE - minHightWaterMark);
        }
#endif

        // If the esPod is disabled, check the queue and purge it before jumping to the next cycle
        if (esPodInstance->disabled)
        {
            while (xQueueReceive(esPodInstance->_cmdQueue, &incCmd, 0) == pdTRUE) // Non blocking receive
            {
                // Do not process, just free the memory
                delete[] incCmd.payload;
                incCmd.payload = nullptr;
                incCmd.length = 0;
            }
            vTaskDelay(pdMS_TO_TICKS(2 * PROCESS_INTERVAL_MS));
            continue;
        }
        if (xQueueReceive(esPodInstance->_cmdQueue, &incCmd, 0) == pdTRUE) // Non blocking receive
        {
            // Process the command
            esPodInstance->_processPacket(incCmd.payload, incCmd.length);
            // Free the memory allocated for the payload
            delete[] incCmd.payload;
            incCmd.payload = nullptr;
            incCmd.length = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(PROCESS_INTERVAL_MS));
    }
}

void esPod::_txTask(void *pvParameters)
{
    esPod *esPodInstance = static_cast<esPod *>(pvParameters);
    aapCommand txCmd;

#ifdef STACK_HIGH_WATERMARK_LOG
    UBaseType_t uxHighWaterMark;
    UBaseType_t minHightWaterMark = TX_TASK_STACK_SIZE;
#endif

    while (true)
    {
// Stack high watermark logging
#ifdef STACK_HIGH_WATERMARK_LOG
        uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
        if (uxHighWaterMark < minHightWaterMark)
        {
            minHightWaterMark = uxHighWaterMark;
            ESP_LOGI("HWM", "TX Task High Watermark: %d, used stack: %d", minHightWaterMark, TX_TASK_STACK_SIZE - minHightWaterMark);
        }
#endif

        // If the esPod is disabled, check the queue and purge it before jumping to the next cycle
        if (esPodInstance->disabled)
        {
            while (xQueueReceive(esPodInstance->_txQueue, &txCmd, 0) == pdTRUE)
            {
                // Do not process, just free the memory
                delete[] txCmd.payload;
                txCmd.payload = nullptr;
                txCmd.length = 0;
            }
            vTaskDelay(pdMS_TO_TICKS(TX_INTERVAL_MS));
            continue;
        }
        if (!esPodInstance->_rxIncomplete && esPodInstance->_pendingCmdId_0x00 == 0x00 && esPodInstance->_pendingCmdId_0x03 == 0x00 && esPodInstance->_pendingCmdId_0x04 == 0x00) //_rxTask is not in the middle of a packet, there isn't a valid pending for either lingoes
        {
            // Retrieve from the queue and send the packet
            if (xQueueReceive(esPodInstance->_txQueue, &txCmd, 0) == pdTRUE)
            {
                // vTaskDelay(pdMS_TO_TICKS(TX_INTERVAL_MS));
                // Send the packet
                esPodInstance->_sendPacket(txCmd.payload, txCmd.length);
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

void esPod::_timerTask(void *pvParameters)
{
    esPod *esPodInstance = static_cast<esPod *>(pvParameters);
    TimerCallbackMessage msg;

    while (true)
    {
        if (xQueueReceive(esPodInstance->_timerQueue, &msg, 0) == pdTRUE)
        {
            if (msg.targetLingo == 0x00)
            {
                // esPodInstance->L0x00_0x02_iPodAck(iPodAck_OK, msg.cmdID);
                L0x00::_0x02_iPodAck(esPodInstance, iPodAck_OK, msg.cmdID);
            }
            else if (msg.targetLingo == 0x04)
            {
                // esPodInstance->L0x04_0x01_iPodAck(iPodAck_OK, msg.cmdID);
                L0x04::_0x01_iPodAck(esPodInstance, iPodAck_OK, msg.cmdID);
                if (msg.cmdID == esPodInstance->trackChangeAckPending)
                {
                    esPodInstance->trackChangeAckPending = 0x00;
                    esPodInstance->_albumNameUpdated = false;
                    esPodInstance->_artistNameUpdated = false;
                    esPodInstance->_trackTitleUpdated = false;
                    esPodInstance->_trackDurationUpdated = false;
                }
            }
            else if (msg.targetLingo == 0x03)
            {
                // esPodInstance->L0x04_0x01_iPodAck(iPodAck_OK, msg.cmdID);
                L0x03::_0x00_iPodAck(esPodInstance, iPodAck_OK, msg.cmdID);
                if (msg.cmdID == esPodInstance->trackChangeAckPending)
                {
                    esPodInstance->trackChangeAckPending = 0x00;
                    esPodInstance->_albumNameUpdated = false;
                    esPodInstance->_artistNameUpdated = false;
                    esPodInstance->_trackTitleUpdated = false;
                    esPodInstance->_trackDurationUpdated = false;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(TIMER_INTERVAL_MS));
    }
}
#pragma endregion

#pragma region Timer Callbacks

void esPod::_pendingTimerCallback_0x00(TimerHandle_t xTimer)
{
    esPod *esPodInstance = static_cast<esPod *>(pvTimerGetTimerID(xTimer));
    TimerCallbackMessage msg = {esPodInstance->_pendingCmdId_0x00, 0x00};
    xQueueSendFromISR(esPodInstance->_timerQueue, &msg, NULL);
}

void esPod::_pendingTimerCallback_0x03(TimerHandle_t xTimer)
{
    esPod *esPodInstance = static_cast<esPod *>(pvTimerGetTimerID(xTimer));
    TimerCallbackMessage msg = {esPodInstance->_pendingCmdId_0x03, 0x03};
    xQueueSendFromISR(esPodInstance->_timerQueue, &msg, NULL);
}

void esPod::_pendingTimerCallback_0x04(TimerHandle_t xTimer)
{
    esPod *esPodInstance = static_cast<esPod *>(pvTimerGetTimerID(xTimer));
    TimerCallbackMessage msg = {esPodInstance->_pendingCmdId_0x04, 0x04};
    xQueueSendFromISR(esPodInstance->_timerQueue, &msg, NULL);
}
#pragma endregion

//-----------------------------------------------------------------------
//|                          Packet management                          |
//-----------------------------------------------------------------------
#pragma region Packet management

byte esPod::_checksum(const byte *byteArray, uint32_t len)
{
    uint32_t tempChecksum = len;
    for (int i = 0; i < len; i++)
    {
        tempChecksum += byteArray[i];
    }
    tempChecksum = 0x100 - (tempChecksum & 0xFF);
    return (byte)tempChecksum;
}

void esPod::_sendPacket(const byte *byteArray, uint32_t len)
{
    uint32_t finalLength = len + 4;
    byte tempBuf[finalLength] = {0x00};

    tempBuf[0] = 0xFF;
    tempBuf[1] = 0x55;
    tempBuf[2] = (byte)len;
    for (uint32_t i = 0; i < len; i++)
    {
        tempBuf[3 + i] = byteArray[i];
    }
    tempBuf[3 + len] = esPod::_checksum(byteArray, len);

    _targetSerial.write(tempBuf, finalLength);
}

void esPod::_queuePacket(const byte *byteArray, uint32_t len)
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

void esPod::_queuePacketToFront(const byte *byteArray, uint32_t len)
{
    aapCommand cmdToQueue;
    cmdToQueue.payload = new byte[len];
    cmdToQueue.length = len;
    memcpy(cmdToQueue.payload, byteArray, len);
    if (xQueueSendToFront(_txQueue, &cmdToQueue, pdMS_TO_TICKS(5)) != pdTRUE)
    {
        ESP_LOGW(__func__, "Could not queue packet");
        delete[] cmdToQueue.payload;
        cmdToQueue.payload = nullptr;
        cmdToQueue.length = 0;
    }
}

void esPod::_processPacket(const byte *byteArray, uint32_t len)
{
    byte rxLingoID = byteArray[0];
    const byte *subPayload = byteArray + 1; // Squeeze the Lingo out
    uint32_t subPayloadLen = len - 1;
    switch (rxLingoID) // 0x00 is general Lingo and 0x04 is extended Lingo. Nothing else is expected from the Mini
    {
    case 0x00: // General Lingo
        ESP_LOGD(__func__, "Lingo 0x00 Packet in processor,payload length: %d", subPayloadLen);
        L0x00::processLingo(this, subPayload, subPayloadLen);
        break;

    case 0x03: // Display Remote Lingo
        ESP_LOGD(__func__, "Lingo 0x03 Packet in processor,payload length: %d", subPayloadLen);
        L0x03::processLingo(this, subPayload, subPayloadLen);
        break;

    case 0x04: // Extended Interface Lingo
        ESP_LOGD(__func__, "Lingo 0x04 Packet in processor,payload length: %d", subPayloadLen);
        L0x04::processLingo(this, subPayload, subPayloadLen);
        break;

    default:
        ESP_LOGW(__func__, "Unknown Lingo packet : L0x%02x 0x%02x", rxLingoID, byteArray[1]);
        break;
    }
}
#pragma endregion

//-----------------------------------------------------------------------
//|         Constructor, reset, attachCallback for PB control           |
//-----------------------------------------------------------------------
#pragma region Constructor, destructor, reset and external PB Contoller attach
void esPod::_checkAllMetaUpdated()
{
    if (_albumNameUpdated && _artistNameUpdated && _trackTitleUpdated && _trackDurationUpdated)
    {
        // If all fields have received at least one update and the
        // trackChangeAckPending is still hanging. The failsafe for this one is
        // in the espod _processTask
        if (trackChangeAckPending > 0x00)
        {
            ESP_LOGD(__func__, "Artist+Album+Title+Duration +++ ACK Pending 0x%x\n\tPending duration: %d", trackChangeAckPending, millis() - trackChangeTimestamp);
            if (trackChangeAckPending == 0x11)
            {
                L0x03::_0x00_iPodAck(this, iPodAck_OK, trackChangeAckPending);
            }
            else
            {
                L0x04::_0x01_iPodAck(this, iPodAck_OK, trackChangeAckPending);
            }
            trackChangeAckPending = 0x00;
            ESP_LOGD(__func__, "trackChangeAckPending reset to 0x00");
        }
        _albumNameUpdated = false;
        _artistNameUpdated = false;
        _trackTitleUpdated = false;
        _trackDurationUpdated = false;
        ESP_LOGD(__func__, "Artist+Album+Title+Duration : True -> False");
        // Inform the car
        if (playStatusNotificationState == NOTIF_ON)
        {
            L0x04::_0x27_PlayStatusNotification(this, 0x01, currentTrackIndex);
        }
    }
}

esPod::esPod(Stream &targetSerial)
    : _targetSerial(targetSerial)
{
    // Create queues with pointer structures to byte arrays
    _cmdQueue = xQueueCreate(CMD_QUEUE_SIZE, sizeof(aapCommand));
    _txQueue = xQueueCreate(TX_QUEUE_SIZE, sizeof(aapCommand));
    _timerQueue = xQueueCreate(TIMER_QUEUE_SIZE, sizeof(TimerCallbackMessage));

    if (_cmdQueue == NULL || _txQueue == NULL || _timerQueue == NULL) // Add _timerQueue check
    {
        ESP_LOGE(__func__, "Could not create queues");
    }

    // Create FreeRTOS tasks for compiling incoming commands, processing commands and transmitting commands
    if (_cmdQueue != NULL && _txQueue != NULL && _timerQueue != NULL) // Add _timerQueue check
    {
        xTaskCreatePinnedToCore(_rxTask, "RX Task", RX_TASK_STACK_SIZE, this, RX_TASK_PRIORITY, &_rxTaskHandle, 1);
        xTaskCreatePinnedToCore(_processTask, "Processor Task", PROCESS_TASK_STACK_SIZE, this, PROCESS_TASK_PRIORITY, &_processTaskHandle, 1);
        xTaskCreatePinnedToCore(_txTask, "Transmit Task", TX_TASK_STACK_SIZE, this, TX_TASK_PRIORITY, &_txTaskHandle, 1);
        xTaskCreatePinnedToCore(_timerTask, "Timer Task", TIMER_TASK_STACK_SIZE, this, TIMER_TASK_PRIORITY, &_timerTaskHandle, 1);

        if (_rxTaskHandle == NULL || _processTaskHandle == NULL || _txTaskHandle == NULL || _timerTaskHandle == NULL)
        {
            ESP_LOGE(__func__, "Could not create tasks");
        }
        else
        {
            _pendingTimer_0x00 = xTimerCreate("Pending Timer 0x00", pdMS_TO_TICKS(1000), pdFALSE, this, esPod::_pendingTimerCallback_0x00);
            _pendingTimer_0x03 = xTimerCreate("Pending Timer 0x03", pdMS_TO_TICKS(1000), pdFALSE, this, esPod::_pendingTimerCallback_0x03);
            _pendingTimer_0x04 = xTimerCreate("Pending Timer 0x04", pdMS_TO_TICKS(1000), pdFALSE, this, esPod::_pendingTimerCallback_0x04);
            if (_pendingTimer_0x00 == NULL || _pendingTimer_0x03 == NULL || _pendingTimer_0x04 == NULL)
            {
                ESP_LOGE(__func__, "Could not create timers");
            }
        }
    }
    else
    {
        ESP_LOGE(__func__, "Could not create tasks, queues not created");
    }
}

esPod::~esPod()
{
    aapCommand tempCmd;
    vTaskDelete(_rxTaskHandle);
    vTaskDelete(_processTaskHandle);
    vTaskDelete(_txTaskHandle);
    vTaskDelete(_timerTaskHandle);
    // Stop timers that might be running
    stopTimer(_pendingTimer_0x00);
    stopTimer(_pendingTimer_0x03);
    stopTimer(_pendingTimer_0x04);
    xTimerDelete(_pendingTimer_0x00, 0);
    xTimerDelete(_pendingTimer_0x03, 0);
    xTimerDelete(_pendingTimer_0x04, 0);
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
    vQueueDelete(_timerQueue);
}

void esPod::resetState()
{

    ESP_LOGW(__func__, "esPod resetState called");
    // State variables
    extendedInterfaceModeActive = false;

    // Metadata variables
    trackDuration = 1;
    prevTrackDuration = 1;
    playPosition = 0;

    // Flags for track change management
    _albumNameUpdated = false;
    _artistNameUpdated = false;
    _trackTitleUpdated = false;
    _trackDurationUpdated = false;

    // Playback Engine
    playStatus = PB_STATE_PAUSED;
    playStatusNotificationState = NOTIF_OFF;
    trackChangeAckPending = 0x00;
    shuffleStatus = 0x00;
    repeatStatus = 0x02;

    // TrackList variables
    currentTrackIndex = 0;
    prevTrackIndex = TOTAL_NUM_TRACKS - 1;
    for (uint16_t i = 0; i < TOTAL_NUM_TRACKS; i++)
        trackList[i] = 0;
    trackListPosition = 0;

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

    // Stop timers
    stopTimer(_pendingTimer_0x00);
    stopTimer(_pendingTimer_0x03);
    stopTimer(_pendingTimer_0x04);
    _pendingCmdId_0x00 = 0x00;
    _pendingCmdId_0x03 = 0x00;
    _pendingCmdId_0x04 = 0x00;
}

void esPod::attachPlayControlHandler(playStatusHandler_t playHandler)
{
    _playStatusHandler = playHandler;
    ESP_LOGD(__func__, "PlayControlHandler attached.");
}

void esPod::play()
{
    playStatus = PB_STATE_PLAYING;
    ESP_LOGD(__func__, "esPod set to play.");
}

void esPod::pause()
{
    playStatus = PB_STATE_PAUSED;
    ESP_LOGD(__func__, "esPod paused.");
}

void esPod::stop()
{
    playStatus = PB_STATE_STOPPED;
    ESP_LOGD(__func__, " esPod stopped.");
}

void esPod::updatePlayPosition(uint32_t position)
{
    playPosition = position;
    if (playStatusNotificationState == NOTIF_ON && trackChangeAckPending == 0x00)
        L0x04::_0x27_PlayStatusNotification(this, 0x04, position);
}

void esPod::updateAlbumName(const char *incAlbumName)
{
    if (trackChangeAckPending > 0x00) // There is a pending track change ack
    {
        if (!_albumNameUpdated)
        {
            strcpy(albumName, incAlbumName);
            _albumNameUpdated = true;
            ESP_LOGD(__func__, "Album name update to %s", albumName);
        }
        else
            ESP_LOGD(__func__, "Album name already updated to %s", albumName);
    }
    else // There is no pending track change ack : change is coming from phone/AVRC target
    {
        if (strcmp(incAlbumName, albumName) != 0) // New Album Name
        {
            strcpy(prevAlbumName, albumName); // Preserve the previous album name
            strcpy(albumName, incAlbumName);  // Copy new album name
            _albumNameUpdated = true;
            ESP_LOGD(__func__, "Album name updated to %s", albumName);
        }
        else // Not new album name
            ESP_LOGD(__func__, "Album name already updated to %s", albumName);
    }
    _checkAllMetaUpdated();
}

void esPod::updateArtistName(const char *incArtistName)
{
    if (trackChangeAckPending > 0x00) // There is a pending track change ack
    {
        if (!_artistNameUpdated)
        {
            strcpy(artistName, incArtistName);
            _artistNameUpdated = true;
            ESP_LOGD(__func__, "Artist name update to %s", artistName);
        }
        else
            ESP_LOGD(__func__, "Artist name already updated to %s", artistName);
    }
    else // There is no pending track change ack : change is coming from phone/AVRC target
    {
        if (strcmp(incArtistName, artistName) != 0) // New Artist Name
        {
            strcpy(prevArtistName, artistName); // Preserve the previous artist name
            strcpy(artistName, incArtistName);  // Copy new artist name
            _artistNameUpdated = true;
            ESP_LOGD(__func__, "Artist name updated to %s", artistName);
        }
        else // Not new artist name
            ESP_LOGD(__func__, "Artist name already updated to %s", artistName);
    }
    _checkAllMetaUpdated();
}

void esPod::updateTrackTitle(const char *incTrackTitle)
{
    if (trackChangeAckPending > 0x00) // There is a pending metadata update
    {
        if (!_trackTitleUpdated) // Track title not yet updated
        {
            strcpy(trackTitle, incTrackTitle);
            _trackTitleUpdated = true;
            ESP_LOGD(__func__, "Title update to %s", trackTitle);
        }
        else
            ESP_LOGD(__func__, "Title already updated to %s", trackTitle);
    }
    else // There is no pending track change ack : change is coming from phone/AVRC target.
    {
        if (strcmp(incTrackTitle, trackTitle) != 0) // New track title, we assume it's a NEXT track because otherwise the comparison logic becomes heavy
        {
            // Assume it is Next, perform cursor operations
            trackListPosition = (trackListPosition + 1) % TOTAL_NUM_TRACKS;
            prevTrackIndex = currentTrackIndex;
            currentTrackIndex = (currentTrackIndex + 1) % TOTAL_NUM_TRACKS;
            trackList[trackListPosition] = (currentTrackIndex);

            strcpy(prevTrackTitle, trackTitle); // Preserve the previous track title
            strcpy(trackTitle, incTrackTitle);  // Update the new track title
            _trackTitleUpdated = true;
            ESP_LOGD(__func__, "Title update to %s", trackTitle);
            // ESP_LOGD("AVRC_CB",
            //          "Title rxed, NO ACK pending, AUTONEXT, trackTitleUpdated "
            //          "to %s\n\ttrackPos %d trackIndex %d",
            //          trackTitle, trackListPosition, currentTrackIndex);
        }
        else // Track title is identical, no movement
        {
            ESP_LOGD(__func__, "Title already updated to : %s", trackTitle);
        }
    }
    _checkAllMetaUpdated();
}

void esPod::updateTrackDuration(uint32_t incTrackDuration)
{
    if (trackChangeAckPending > 0x00) // There is a pending metadata update
    {
        if (!_trackDurationUpdated) // Track duration not yet updated
        {
            trackDuration = incTrackDuration;
            _trackDurationUpdated = true;
            ESP_LOGD(__func__, "Track duration updated to %d", trackDuration);
        }
        else
            ESP_LOGD(__func__, "Track duration already updated to %d", trackDuration);
    }
    else // There is no pending track change ack : change is coming from phone/AVRC target.
    {
        if (incTrackDuration != trackDuration) // Different incoming metadata
        {
            prevTrackDuration = trackDuration; // Preserve the current value
            trackDuration = incTrackDuration;  // Then updated it
            _trackDurationUpdated = true;
            ESP_LOGD(__func__, "Track duration updated to %d", trackDuration);
        }
        else
            ESP_LOGD(__func__, "Track duration already updated to %d", trackDuration);
    }
    _checkAllMetaUpdated();
}

#pragma endregion
