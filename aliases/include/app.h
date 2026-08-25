#ifndef APP_H_
#define APP_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    const char *wifi_ssid;
    const char *wifi_password;
    const char *mqtt_broker_ip;
    uint16_t mqtt_broker_port;
    const char *mqtt_client_id;
    const char *mqtt_publish_topic;
    uint32_t publish_interval_ms;
} app_config_t;

int app_set_config(const app_config_t *config);
int app_init(void);
void app_start_and_wait(void);

#endif