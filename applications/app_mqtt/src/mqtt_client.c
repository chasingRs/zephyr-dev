//
// Created by pkj on 2025/10/17.
//

#include <rom/cache.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_mqtt_client, LOG_LEVEL_INF);

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/data/json.h>
#include <zephyr/random/random.h>

#include "mqtt_client.h"
#include "device.h"

#define MSECS_WAIT_RECONNECT 1000
#define MSECS_NET_POLL_TIMEOUT 5000

// Buffers for MQTT client
static uint8_t rx_buffer[CONFIG_NET_SAMPLE_MQTT_PAYLOAD_SIZE];
static uint8_t tx_buffer[CONFIG_NET_SAMPLE_MQTT_PAYLOAD_SIZE];

static uint8_t payload_buffer[CONFIG_NET_SAMPLE_MQTT_PAYLOAD_SIZE];

// MQTT broker details
static struct sockaddr_storage broker;

static struct zsock_pollfd fds[1];
static int nfds;

static const struct json_obj_descr sensor_data_descr[] = {
    JSON_OBJ_DESCR_PRIM(struct sensor_data, unit, JSON_TOK_STRING),
    JSON_OBJ_DESCR_PRIM(struct sensor_data, value, JSON_TOK_FLOAT_FP)
};

bool mqtt_connected = false;

// MQTT client id buffer
static uint8_t client_id[50];

// Tls
#if defined(CONFIG_MQTT_LIB_TLS)
#include "tls_config/cert.h"
#define TLS_SNI_HOSTNAME CONFIG_NET_SAMPLE_MQTT_BROKER_HOSTNAME
#define APP_CA_CERT_TAG 1
static const sec_tag_t m_sec_tags[] = {
    APP_CA_CERT_TAG,
};

static int tls_init() {
    int ret = 0;
    ret = tls_credential_add(APP_CA_CERT_TAG, TLS_CREDENTIAL_CA_CERTIFICATE, ca_certificate, sizeof(ca_certificate));
    if (ret < 0) {
        LOG_ERR("failed to create credentials");
        return ret;
    }
    return ret;
}

#endif

static void prepare_fds(struct mqtt_client *client) {
    if (client->transport.type == MQTT_TRANSPORT_NON_SECURE) {
        fds[0].fd = client->transport.tcp.sock;
    }
#if defined(CONFIG_MQTT_LIB_TLS)
    else if (client->transport.type == MQTT_TRANSPORT_SECURE) {
        fds[0].fd = client->transport.tls.sock;
    }
#endif
    fds[0].events = ZSOCK_POLLIN;
    nfds = 1;
}

static void clear_fds() {
    nfds = 0;
}

static void init_mqtt_client_id() {
    snprintk(client_id, sizeof(client_id), CONFIG_BOARD"_%x", (uint8_t)sys_rand32_get());
}

// Handler callback functions
static inline void on_mqtt_connected() {
    mqtt_connected = true;
    device_write_led(LED_NET, LED_ON);
    LOG_INF("MQTT connected");
    LOG_INF("Hostname: %s", CONFIG_NET_SAMPLE_MQTT_BROKER_HOSTNAME);
    LOG_INF("Client ID: %s", client_id);
    LOG_INF("Port: %s", CONFIG_NET_SAMPLE_MQTT_BROKER_PORT);
    LOG_INF("TLS: %s", IS_ENABLED(CONFIG_MQTT_LIB_TLS)?"Enabled": "Disabled");
}

static inline void on_mqtt_disconnected() {
    clear_fds();
    device_write_led(LED_NET, LED_OFF);
    mqtt_connected = false;
    LOG_INF("MQTT disconnected");
}


