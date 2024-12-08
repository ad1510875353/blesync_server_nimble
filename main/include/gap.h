#ifndef __GAP__
#define __GAP__

#include "common.h"

typedef int (*GapCallback)(struct ble_gap_event *event, void *arg);

struct GapCallbacks_t
{
    GapCallback OnConnect;
    GapCallback OnDisconnect;
    GapCallback OnNotifyTx;
    GapCallback OnSubscribe;
};

void ble_init();
void bleprph_advertise();
void set_gap_callbacks(struct GapCallbacks_t cbs);

#endif