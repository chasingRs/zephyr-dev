//
// Created by pkj on 2025/10/17.
//

#ifndef APP_MQTT_CLIENT_H
#define APP_MQTT_CLIENT_H

extern bool mqtt_connected;

int app_mqtt_init(struct mqtt_client *client);

int app_mqtt_connect(struct mqtt_client *client);

int app_mqtt_run(struct mqtt_client *client);

int app_mqtt_subscribe(struct mqtt_client *client);

int app_mqtt_publish(struct mqtt_client *client);

#endif //APP_MQTT_CLIENT_H
