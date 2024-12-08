#include "common.h"
#include "gatt_svc.h"

uint16_t data_handle;

static struct GattCallbacks_t gattcbs;

static const ble_uuid128_t svc_uuid = BLE_UUID128_INIT(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x00, 0x6E);
static const ble_uuid128_t chr_uuid = BLE_UUID128_INIT(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x00, 0x6E);

static int gatt_svc_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = &chr_uuid.u,
                .access_cb = gatt_svc_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &data_handle,
            },
            {0}},
    },
    {0},
};

static int gatt_svc_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int rc = 0;
    switch (ctxt->op)
    {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        if (attr_handle == data_handle)
        {
            if (gattcbs.OnRead != NULL)
                rc = gattcbs.OnRead(conn_handle, attr_handle, ctxt, arg);
        }
        return rc;

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        if (attr_handle == data_handle)
        {
            if (gattcbs.OnWrite != NULL)
                rc = gattcbs.OnWrite(conn_handle, attr_handle, ctxt, arg);
        }
        return rc;

    default:
        return rc;
    }
}

void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op)
    {
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGD(TAG, "registered service %s with handle=%d\n",
                 ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                 ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGD(TAG, "registering characteristic %s with "
                      "def_handle=%d val_handle=%d\n",
                 ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                 ctxt->chr.def_handle, ctxt->chr.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGD(TAG, "registering descriptor %s with handle=%d\n",
                 ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                 ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
    }
}

void set_gatt_callbacks(struct GattCallbacks_t cbs)
{
    gattcbs.OnRead = cbs.OnRead;
    gattcbs.OnWrite = cbs.OnWrite;
}

void notify_data_to_central()
{
    ble_gatts_chr_updated(data_handle);
}

int gatt_svr_init(void)
{
    int rc;

    // 添加gap服务
    ble_svc_gap_init();

    // 添加gatt服务
    ble_svc_gatt_init();

    // 添加自定义服务
    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0)
        return rc;
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0)
        return rc;

    return 0;
}