static void on_mqtt_publish(struct mqtt_client *const client, const struct mqtt_evt *evt) {
    int ret;
    uint8_t payload[CONFIG_NET_SAMPLE_MQTT_PAYLOAD_SIZE];

    ret = mqtt_read_publish_payload(client, payload,CONFIG_NET_SAMPLE_MQTT_PAYLOAD_SIZE);
    if (ret < 0) {
        LOG_ERR("Failed to read message payload: %d", ret);
        return;
    }
    payload[ret] = '\0';
    LOG_INF("MQTT payload received");
    LOG_INF("Topic: %s, Payload: %s", evt->param.publish.message.topic.topic.utf8, payload);
    if (strcmp((const char *) evt->param.publish.message.topic.topic.utf8,CONFIG_NET_SAMPLE_MQTT_SUB_TOPIC_CMD) == 0) {
        device_handle_command(payload);
    }
}

// MQTT event handler
static void mqtt_event_handler(struct mqtt_client *const client, const struct mqtt_evt *evt) {
    switch (evt->type) {
        case MQTT_EVT_CONNACK:
            if (evt->result != 0) {
                LOG_ERR("MQTT connect failed: %d", evt->result);
                break;
            }
            on_mqtt_connected();
            break;
        case MQTT_EVT_DISCONNECT:
            on_mqtt_disconnected();
            break;
        case MQTT_EVT_PINGRESP:
            LOG_INF("MQTT PING RESPONSE");
            break;
        case MQTT_EVT_PUBACK:
            if (evt->result != 0) {
                LOG_ERR("MQTT PUBACK error: %d", evt->result);
                break;
            }
            LOG_INF("MQTT PUBACK packet: %u", evt->param.puback.message_id);
            break;
        case MQTT_EVT_PUBREC:
            if (evt->result != 0) {
                LOG_ERR("MQTT PUBREC error: %d", evt->result);
                break;
            }
            LOG_INF("MQTT PUBREC packet: %u", evt->param.pubrec.message_id);
            const struct mqtt_pubrel_param rel_param = {
                .message_id = evt->param.pubrec.message_id
            };
            mqtt_publish_qos2_release(client, &rel_param);
            break;
        case MQTT_EVT_PUBREL:
            if (evt->result != 0) {
                LOG_ERR("MQTT PUBREL error: %d", evt->result);
                break;
            }
            LOG_INF("MQTT PUBREL packet: %u", evt->param.pubrel.message_id);
            const struct mqtt_pubcomp_param rec_param = {
                .message_id = evt->param.pubrel.message_id
            };
            mqtt_publish_qos2_complete(client, &rec_param);
            break;
        case MQTT_EVT_PUBCOMP:
            if (evt->result != 0) {
                LOG_ERR("MQTT PUBCOMP error: %d", evt->result);
                break;
            }
            LOG_INF("PUBCOMP packet: %u", evt->param.pubcomp.message_id);
            break;
        case MQTT_EVT_SUBACK:
            if (evt->result == MQTT_SUBACK_FAILURE) {
                LOG_ERR("MQTT SUBACK error: %d", evt->result);
                break;
            }
            LOG_INF("MQTT SUBACK packet: %d", evt->param.suback.message_id);
            break;
        case MQTT_EVT_PUBLISH:
            const struct mqtt_publish_param *p = &evt->param.publish;
            if (p->message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE) {
                const struct mqtt_puback_param ack_param = {
                    .message_id = p->message_id
                };
                mqtt_publish_qos1_ack(client, &ack_param);
            } else if (p->message.topic.qos == MQTT_QOS_2_EXACTLY_ONCE) {
                const struct mqtt_pubrec_param rec_param = {
                    .message_id = p->message_id
                };
                mqtt_publish_qos2_receive(client, &rec_param);
            }
            on_mqtt_publish(client, evt);
            break;
        default: break;
    }
}

