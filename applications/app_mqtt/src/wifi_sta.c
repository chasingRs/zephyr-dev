//
// Created by pkj on 2025/10/17.
//

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(wifi_sta, CONFIG_APP_LOG_LEVEL);

#include <zephyr/kernel.h>
#include <zephyr/net/wifi_mgmt.h>

#include "wifi_sta.h"

static struct net_if *sta_iface;
static struct wifi_connect_req_params sta_config;

BUILD_ASSERT(sizeof(CONFIG_WIFI_SAMPLE_SSID)>1, "CONFIG_WIFI_SAMPLE_SSID is empty");

int connect_to_wifi() {
    int ret = 0;

    /* Get STA interface in AP-STA mode. */
    sta_iface = net_if_get_wifi_sta();
    if (!sta_iface) {
        LOG_INF("STA: interface not initialized");
        return -EIO;
    }

    sta_config.ssid = (const uint8_t *) CONFIG_WIFI_SAMPLE_SSID;
    sta_config.ssid_length = sizeof(CONFIG_WIFI_SAMPLE_SSID) - 1;
    sta_config.psk = (const uint8_t *) CONFIG_WIFI_SAMPLE_PSK;
    sta_config.psk_length = sizeof(CONFIG_WIFI_SAMPLE_PSK) - 1;
    sta_config.security = WIFI_SECURITY_TYPE_PSK;
    sta_config.channel = WIFI_CHANNEL_ANY;
    sta_config.band = WIFI_FREQ_BAND_2_4_GHZ;

    LOG_INF("Connecting to SSID: %s", sta_config.ssid);

    ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, sta_iface, &sta_config, sizeof(struct wifi_connect_req_params));
    if (ret != 0) {
        LOG_ERR("Wifi connect request failed");
    }
    return ret;
}
