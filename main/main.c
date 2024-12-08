#include "common.h"
#include "gap.h"
#include "gatt_svc.h"
#include "GPIOcontrol.h"
#include "esp_random.h"

bool is_notify_time = true;
bool is_notify_other = false;

struct timeval timeval_s;
struct TimeInfo timeinfo;
struct ConnInfo conninfo;

int onConnect(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    gettimeofday(&timeval_s, NULL);
    ble_gap_conn_find(event->connect.conn_handle, &desc);
    ESP_LOGI(TAG, "connection established,conn_handle=%d;peer addr:" BT_BD_ADDR_STR,
             desc.conn_handle, BT_BD_ADDR_HEX(desc.peer_id_addr.val));
    ESP_LOGI(TAG, "connect interval=%d conn_latency=%d supervision_timeout=%d ",
             desc.conn_itvl, desc.conn_latency, desc.supervision_timeout);
    conninfo.is_connect = true;
    conninfo.is_first_sync = true;
    conninfo.conn_handle = event->connect.conn_handle;
    conninfo.connect_ts = (uint64_t)timeval_s.tv_sec * 1000000L + (uint64_t)timeval_s.tv_usec;
    return 0;
}

int onDisconnect(struct ble_gap_event *event, void *arg)
{
    ESP_LOGI(TAG, "disconnect event, reason=%d", event->disconnect.reason);
    conninfo.is_connect = false;
    conninfo.is_first_sync = false;
    conninfo.conn_handle = BLE_HS_CONN_HANDLE_NONE;
    conninfo.connect_ts = 0;
    bleprph_advertise();
    return 0;
}

int onNotifyTx(struct ble_gap_event *event, void *arg)
{
    ESP_LOGI(TAG, "自定义回调，发送通知");
    // #ifdef CONN_EVENT
    //     vTaskDelay(1000);
    //     esp_restart();
    // #endif
    return 0;
}

int onSubscribe(struct ble_gap_event *event, void *arg)
{
    ESP_LOGI(TAG, "自定义回调，订阅");
#ifdef NOTIFY_EVENT
    xSemaphoreGive(conn_state.should_sync);
#endif
    return 0;
}

int onRead(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    ESP_LOGI(TAG, "自定义回调，读");
    int rc;
    if (is_notify_time)
    {
        rc = os_mbuf_append(ctxt->om, &timeinfo.local_ts, sizeof(timeinfo.local_ts));
        // is_notify_time = false;
        xSemaphoreGive(conninfo.should_sync);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    else if (is_notify_other)
    {
        rc = os_mbuf_append(ctxt->om, &timeinfo.local_ts, 10);
        ESP_LOGI(TAG, "Send Noise");
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    else
    {
        ESP_LOGI(TAG, "  ");
        // ESP_LOGI(TAG, "Send Sync Request via handle = %d", data_handle);
        return 0;
    }
    return rc;
}

int onWrite(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    ESP_LOGI(TAG, "自定义回调，写");
    uint8_t data_buf[10];
    int rc = 0;
    gettimeofday(&timeval_s, NULL);
    timeinfo.local_ts = (uint64_t)timeval_s.tv_sec * 1000000L + (uint64_t)timeval_s.tv_usec;
    // 只处理长度为8的消息
    if (ctxt->om->om_len == 8)
    {
        rc = ble_hs_mbuf_to_flat(ctxt->om, data_buf, ctxt->om->om_len, NULL);
        timeinfo.remote_ts = *(int64_t *)data_buf;
        if (conninfo.is_first_sync)
        {
            timeinfo.current_bias = timeinfo.remote_ts - conninfo.connect_ts;
            conninfo.is_first_sync = false;
            ESP_LOGE(TAG, "local_connect_time = %lld,remote_connect_time = %lld", conninfo.connect_ts, timeinfo.remote_ts);
        }
        // 通知事件
        // timeinfo.current_bias = timeinfo.remote_ts - timeinfo.local_ts;
        // ESP_LOGE(TAG, "sync finished.local_time_us = %lld,remote_time_us = %lld", timeinfo.local_ts, timeinfo.remote_ts);
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
            // 在server端加干扰
            // is_notify_time = false;
            // for(int i=0 ; i< 10; i++)
            // {
            //     is_notify_other = true;
            //     ble_gatts_chr_updated(data_handle);
            //     vTaskDelay(1);
            // }
            is_notify_other = false;
            vTaskDelay(esp_random() % 3000 + 1000);
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
#ifdef NOTIFY_EVENT
    xTaskCreate(sync_task, "sync_task", 2048, NULL, 1, NULL);
#endif
}