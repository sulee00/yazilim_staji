#include "app.h"
#include <zephyr/device.h>

int main(void)
{
    app_config_t config = {
        .i2c_dev             = DEVICE_DT_GET(DT_NODELABEL(i2c1)),
        .wifi_ssid           = "Redmi 13",
        .wifi_password       = "WIFI_SIFRENIZ",
        .mqtt_broker_ip      = "10.150.149.139",
        .mqtt_broker_port    = 1883,
        .mqtt_client_id      = "stm32_f446re_node",
        .mqtt_publish_topic  = "sensor/temperature",
        .publish_interval_ms = 3000,
    };

    app_set_config(&config);
    app_init();
    app_start_and_wait();

    return 0;
}