static int get_mqtt_payload(struct mqtt_binstr *payload) {
    int ret = 0;
    struct sensor_data data;
    ret = device_read_sensor(&data);
    if (ret < 0) {
        LOG_ERR("Failed to read message data: %d", ret);
        return ret;
    }

    ret = json_obj_encode_buf(sensor_data_descr,ARRAY_SIZE(sensor_data_descr), &data, payload_buffer,
                              CONFIG_NET_SAMPLE_MQTT_PAYLOAD_SIZE);
    if (ret < 0) {
        LOG_ERR("Failed to encode message data: %d", ret);
        return ret;
    }
    payload->len = strlen(payload_buffer);
    payload->data = payload_buffer;

    return ret;
}

int app_mqtt_init(struct mqtt_client *client) {
    int ret = 0;
    uint8_t broker_ip[NET_IPV4_ADDR_LEN];
    struct sockaddr_in *broker4;
    struct addrinfo *result;
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };

    ret = getaddrinfo(CONFIG_NET_SAMPLE_MQTT_BROKER_HOSTNAME,CONFIG_NET_SAMPLE_MQTT_BROKER_PORT, &hints, &result);
    if (ret != 0) {
        LOG_ERR("Failed to get hostname: %d", ret);
        return ret;
    }
    if (result == NULL) {
        LOG_ERR("Broker address not found");
        return -ENOENT;
    }

    broker4 = (struct sockaddr_in *) &broker;
    broker4->sin_addr.s_addr = ((struct sockaddr_in *) result->ai_addr)->sin_addr.s_addr;
    broker4->sin_family = AF_INET;
    broker4->sin_port = ((struct sockaddr_in *) result->ai_addr)->sin_port;
    freeaddrinfo(result);

    inet_ntop(AF_INET, &broker4->sin_addr.s_addr, broker_ip, sizeof(broker_ip));
    LOG_INF("Broker ip: %s", broker_ip);

    init_mqtt_client_id();
    mqtt_client_init(client);
    client->broker = &broker;
    client->evt_cb = mqtt_event_handler;
    client->client_id.utf8 = client_id;
    client->client_id.size = strlen(client_id);
    client->password = NULL;
    client->user_name = NULL;
    client->protocol_version = MQTT_VERSION_3_1_1;

    client->rx_buf = rx_buffer;
    client->rx_buf_size = sizeof(rx_buffer);
    client->tx_buf = tx_buffer;
    client->tx_buf_size = sizeof(tx_buffer);

#if defined(CONFIG_MQTT_LIB_TLS)
    struct mqtt_sec_config *tls_cfg;
    client->transport.type = MQTT_TRANSPORT_SECURE;
    ret = tls_init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize MQTT TLS: %d", ret);
        return ret;
    }
    tls_cfg = &client->transport.tls.config;
    tls_cfg->peer_verify = TLS_PEER_VERIFY_NONE;
    tls_cfg->cipher_list = NULL;
    tls_cfg->sec_tag_list = m_sec_tags;
    tls_cfg->sec_tag_count = ARRAY_SIZE(m_sec_tags);
#if defined(CONFIG_MBEDTLS_SERVER_NAME_INDICATION)
    tls_cfg->hostname = TLS_SNI_HOSTNAME;
#else
    tls_cfg->hostname = NULL;
#endif
#endif
    return ret;
}

static int poll_mqtt_socket(struct mqtt_client *client, int timeout) {
    int ret = 0;
    prepare_fds(client);
    if (nfds <= 0) {
        return -EINVAL;
    };

    ret = zsock_poll(fds, nfds, timeout);
    if (ret < 0) {
        LOG_ERR("poll mqtt socket poll failed: %d", ret);
    }
    return ret;
}

int app_mqtt_connect(struct mqtt_client *client) {
    int ret = 0;
    mqtt_connected = false;
    while (!mqtt_connected) {
        ret = mqtt_connect(client);
        if (ret < 0) {
            LOG_ERR("MQTT connect failed: %d", ret);
            k_msleep(MSECS_WAIT_RECONNECT);
            continue;
        }
        ret = poll_mqtt_socket(client,MSECS_WAIT_RECONNECT);
        if (ret > 0) {
            mqtt_input(client);
        }
        if (!mqtt_connected) {
            mqtt_abort(client);
        }
    }
    return ret;
}

