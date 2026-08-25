#ifndef MQTT_MGR_H_
#define MQTT_MGR_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    const char *broker_ip;
    uint16_t broker_port;
    const char *client_id;
} mqtt_mgr_config_t;

int mqtt_mgr_init(const mqtt_mgr_config_t *cfg);
int mqtt_mgr_connect(void);
void mqtt_mgr_process(void);
int mqtt_mgr_publish(const char *topic, const char *payload);
bool mqtt_mgr_is_connected(void);

#endif