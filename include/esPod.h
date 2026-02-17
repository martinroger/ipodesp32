#pragma once
#include "Arduino.h"
#include "L0x00.h"
#include "L0x03.h"
#include "L0x04.h"
#include "esPod_conf.h"
#include "esPod_utils.h"

class esPod
{
    friend class L0x00; // Lingo 0x00 message handlers
    friend class L0x03; // Lingo 0x03 message handlers
    friend class L0x04; // Lingo 0x04 message handlers

public:
    typedef void playStatusHandler_t(byte playControlCommand); // Type definition for the external callback to control playback FROM the espod object

    // State variables
    bool extendedInterfaceModeActive = false; // Indicates if the extended interface mode is accessible (Lingo 0x04 mostly)
    bool disabled = true;                     // espod starts disabled... it means it keeps flushing the Serial until it is ready to process something

    // Metadata variables
    char trackTitle[255] = "Title";  // Current track Title
    char prevTrackTitle[255] = " ";  // Previous track Title
    char artistName[255] = "Artist"; // Current track Artist Name
    char prevArtistName[255] = " ";  // Previous track Artist Name
    char albumName[255] = "Album";   // Current track Album Name
    char prevAlbumName[255] = " ";   // Previous track Album Name
    char trackGenre[255] = "Genre";  // Current track Genre
    char playList[255] = "Spotify";  // Current playlist (always the same)
    char composer[255] = "Composer"; // Current track's composer (sometimes gets requested)
    uint32_t trackDuration = 1;      // Track duration in ms
    uint32_t prevTrackDuration = 1;  // Previous track duration in ms
    uint32_t playPosition = 0;       // Current playing position of the track in ms

    // Playback Engine
    byte playStatus = PB_STATE_PAUSED;            // Current state of the PBEngine
    byte playStatusNotificationState = NOTIF_OFF; // Current state of the Notifications engine
    byte trackChangeAckPending = 0x00;            // Indicate there is a pending track change.
    uint64_t trackChangeTimestamp = 0;            // Trigger for the last track change request. Time outs the pending track change.
    byte shuffleStatus = 0x00;                    // 00 No Shuffle, 0x01 Tracks 0x02 Albums
    byte repeatStatus = 0x02;                     // 00 Repeat off, 01 One track, 02 All tracks

    // TrackList variables
    uint32_t currentTrackIndex = 0;                      // Current internal track index
    uint32_t prevTrackIndex = TOTAL_NUM_TRACKS - 1;      // Previous track index, starts at the end of the tracklist
    const uint32_t totalNumberTracks = TOTAL_NUM_TRACKS; // Total number of tracks. Has little influence
    uint32_t trackList[TOTAL_NUM_TRACKS] = {0};          // Initial track list is filled with 0 track indexs
    uint32_t trackListPosition = 0;                      // Locator for the position of the track ID in the TrackList (of IDs) (i.e. cursor)

private:
    // FreeRTOS Queues
    QueueHandle_t _cmdQueue;   // Incoming commands queue from accessory (car)
    QueueHandle_t _txQueue;    // Outgoing response/commands queue from espod to car
    QueueHandle_t _timerQueue; // Queue for processing "pending" commands timer callbacks (rather than in-ISR processing)

    // FreeRTOS tasks (and methods...)
    TaskHandle_t _rxTaskHandle;      // RX task handle (from car)
    TaskHandle_t _processTaskHandle; // Command dispatcher/processor task handle
    TaskHandle_t _txTaskHandle;      // TX task handle (to car)
    TaskHandle_t _timerTaskHandle;   // Pending command timer task handle

    /// @brief RX Task, sifts through the incoming serial data and compiles packets that pass the checksum and passes them to the processing Queue _cmdQueue. Also handles timeouts and can trigger state resets.
    /// @param pvParameters Unused
    static void _rxTask(void *pvParameters);

    /// @brief Processor task retrieving from the cmdQueue and processing the commands
    /// @param pvParameters
    static void _processTask(void *pvParameters);

    /// @brief Transmit task, retrieves from the txQueue and sends the packets over Serial at high priority but wider timing
    /// @param pvParameters
    static void _txTask(void *pvParameters);

    /// @brief Low priority task to queue acks *outside* of the timer interrupt context
    /// @param pvParameters
    static void _timerTask(void *pvParameters);

    // FreeRTOS timers for delayed acks
    TimerHandle_t _pendingTimer_0x00;
    TimerHandle_t _pendingTimer_0x03;
    TimerHandle_t _pendingTimer_0x04;

    /// @brief Callback for L0x00 pending Ack timer
    /// @param xTimer
    static void _pendingTimerCallback_0x00(TimerHandle_t xTimer);

