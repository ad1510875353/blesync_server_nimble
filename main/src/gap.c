#include "common.h"
#include "gatt_svc.h"
#include "gap.h"

void ble_store_config_init(void);

static uint8_t own_addr_type;
static struct GapCallbacks_t gapcbs;

static int bleprph_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    switch (event->type)
    {
    // 连接事件
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0)
        {
            ble_gap_conn_find(event->connect.conn_handle, &desc);
            ESP_LOGI(TAG, "connection established,conn_handle=%d;peer addr:" BT_BD_ADDR_STR,
                     desc.conn_handle, BT_BD_ADDR_HEX(desc.peer_id_addr.val));
            ESP_LOGI(TAG, "connect interval=%d conn_latency=%d supervision_timeout=%d ",
                     desc.conn_itvl, desc.conn_latency, desc.supervision_timeout);
            if (gapcbs.OnConnect != NULL)
                gapcbs.OnConnect(event, arg);
        }
        else
        {
            ESP_LOGI(TAG, "connect failed,status=%d", event->connect.status);
            bleprph_advertise();
        }
        return 0;

    // 断开连接
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnect event, reason=%d", event->disconnect.reason);
        if (gapcbs.OnDisconnect != NULL)
            gapcbs.OnDisconnect(event, arg);
        bleprph_advertise();
        return 0;

    // 广播结束
    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "advertise complete; reason=%d", event->adv_complete.reason);
        bleprph_advertise();
        return 0;

    // 发送通知完成
    case BLE_GAP_EVENT_NOTIFY_TX:
        ESP_LOGI(TAG,
                 "notify_tx event; conn_handle=%d attr_handle=%d "
                 "status=%d is_indication=%d",
                 event->notify_tx.conn_handle, event->notify_tx.attr_handle,
                 event->notify_tx.status, event->notify_tx.indication);
        if (gapcbs.OnNotifyTx != NULL)
            gapcbs.OnNotifyTx(event, arg);
        return 0;

    // 收到订阅
    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG,
                 "subscribe event; conn_handle=%d attr_handle=%d "
                 "reason=%d prevn=%d curn=%d previ=%d curi=%d",
                 event->subscribe.conn_handle, event->subscribe.attr_handle,
                 event->subscribe.reason, event->subscribe.prev_notify,
                 event->subscribe.cur_notify, event->subscribe.prev_indicate,
                 event->subscribe.cur_indicate);
        if (gapcbs.OnSubscribe != NULL)
            gapcbs.OnSubscribe(event, arg);
        return 0;

    // 主机更新连接参数
    case BLE_GAP_EVENT_CONN_UPDATE:
        ble_gap_conn_find(event->conn_update.conn_handle, &desc);
        ESP_LOGI(TAG,
                 "connection updated event, status=%d; conn_itvl=%d "
                 "conn_latency=%d supervision_timeout=%d ",
                 event->conn_update.status, desc.conn_itvl,
                 desc.conn_latency, desc.supervision_timeout);
        return 0;

    // 交换mtu
    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "mtu update event; conn_handle=%d cid=%d mtu=%d",
                 event->mtu.conn_handle, event->mtu.channel_id,
                 event->mtu.value);
        return 0;
    }

    return 0;
}

static void bleprph_on_reset(int reason)
{
    ESP_LOGE(TAG, "Resetting state; reason=%d\n", reason);
}

static void bleprph_on_sync(void)
{
    int rc;

    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "error determining address type; rc=%d\n", rc);
        return;
    }

    // 打印自己的地址
    uint8_t addr[6] = {0};
    rc = ble_hs_id_copy_addr(own_addr_type, addr, NULL);
    ESP_LOGI(TAG, "Device Address:" BT_BD_ADDR_STR "\n", BT_BD_ADDR_HEX(addr));

    // 开始广播
    bleprph_advertise();
}

static void bleprph_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started\n");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void set_gap_callbacks(struct GapCallbacks_t cbs)
{
    gapcbs.OnConnect = cbs.OnConnect;
    gapcbs.OnDisconnect = cbs.OnDisconnect;
    gapcbs.OnNotifyTx = cbs.OnNotifyTx;
    gapcbs.OnSubscribe = cbs.OnSubscribe;
}

void ble_init(void)
{
    int rc;

    rc = nvs_flash_init();
    if (rc == ESP_ERR_NVS_NO_FREE_PAGES || rc == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        rc = nvs_flash_init();
    }
    ESP_ERROR_CHECK(rc);

    rc = nimble_port_init();
    if (rc != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init nimble %d ", rc);
        return;
    }

    ble_hs_cfg.reset_cb = bleprph_on_reset;
    ble_hs_cfg.sync_cb = bleprph_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;

    // 存储主机配置
    ble_store_config_init();

    rc = gatt_svr_init();
    assert(rc == 0);

    rc = ble_svc_gap_device_name_set(DEVICE_NAME);
    assert(rc == 0);

    nimble_port_freertos_init(bleprph_host_task);
}

void bleprph_advertise(void)
{
    // 获取设备名字
    const char *name = ble_svc_gap_device_name();

    // 广播字段
    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof fields);
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.appearance = 0x0200;
    fields.appearance_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    fields.tx_pwr_lvl_is_present = 1;
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;
    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "error setting advertisement data; rc=%d", rc);
        return;
    }

    // 广播参数 可发现非定向
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, bleprph_gap_event, NULL);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "error enabling advertisement; rc=%d", rc);
        return;
    }
    ESP_LOGI(TAG, "start advertisement successed\n");
}