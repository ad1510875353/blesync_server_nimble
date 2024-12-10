#ifndef COMMON_H
#define COMMON_H

/* STD APIs */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* ESP APIs */
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "console/console.h"
#include <sys/time.h>

/* FreeRTOS APIs */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

/* NimBLE stack APIs */
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

/* Defines */
#define BT_BD_ADDR_STR "%02x:%02x:%02x:%02x:%02x:%02x"
#define BT_BD_ADDR_HEX(addr) addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]

#define TAG "TimeSync_Server"
#define DEVICE_NAME "ESP32_Server"

struct ConnInfo
{
    bool is_connect;
    bool is_first_sync;
    int64_t connect_ts;
    uint16_t conn_handle;
    SemaphoreHandle_t should_sync;
};

struct TimeInfo
{
    int64_t local_ts;
    int64_t remote_ts;
    int64_t current_bias;
    int64_t previous_bias;
};

enum NotifyType
{
    NOTIFY_TIME = 0,
    NOTIFY_OTHER
};

extern struct timeval timeval_s;
extern struct TimeInfo timeinfo;
extern struct ConnInfo conninfo;
extern enum NotifyType notify_type;

#endif // COMMON_H
