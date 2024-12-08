#ifndef __GATT_SVC__
#define __GATT_SVC__

#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define CONN_EVENT
// #define NOTIFY_EVENT
// #define ALL_EVENT

// 定义函数指针类型
typedef int (*GattCallback)(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);

struct GattCallbacks_t
{
    GattCallback OnRead;
    GattCallback OnWrite;
};

int gatt_svr_init();
void notify_data_to_central();
void set_gatt_callbacks(struct GattCallbacks_t cbs);
void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);

#endif
