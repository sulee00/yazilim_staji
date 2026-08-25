#include "app.h"
#include "wifi_mgr.h"
#include "mqtt_mgr.h"
#include "lcd.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <stdio.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

static app_config_t app_cfg;
static bool config_set = false;
static bool trigger_mqtt = false;
static const struct device *i2c_dev;

static void on_ip_ready(const char *ip_addr)
{
    trigger_mqtt = true;
}

int app_set_config(const app_config_t *config)
{
    if (!config) return -EINVAL;
    app_cfg = *config;
    config_set = true;
    return 0;
}

int app_init(void)
{
    if (!config_set) return -EPERM;

    i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c1));
    if (device_is_ready(i2c_dev)) {
        lcd_init(i2c_dev);
        lcd_clear(i2c_dev);
        lcd_set_cursor(i2c_dev, 0, 0);
        lcd_print(i2c_dev, "App Init...");
    }

    wifi_mgr_init(on_ip_ready);

    mqtt_mgr_config_t m_cfg = {
        .broker_ip = app_cfg.mqtt_broker_ip,
        .broker_port = app_cfg.mqtt_broker_port,
        .client_id = app_cfg.mqtt_client_id,
    };
    mqtt_mgr_init(&m_cfg);

    return 0;
}

void app_start_and_wait(void)
{
    char line1[17];
    char line2[17];
    char payload[32];
    int dummy_temp = 24;
    uint32_t elapsed_time = 0;

    while (1) {
        if (!wifi_mgr_is_connected()) {
            wifi_mgr_connect(app_cfg.wifi_ssid, app_cfg.wifi_password);
            k_sleep(K_MSEC(2000));
            continue;
        }

        if (trigger_mqtt && !mqtt_mgr_is_connected()) {
            mqtt_mgr_connect();
        }

        mqtt_mgr_process();

        if (mqtt_mgr_is_connected()) {
            elapsed_time += 100;
            if (elapsed_time >= app_cfg.publish_interval_ms) {
                snprintf(payload, sizeof(payload), "{\"temperature\": %d}", dummy_temp);
                mqtt_mgr_publish(app_cfg.mqtt_publish_topic, payload);
                elapsed_time = 0;
            }
        }

        if (device_is_ready(i2c_dev)) {
            snprintf(line1, sizeof(line1), "Sicaklik: %d C", dummy_temp);
            snprintf(line2, sizeof(line2), mqtt_mgr_is_connected() ? "MQTT: Yayinda" : "MQTT: Bekliyor");
            lcd_set_cursor(i2c_dev, 0, 0);
            lcd_print(i2c_dev, line1);
            lcd_set_cursor(i2c_dev, 1, 0);
            lcd_print(i2c_dev, line2);
        }

        k_sleep(K_MSEC(100));
    }
}