int app_mqtt_subscribe(struct mqtt_client *client) {
    int ret = 0;
    struct mqtt_topic sub_topics[] = {
        {
            .topic = {
                .utf8 = CONFIG_NET_SAMPLE_MQTT_SUB_TOPIC_CMD, .size = strlen(CONFIG_NET_SAMPLE_MQTT_SUB_TOPIC_CMD)
            },
            .qos = IS_ENABLED(CONFIG_NET_SAMPLE_MQTT_QOS_0_AT_MOST_ONCE)
                       ? 0
                       : (
                           IS_ENABLED(CONFIG_NET_SAMPLE_MQTT_QOS_1_AT_LEAST_ONCE) ? 1 : 2
                       )
        }
    };
    const struct mqtt_subscription_list sub_list = {
        .list = sub_topics,
        .list_count = ARRAY_SIZE(sub_topics),
        .message_id = 5841U
    };
    LOG_INF("Subscribe to %d topics", sub_list.list_count);
    ret = mqtt_subscribe(client, &sub_list);
    if (ret != 0) {
        LOG_ERR("Subscribe failed: %d", ret);
    }
    return ret;
}

int app_mqtt_publish(struct mqtt_client *client) {
    int ret = 0;
    struct mqtt_publish_param param;
    struct mqtt_binstr payload;
    static uint16_t msg_id = 1;
    struct mqtt_topic topic = {
        .topic = {
            .utf8 = CONFIG_NET_SAMPLE_MQTT_PUB_TOPIC,
            .size = strlen(CONFIG_NET_SAMPLE_MQTT_PUB_TOPIC),
        },
        .qos = IS_ENABLED(CONFIG_NET_SAMPLE_MQTT_QOS_0_AT_MOST_ONCE)
                   ? 0
                   : (
                       CONFIG_NET_SAMPLE_MQTT_QOS_1_AT_LEAST_ONCE ? 1 : 2
                   ),
    };
    ret = get_mqtt_payload(&payload);
    if (ret != 0) {
        LOG_ERR("get_mqtt_payload failed: %d", ret);
    }
    param.message.topic = topic;
    param.message.payload = payload;
    param.message_id = msg_id++;
    param.dup_flag = 0;
    param.retain_flag = 0;
    ret = mqtt_publish(client, &param);
    if (ret != 0) {
        LOG_ERR("Publish failed: %d", ret);
    }
    LOG_INF("Published to topic: %s, Qos %d", param.message.topic.topic.utf8, param.message.topic.qos);
    return ret;
}

int app_mqtt_process(struct mqtt_client *client) {
    int ret = 0;
    int time_left;

    time_left = mqtt_keepalive_time_left(client);

    ret = poll_mqtt_socket(client, time_left);
    if (ret != 0) {
        if (fds[0].revents & ZSOCK_POLLIN) {
            ret = mqtt_input(client);
            if (ret != 0) {
                LOG_ERR("MQTT input failed: %d", ret);
                return ret;
            }
            if (fds[0].revents & (ZSOCK_POLLHUP | ZSOCK_POLLERR)) {
                LOG_ERR("MQTT socket closed/error");
                return -ENOTCONN;
            }
        }
    } else {
        ret = mqtt_live(client);
        if (ret != 0) {
            LOG_ERR("MQTT live failed: %d", ret);
            return ret;
        }
    }
    return ret;
}

int app_mqtt_run(struct mqtt_client *client) {
    int ret = 0;

    app_mqtt_subscribe(client);
    while (mqtt_connected) {
        k_sleep(K_SECONDS(1));
        ret = app_mqtt_process(client);
        if (ret != 0) {
            LOG_ERR("MQTT process failed: %d", ret);
            break;
        }
    }
    mqtt_disconnect(client,NULL);

    return 0;
}
