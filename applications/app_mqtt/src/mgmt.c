//
// Created by pkj on 2025/10/17.
//
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(mgmt, CONFIG_APP_LOG_LEVEL);

#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>

#include "mgmt.h"

#define MACSTR "%02X:%02X:%02X:%02X:%02X:%02X"

#define NET_EVENT_MASK                                                 \
    (NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT |     \
    NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED| NET_EVENT_IPV4_ADDR_ADD  )

K_SEM_DEFINE(net_conn_sem, 0, 1);

static struct net_mgmt_event_callback cb;

void mgmt_evt_handler(struct net_mgmt_event_callback *callback, uint64_t mgmt_event, struct net_if *iface) {
    switch (mgmt_event) {
        case NET_EVENT_L4_CONNECTED:
        case NET_EVENT_IPV4_ADDR_ADD:
            k_sem_give(&net_conn_sem);
            LOG_INF("Link up");
            break;
        case NET_EVENT_L4_DISCONNECTED:
            LOG_INF("Link down");
            break;
        case NET_EVENT_WIFI_CONNECT_RESULT:
            LOG_INF("Wifi Connected");
            break;
        case NET_EVENT_WIFI_DISCONNECT_RESULT:
            LOG_INF("Wifi Disconnected");
            break;
        default: break;
    }
}

void mgmt_init(void) {
    net_mgmt_init_event_callback(&cb, mgmt_evt_handler, NET_EVENT_MASK);
    net_mgmt_add_event_callback(&cb);
}
