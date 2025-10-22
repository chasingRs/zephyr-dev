/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);

#include <zephyr/net/dhcpv4.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/conn_mgr_monitor.h>

#include "mqtt_client.h"
#include "mgmt.h"
#include "wifi_sta.h"
#include "device.h"

static struct mqtt_client client_ctx;
struct k_work_delayable mqtt_publish_work;

void log_mac_addr(struct net_if *iface) {
    struct net_linkaddr *mac;

    mac = net_if_get_link_addr(iface);
    LOG_INF("MAC address: %02X:%02X:%02X:%02X:%02X:%02X", mac->addr[0], mac->addr[1], mac->addr[2], mac->addr[3],
            mac->addr[4], mac->addr[5]);
}

static void publish_work_handler(struct k_work *work) {
    int ret;

    LOG_INF("Publish work");

    if (mqtt_connected) {
        ret = app_mqtt_publish(&client_ctx);
        if (ret != 0) {
            LOG_ERR("publish failed (%d)", ret);
        }
        LOG_INF("published");
        k_work_reschedule(&mqtt_publish_work,K_SECONDS(CONFIG_NET_SAMPLE_MQTT_PUBLISH_INTERVAL));
    } else {
        LOG_INF("not connected");
        k_work_cancel_delayable(&mqtt_publish_work);
    }
}

int main(void) {
    int ret = 0;
    struct net_if *iface;

    ret = device_ready();
    if (ret != true) {
        LOG_ERR("device_ready failed (%d)", ret);
        return ret;
    }

    mgmt_init();

    LOG_INF("Bring up network");

    ret = connect_to_wifi();
    if (ret != 0) {
        LOG_ERR("connect_to_wifi failed (%d)", ret);
    }

    iface = net_if_get_default();
    if (iface == NULL) {
        LOG_ERR("net_if_get_default() failed");
        return -ENETDOWN;
    }
    log_mac_addr(iface);

#if defined(CONFIG_NET_DHCPV4)
    net_dhcpv4_start(iface);
#else
    conn_mgr_mon_resend_status();
#endif
    k_sleep(K_SECONDS(8));
    // while (k_sem_take(&net_conn_sem, K_MSEC(5000)) != 0) {
    //     LOG_INF("Waiting for network connection");
    // }

    ret = app_mqtt_init(&client_ctx);
    if (ret != 0) {
        LOG_ERR("mqtt init failed (%d)", ret);
        return ret;
    }

    k_work_init_delayable(&mqtt_publish_work, publish_work_handler);

    while (true) {
        app_mqtt_connect(&client_ctx);

        k_work_reschedule(&mqtt_publish_work, K_SECONDS(CONFIG_NET_SAMPLE_MQTT_PUBLISH_INTERVAL));

        app_mqtt_run(&client_ctx);
    }

    return 0;
}
