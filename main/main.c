#include "common.h"
#include "gap.h"
#include "gatt_svc.h"
#include "GPIOcontrol.h"
#include "esp_random.h"

struct timeval timeval_s;
struct TimeInfo timeinfo;
struct ConnInfo conninfo;

int onConnect(struct ble_gap_event *event, void *arg)
{
    gettimeofday(&timeval_s, NULL);
    conninfo.is_connect = true;
    conninfo.is_first_sync = true;
    conninfo.conn_handle = event->connect.conn_handle;
    conninfo.connect_ts = (uint64_t)timeval_s.tv_sec * 1000000L + (uint64_t)timeval_s.tv_usec;
    return 0;
}

int onDisconnect(struct ble_gap_event *event, void *arg)
{
    conninfo.is_connect = false;
    conninfo.is_first_sync = false;
    conninfo.conn_handle = BLE_HS_CONN_HANDLE_NONE;
    conninfo.connect_ts = 0;
    return 0;
}

int onNotifyTx(struct ble_gap_event *event, void *arg)
{
    ESP_LOGI(TAG, "自定义回调，发送通知");
    return 0;
}

int onSubscribe(struct ble_gap_event *event, void *arg)
{
    ESP_LOGI(TAG, "自定义回调，订阅");
    xSemaphoreGive(conninfo.should_sync);
    return 0;
}

int onRead(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    ESP_LOGI(TAG, "自定义回调，读");
    int rc;
    rc = os_mbuf_append(ctxt->om, &timeinfo.local_ts, 10);
    ESP_LOGI(TAG, "Notify Noise To Central ");
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

int onWrite(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    struct ble_gap_conn_desc desc;
    set_led_state(1);
    ESP_LOGI(TAG, "自定义回调，写");
    uint8_t data_buf[10];
    int rc = 0;
    gettimeofday(&timeval_s, NULL);
    timeinfo.local_ts = (uint64_t)timeval_s.tv_sec * 1000000L + (uint64_t)timeval_s.tv_usec;
    // 只处理长度为8的消息
    if (ctxt->om->om_len == 8)
    {
        ble_gap_conn_find(conn_handle, &desc);
        rc = ble_hs_mbuf_to_flat(ctxt->om, data_buf, ctxt->om->om_len, NULL);
        timeinfo.remote_ts = *(int64_t *)data_buf + desc.conn_itvl * 1250;
        timeinfo.current_bias = timeinfo.remote_ts - timeinfo.local_ts;
        ESP_LOGE(TAG, "sync finished. local_time_us = %lld,remote_time_us = %lld", timeinfo.local_ts, timeinfo.remote_ts);
        xSemaphoreGive(conninfo.should_sync);
        return rc;
    }
    return rc;
}

// 发送一个空白notify作为同步指令
void sync_task(void *pvParameters)
{
    while (1)
    {
        if (xSemaphoreTake(conninfo.should_sync, portMAX_DELAY))
        {
            vTaskDelay(esp_random() % 3000 + 5000);
            set_led_state(0);
            vTaskDelay(esp_random() % 3000 + 5000);
            notify_data_to_central();
        }
    }
}

void app_main(void)
{
    conninfo.should_sync = xSemaphoreCreateBinary();
    struct GattCallbacks_t gattcbs = {
        .OnRead = onRead,
        .OnWrite = onWrite};
    struct GapCallbacks_t gapcbs = {
        .OnConnect = onConnect,
        .OnDisconnect = onDisconnect,
        .OnNotifyTx = onNotifyTx,
        .OnSubscribe = onSubscribe};
    set_gap_callbacks(gapcbs);
    set_gatt_callbacks(gattcbs);
    ble_init();
    gpio_init();
    xTaskCreate(sync_task, "sync_task", 2048, NULL, 1, NULL);
}