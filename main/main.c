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
    set_led_state(1);
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
    set_led_state(0);
    ESP_LOGI(TAG, "disconnect event, reason=%d", event->disconnect.reason);
    conninfo.is_connect = false;
    conninfo.is_first_sync = false;
    conninfo.conn_handle = BLE_HS_CONN_HANDLE_NONE;
    conninfo.connect_ts = 0;
    vTaskDelay(3000);
    bleprph_advertise();
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
    return 0;
}

int onWrite(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    ESP_LOGI(TAG, "自定义回调，写");
    return 0;
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