#include "common.h"
#include "gap.h"
#include "gatt_svc.h"
#include "GPIOcontrol.h"
#include "esp_random.h"

struct timeval timeval_s;
struct TimeInfo timeinfo;
struct ConnInfo conninfo;
enum NotifyType notify_type = NOTIFY_TIME;

int onConnect(struct ble_gap_event *event, void *arg)
{
    gettimeofday(&timeval_s, NULL);
    struct ble_gap_conn_desc desc;
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
    // 如果这一次已经把时间戳发过去了，那么下一次就发送同步指令
    if (notify_type == NOTIFY_TIME)
    {
        ESP_LOGI(TAG, "notify type is time for the host to print");
        xSemaphoreGive(conninfo.should_sync);
    }
    else if (notify_type == NOTIFY_OTHER)
    {
        ESP_LOGI(TAG, "notify type is sync command,then the host will send time to sync");
    }
    return 0;
}

int onSubscribe(struct ble_gap_event *event, void *arg)
{
    ESP_LOGI(TAG, "收到订阅，可以开始同步了");
    xSemaphoreGive(conninfo.should_sync);
    return 0;
}

int onRead(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    // 在中断内已经更新了时间，直接放到om里面让协议栈读取并发送出去
    // 对方根据state判断是否需要同步，notify_time是单纯传时间，notify_other是同步指令
    uint8_t state = notify_type;
    int rc = os_mbuf_append(ctxt->om, &state, sizeof(state));
    os_mbuf_append(ctxt->om, &timeinfo.local_ts, sizeof(timeinfo.local_ts));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

// 收到对方发来的时间戳，计算时间偏差
int onWrite(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    gettimeofday(&timeval_s, NULL);
    if (ctxt->om->om_len == 8)
    {
        uint8_t data_buf[8];
        struct ble_gap_conn_desc desc;
        ble_gap_conn_find(conn_handle, &desc);
        int rc = ble_hs_mbuf_to_flat(ctxt->om, data_buf, ctxt->om->om_len, NULL);
        timeinfo.remote_ts = *(int64_t *)data_buf + desc.conn_itvl * 1250;
        timeinfo.local_ts = (uint64_t)timeval_s.tv_sec * 1000000L + (uint64_t)timeval_s.tv_usec;
        timeinfo.current_bias = timeinfo.remote_ts - timeinfo.local_ts;
        ESP_LOGE(TAG, "local_time = %lld, remote_time = %lld", timeinfo.local_ts, timeinfo.remote_ts);
        return rc;
    }
    return 0;
}

// 发送一个空白notify作为同步指令
void sync_task(void *pvParameters)
{
    while (1)
    {
        if (xSemaphoreTake(conninfo.should_sync, portMAX_DELAY))
        {
            vTaskDelay(esp_random() % 3000 + 1000);
            notify_type = NOTIFY_OTHER;
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