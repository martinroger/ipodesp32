#pragma once
#include "Arduino.h"
#include "esPod_conf.h"

// // Possible values for L0x00 0x02 iPodAck
// #define iPodAck_OK 0x00
// #define iPodAck_CmdFailed 0x02
// #define iPodAck_BadParam 0x04
// #define iPodAck_UnknownID 0x05
// #define iPodAck_CmdPending 0x06
// #define iPodAck_TimedOut 0x0F
// #define iPodAck_CmdUnavail 0x10
// #define iPodAck_LingoBusy 0x14

#pragma region ENUMS

enum IPOD_ACK_CODE : byte
{
    iPodAck_OK = 0x00,
    iPodAck_UnknownDBCat = 0x01,
    iPodAck_CmdFailed = 0x02,
    iPodAck_OutOfResources = 0x03,
    iPodAck_BadParam = 0x04,
    iPodAck_UnknownID = 0x05,
    iPodAck_CmdPending = 0x06,
    iPodAck_NotAuthenticated = 0x07,
    iPodAck_BadAuthVersion = 0x08,
    iPodAck_AccPowerModeReqFailed = 0x09,
    iPodAck_CertificateInvalid = 0x0A,
    iPodAck_CertPermissionsInvalid = 0x0B,
    iPodAck_FileInUse = 0x0C,
    iPodAck_FileHndlInvalid = 0x0D,
    iPodAck_DirNotEmpty = 0x0E,
    iPodAck_TimedOut = 0x0F,
    iPodAck_CmdUnavail = 0x10,
    iPodAck_DetectFloat_BadResistor = 0x11,
    iPodAck_SelNotGenius = 0x12,
    iPodAck_MultiDataSection_OK = 0x13,
    iPodAck_LingoBusy = 0x14,
    iPodAck_MaxConnections = 0x15,
    iPodAck_HIDAlreadyInUse = 0x16,
    iPodAck_DroppedData = 0x17,
    iPodAck_OutModeError = 0x18
};

enum PB_STATUS : byte
{
    PB_STATE_STOPPED = 0x00,
    PB_STATE_PLAYING = 0x01,
    PB_STATE_PAUSED = 0x02,
    PB_STATE_ERROR = 0xFF
};

enum PB_COMMAND : byte
{
    PB_CMD_TOGGLE = 0x01,
    PB_CMD_STOP = 0x02,
    PB_CMD_NEXT_TRACK = 0x03,
    PB_CMD_PREVIOUS_TRACK = 0x04,
    PB_CMD_SEEK_FF = 0x05,
    PB_CMD_SEEK_RW = 0x06,
    PB_CMD_STOP_SEEK = 0x07,
    PB_CMD_NEXT = 0x08,
    PB_CMD_PREV = 0x09,
    PB_CMD_PLAY = 0x0A,
    PB_CMD_PAUSE = 0x0B
};

enum DB_CATEGORY : byte
{
    DB_CAT_PLAYLIST = 0x01,
    DB_CAT_ARTIST = 0x02,
    DB_CAT_ALBUM = 0x03,
    DB_CAT_GENRE = 0x04,
    DB_CAT_TRACK = 0x05,
    DB_CAT_COMPOSER = 0x06,
    DB_CAT_AUDIOBOOK = 0x07,
    DB_CAT_PODCAST = 0x08
}; // Just a small selection

enum A2DP_PB_CMD : byte
{
    A2DP_STOP = 0x00,
    A2DP_PLAY = 0x01,
    A2DP_PAUSE = 0x02,
    A2DP_REWIND = 0x03,
    A2DP_NEXT = 0x04,
    A2DP_PREV = 0x05
};

enum NOTIF_STATES : byte
{
    NOTIF_OFF = 0x00,
    NOTIF_ON = 0x01
};

#pragma endregion

struct aapCommand
{
    byte *payload = nullptr;
    uint32_t length = 0;
};

struct TimerCallbackMessage
{
    byte cmdID;       // Command ID to be acked
    byte targetLingo; // Targeted Lingo
};

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

/// @brief (Re)starts a timer and changes the interval on the fly.
/// @param timer Timer handle to (re)start.
/// @param time_ms New interval in milliseconds. No verification is done if this is 0! Defaults to TRACK_CHANGE_TIMEOUT.
inline void startTimer(TimerHandle_t timer, unsigned long time_ms = TRACK_CHANGE_TIMEOUT)
{
    // If the timer is already active, it needs to be stopped without a callback call first
    if (xTimerIsTimerActive(timer) == pdTRUE)
    {
        xTimerStop(timer, 0);
    }
    // Change the period and start the timer
    xTimerChangePeriod(timer, pdMS_TO_TICKS(time_ms), 0);
    xTimerStart(timer, 0);
}

/// @brief Stops a running timer. No status is returned if it was already stopped.
/// @param timer Handle to the Timer that needs to be stopped.
inline void stopTimer(TimerHandle_t timer)
{
    // If the timer is already active, it needs to be stop without a callback call first
    if (xTimerIsTimerActive(timer) == pdTRUE)
    {
        xTimerStop(timer, 0);
    }
}
#pragma endregion