    /// @brief Callback for L0x03 pending Ack timer
    /// @param xTimer
    static void _pendingTimerCallback_0x03(TimerHandle_t xTimer);

    /// @brief Callback for L0x04 pending Ack timer
    /// @param xTimer
    static void _pendingTimerCallback_0x04(TimerHandle_t xTimer);

    byte _pendingCmdId_0x00; // Command ID that is pending in Lingo 0x00
    byte _pendingCmdId_0x03; // Command ID that is pending in Lingo 0x03
    byte _pendingCmdId_0x04; // Command ID that is pending in Lingo 0x04

    // Serial to the car
    Stream &_targetSerial;

    /// @brief //Calculates the checksum of a packet that starts from i=0 ->Lingo to i=len -> Checksum
    /// @param byteArray Array from Lingo byte to Checksum byte
    /// @param len Length of array (Lingo byte to Checksum byte)
    /// @return Calculated checksum for comparison
    static byte _checksum(const byte *byteArray, uint32_t len);

    /// @brief Composes and sends a packet over the _targetSerial
    /// @param byteArray Array to send, starting with the Lingo byte and without the checksum byte
    /// @param len Length of the array to send
    void _sendPacket(const byte *byteArray, uint32_t len);

    /// @brief Adds a packet to the transmit queue
    /// @param byteArray Array of bytes to add to the queue
    /// @param len Length of data in the array
    void _queuePacket(const byte *byteArray, uint32_t len);

    /// @brief Adds a packet to the transmit queue, but at the front for immediate processing
    /// @param byteArray Array of bytes to add to the queue
    /// @param len Length of data in the array
    void _queuePacketToFront(const byte *byteArray, uint32_t len);

    /// @brief Processes a valid packet and calls the relevant Lingo processor
    /// @param byteArray Checksum-validated packet starting at LingoID
    /// @param len Length of valid data in the packet
    void _processPacket(const byte *byteArray, uint32_t len);

    bool _rxIncomplete = false; // Marker in case of incomplete command sequence reception

    // Device metadata
    const char *_name = ESPIPOD_NAME;          // Published esPOD name
    const uint8_t _SWMajor = 0x01;             // Published SW Major version
    const uint8_t _SWMinor = 0x03;             // Published SW Minor version
    const uint8_t _SWrevision = 0x00;          // Published SW revision
    const char *_serialNumber = "AB345F7HIJK"; // Made-up serial number

    // Handler functions
    playStatusHandler_t *_playStatusHandler = nullptr; // Pointer to external callback used to let the espod instance control playback

    // Boolean flags for track change management
    bool _albumNameUpdated = false;     // Internal flag if the albumName has been updated. Used to send relevant notifications if necessary
    bool _artistNameUpdated = false;    // Internal flag if the artistName has been updated. Used to send relevant notifications if necessary
    bool _trackTitleUpdated = false;    // Internal flag if the trackTitle has been updated. Used to send relevant notifications if necessary
    bool _trackDurationUpdated = false; // Internal flag if the trackDuration has been updated. Used to send relevant notifications if necessary
    void _checkAllMetaUpdated();

public:
    /// @brief Constructor for the esPod class
    /// @param targetSerial (Serial) stream on which the esPod will be communicating
    esPod(Stream &targetSerial);

    /// @brief Destructor for the esPod class. Normally not used.
    ~esPod();

    /// @brief Resets the esPod instance to a "clean" startup state
    void resetState();

    /// @brief Function to attach the playback controller that allows the espod instance to perform playback operations on the audio source
    /// @param playHandler Type-function of a playStatusHandler, linking the espod instance to the audio source controls
    void attachPlayControlHandler(playStatusHandler_t playHandler);

    // Useful wrappers for A2DP and AVRC integration

    /// @brief Sets the esPod instance to "PLAY"
    void play();

    /// @brief Sets the esPod instance to "PAUSED"
    void pause();

    /// @brief Sets the esPod instance to "STOPPED"
    void stop();

    /// @brief Updates the play position (in ms) in the instance. Some internal checks are run to debounce double updates that might happen through AVRC
    /// @param position Current play position in ms
    void updatePlayPosition(uint32_t position);

    /// @brief Checks and updates the album name in the espod instance.
    /// @param incAlbumName Char array of new album name
    void updateAlbumName(const char *incAlbumName);

    /// @brief Checks and updates the artist name in the espod instance.
    /// @param incArtistName Char array of new artist name
    void updateArtistName(const char *incArtistName);

    /// @brief Checks and udpates the track title in the espod instance.
    /// @param incTrackTitle Char array of new track title
    void updateTrackTitle(const char *incTrackTitle);

    /// @brief Checks and updates the track duration in the espod instance.
    /// @param incTrackDuration Track duration in ms.
    void updateTrackDuration(uint32_t incTrackDuration);
};