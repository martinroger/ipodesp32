#pragma once
#include "Arduino.h"

#pragma region ENUMS

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
    DB_CAT_COMPOSER = 0x06
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
