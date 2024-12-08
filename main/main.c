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
    set_led_state(1);
    gettimeofday(&timeval_s, NULL);
    conninfo.is_connect = true;
    conninfo.is_first_sync = true;
    conninfo.conn_handle = event->connect.conn_handle;
    conninfo.connect_ts = (uint64_t)timeval_s.tv_sec * 1000000L + (uint64_t)timeval_s.tv_usec;
    return 0;
}

int onDisconnect(struct ble_gap_event *event, void *arg)
{
    set_led_state(0);
    conninfo.is_connect = false;
    conninfo.is_first_sync = false;
    conninfo.conn_handle = BLE_HS_CONN_HANDLE_NONE;
    conninfo.connect_ts = 0;
    vTaskDelay(3000);
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
    return 0;
}

int onRead(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    ESP_LOGI(TAG, "自定义回调，读");
    int rc;
    rc = os_mbuf_append(ctxt->om, &timeinfo.local_ts, sizeof(timeinfo.local_ts));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
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
        return rc;
    }
    return rc;
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
}