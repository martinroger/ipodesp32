#pragma once
#include "Arduino.h"

// Serial settings
#ifndef MAX_PACKET_SIZE
#define MAX_PACKET_SIZE 1024
#endif
#ifndef SERIAL_TIMEOUT
#define SERIAL_TIMEOUT 60000
#endif
#ifndef INTERBYTE_TIMEOUT
#define INTERBYTE_TIMEOUT 500
#endif
// FreeRTOS Queues
#ifndef CMD_QUEUE_SIZE
#define CMD_QUEUE_SIZE 32
#endif
#ifndef TX_QUEUE_SIZE
#define TX_QUEUE_SIZE 32
#endif
#ifndef TIMER_QUEUE_SIZE
#define TIMER_QUEUE_SIZE 10
#endif
// RX Task settings
#ifndef RX_TASK_STACK_SIZE
#define RX_TASK_STACK_SIZE 4096
#endif
#ifndef RX_TASK_PRIORITY
#define RX_TASK_PRIORITY 2
#endif
#ifndef RX_TASK_INTERVAL_MS
#define RX_TASK_INTERVAL_MS 10
#endif
// Process Task settings
#ifndef PROCESS_TASK_STACK_SIZE
#define PROCESS_TASK_STACK_SIZE 4096
#endif
#ifndef PROCESS_TASK_PRIORITY
#define PROCESS_TASK_PRIORITY 5
#endif
#ifndef PROCESS_INTERVAL_MS
#define PROCESS_INTERVAL_MS 15
#endif
// TX Task settings
#ifndef TX_TASK_STACK_SIZE
#define TX_TASK_STACK_SIZE 4096
#endif
#ifndef TX_TASK_PRIORITY
#define TX_TASK_PRIORITY 20
#endif
#ifndef TX_INTERVAL_MS
#define TX_INTERVAL_MS 20
#endif
// Timer Task settings
#ifndef TIMER_TASK_STACK_SIZE
#define TIMER_TASK_STACK_SIZE 2048
#endif
#ifndef TIMER_TASK_PRIORITY
#define TIMER_TASK_PRIORITY 1
#endif
#ifndef TIMER_INTERVAL_MS
#define TIMER_INTERVAL_MS 5
#endif
// General iPod settings
#ifndef TOTAL_NUM_TRACKS
#define TOTAL_NUM_TRACKS 3000
#endif
#ifndef TRACK_CHANGE_TIMEOUT
#define TRACK_CHANGE_TIMEOUT 1100
#endif