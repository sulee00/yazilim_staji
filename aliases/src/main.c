#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <stdio.h>
#include "lcd.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define WIFI_SSID "Redmi 13"
#define WIFI_PSK  "Sule19033"

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;
static bool wifi_connected = false;
static char ip_str[NET_IPV4_ADDR_LEN] = "Baglaniyor...";

static void handle_wifi_connect_result(struct net_mgmt_event_callback *cb)
{
    const struct wifi_status *status = (const struct wifi_status *)cb->info;

    if (status->status) {
        LOG_ERR("Wi-Fi baglanti hatasi: %d", status->status);
        wifi_connected = false;
    } else {
        LOG_INF("Wi-Fi agina basariyla baglanildi!");
        wifi_connected = true;
    }
}

static void handle_ipv4_result(struct net_if *iface)
{
    if (iface && iface->config.ip.ipv4) {
        net_addr_ntop(AF_INET, &iface->config.ip.ipv4->unicast[0].ipv4.address.in_addr,
                      ip_str, sizeof(ip_str));
        LOG_INF("IP Adresi alindi: %s", ip_str);
    }
}

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                    uint64_t mgmt_event, struct net_if *iface)
{
    switch (mgmt_event) {
    case NET_EVENT_WIFI_CONNECT_RESULT:
        handle_wifi_connect_result(cb);
        break;
    case NET_EVENT_IPV4_ADDR_ADD:
        handle_ipv4_result(iface);
        break;
    case NET_EVENT_WIFI_DISCONNECT_RESULT:
        LOG_WRN("Wi-Fi baglantisi koptu!");
        wifi_connected = false;
        snprintf(ip_str, sizeof(ip_str), "Koptu");
        break;
    default:
        break;
    }
}

int main(void)
{
    LOG_INF("Sistem Baslatiliyor...");

    const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c1));
    if (!device_is_ready(i2c_dev)) {
        LOG_ERR("I2C1 aygiti hazir degil!");
        return -1;
    }

    lcd_init(i2c_dev);
    lcd_clear(i2c_dev);
    lcd_set_cursor(i2c_dev, 0, 0);
    lcd_print(i2c_dev, "WiFi Baslatildi");

    struct net_if *iface = net_if_get_default();
    if (!iface) {
        LOG_ERR("Varsayilan ag arayuzu bulunamadi!");
        return -1;
    }

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

    while (1) {
        if (!wifi_connected) {
            LOG_INF("Wi-Fi baglantisi deneniyor...");
            net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(struct wifi_connect_req_params));
        }

        snprintf(line1_buf, sizeof(line1_buf), "Sicaklik: %d C", temp_val);
        
        if (wifi_connected) {
            snprintf(line2_buf, sizeof(line2_buf), "IP:%s", ip_str);
        } else {
            snprintf(line2_buf, sizeof(line2_buf), "WiFi: Baglaniyor");
        }

        lcd_set_cursor(i2c_dev, 0, 0);
        lcd_print(i2c_dev, line1_buf);
        lcd_set_cursor(i2c_dev, 1, 0);
        lcd_print(i2c_dev, line2_buf);

        k_sleep(K_SECONDS(3));
    }

    return 0;
}