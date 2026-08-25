#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include <stdio.h>
#include "lcd.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* Wi-Fi Bilgileri */
#define WIFI_SSID "Redmi 13"
#define WIFI_PSK  "Sule19033"

/* Bilgisayarının Broker IP ve Portu */
#define MQTT_BROKER_ADDR "10.150.149.139"
#define MQTT_BROKER_PORT 1883
#define MQTT_TOPIC       "sensor/temperature"

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;
static bool wifi_connected = false;
static bool ip_ready = false;
static bool mqtt_connected = false;
static bool mqtt_connecting = false;
static char ip_str[NET_IPV4_ADDR_LEN] = "Baglaniyor...";

static struct mqtt_client client_ctx;
static struct sockaddr_storage broker;
static uint8_t rx_buffer[256];
static uint8_t tx_buffer[256];

static void mqtt_evt_handler(struct mqtt_client *const client, const struct mqtt_evt *evt)
{
    switch (evt->type) {
    case MQTT_EVT_CONNACK:
        if (evt->result != 0) {
            LOG_ERR("MQTT Baglanti Hatasi: %d", evt->result);
            mqtt_connected = false;
            mqtt_connecting = false;
        } else {
            LOG_INF("MQTT Broker'a basariyla baglanildi!");
            mqtt_connected = true;
            mqtt_connecting = false;
        }
        break;
    case MQTT_EVT_DISCONNECT:
        LOG_WRN("MQTT Baglantisi kesildi: %d", evt->result);
        mqtt_connected = false;
        mqtt_connecting = false;
        break;
    case MQTT_EVT_PUBACK:
        if (evt->result == 0) {
            LOG_INF("Veri MQTT Broker'a iletildi (PUBACK)");
        }
        break;
    default:
        break;
    }
}

static int mqtt_init_and_connect(void)
{
    struct sockaddr_in *broker4 = (struct sockaddr_in *)&broker;

    broker4->sin_family = AF_INET;
    broker4->sin_port = htons(MQTT_BROKER_PORT);
    zsock_inet_pton(AF_INET, MQTT_BROKER_ADDR, &broker4->sin_addr);

    mqtt_client_init(&client_ctx);

    client_ctx.broker = &broker;
    client_ctx.evt_cb = mqtt_evt_handler;
    client_ctx.client_id.utf8 = (uint8_t *)"stm32_f446re_node";
    client_ctx.client_id.size = strlen("stm32_f446re_node");
    client_ctx.password = NULL;
    client_ctx.user_name = NULL;
    client_ctx.protocol_version = MQTT_VERSION_3_1_1;

    client_ctx.rx_buf = rx_buffer;
    client_ctx.rx_buf_size = sizeof(rx_buffer);
    client_ctx.tx_buf = tx_buffer;
    client_ctx.tx_buf_size = sizeof(tx_buffer);
    client_ctx.transport.type = MQTT_TRANSPORT_NON_SECURE;

    int ret = mqtt_connect(&client_ctx);
    if (ret == 0) {
        mqtt_connecting = true;
    }
    return ret;
}

static int mqtt_publish_temp(int temp)
{
    char payload[32];
    snprintf(payload, sizeof(payload), "{\"temperature\": %d}", temp);

    struct mqtt_publish_param param;
    param.message.topic.qos = MQTT_QOS_0_AT_MOST_ONCE;
    param.message.topic.topic.utf8 = (uint8_t *)MQTT_TOPIC;
    param.message.topic.topic.size = strlen(MQTT_TOPIC);
    param.message.payload.data = (uint8_t *)payload;
    param.message.payload.len = strlen(payload);
    param.message_id = sys_rand32_get();
    param.dup_flag = 0;
    param.retain_flag = 0;

    return mqtt_publish(&client_ctx, &param);
}

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                    uint64_t mgmt_event, struct net_if *iface)
{
    switch (mgmt_event) {
    case NET_EVENT_WIFI_CONNECT_RESULT: {
        const struct wifi_status *status = (const struct wifi_status *)cb->info;
        if (status->status == 0) {
            LOG_INF("Wi-Fi baglantisi tamam.");
            wifi_connected = true;
        } else {
            wifi_connected = false;
        }
        break;
    }
    case NET_EVENT_IPV4_ADDR_ADD:
        if (iface && iface->config.ip.ipv4) {
            net_addr_ntop(AF_INET, &iface->config.ip.ipv4->unicast[0].ipv4.address.in_addr,
                          ip_str, sizeof(ip_str));
            LOG_INF("IP Alindi: %s", ip_str);
            ip_ready = true;
        }
        break;
    case NET_EVENT_WIFI_DISCONNECT_RESULT:
        wifi_connected = false;
        ip_ready = false;
        mqtt_connected = false;
        mqtt_connecting = false;
        break;
    }
}

int main(void)
{
    LOG_INF("Sistem Baslatiliyor...");

    const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c1));
    if (device_is_ready(i2c_dev)) {
        lcd_init(i2c_dev);
        lcd_clear(i2c_dev);
        lcd_set_cursor(i2c_dev, 0, 0);
        lcd_print(i2c_dev, "MQTT Baslatildi");
    }

    struct net_if *iface = net_if_get_default();
    net_mgmt_init_event_callback(&wifi_cb, wifi_mgmt_event_handler,
                                 NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT);
    net_mgmt_init_event_callback(&ipv4_cb, wifi_mgmt_event_handler,
                                 NET_EVENT_IPV4_ADDR_ADD);

    net_mgmt_add_event_callback(&wifi_cb);
    net_mgmt_add_event_callback(&ipv4_cb);

    struct wifi_connect_req_params params = {
        .ssid = (const uint8_t *)WIFI_SSID,
        .ssid_length = strlen(WIFI_SSID),
        .psk = (const uint8_t *)WIFI_PSK,
        .psk_length = strlen(WIFI_PSK),
        .channel = WIFI_CHANNEL_ANY,
        .security = WIFI_SECURITY_TYPE_PSK,
        .band = WIFI_FREQ_BAND_2_4_GHZ,
        .mfp = WIFI_MFP_OPTIONAL,
    };

    char line1_buf[17];
    char line2_buf[17];
    int temp_val = 24;
    int pub_timer = 0;

    while (1) {
        if (!wifi_connected) {
            net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(struct wifi_connect_req_params));
            k_sleep(K_MSEC(2000));
            continue;
        }

        if (ip_ready && !mqtt_connected && !mqtt_connecting) {
            LOG_INF("MQTT baglantisi deneniyor...");
            mqtt_init_and_connect();
        }

        if (mqtt_connecting || mqtt_connected) {
            mqtt_input(&client_ctx);
            mqtt_live(&client_ctx);
        }

        if (mqtt_connected) {
            pub_timer += 100;
            if (pub_timer >= 3000) {
                mqtt_publish_temp(temp_val);
                pub_timer = 0;
            }
        }

        if (device_is_ready(i2c_dev)) {
            snprintf(line1_buf, sizeof(line1_buf), "Sicaklik: %d C", temp_val);
            snprintf(line2_buf, sizeof(line2_buf), mqtt_connected ? "MQTT: Yayinda" : "MQTT: Bekliyor");
            lcd_set_cursor(i2c_dev, 0, 0);
            lcd_print(i2c_dev, line1_buf);
            lcd_set_cursor(i2c_dev, 1, 0);
            lcd_print(i2c_dev, line2_buf);
        }

        k_sleep(K_MSEC(100));
    }

    return 0;
}