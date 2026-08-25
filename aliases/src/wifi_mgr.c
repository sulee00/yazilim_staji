#include "wifi_mgr.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <string.h>

LOG_MODULE_REGISTER(wifi_mgr, LOG_LEVEL_INF);

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;
static wifi_ip_ready_cb_t ip_callback = NULL;
static bool connected = false;
static char ip_str[NET_IPV4_ADDR_LEN];

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                    uint64_t mgmt_event, struct net_if *iface)
{
    switch (mgmt_event) {
    case NET_EVENT_WIFI_CONNECT_RESULT: {
        const struct wifi_status *status = (const struct wifi_status *)cb->info;
        if (status->status == 0) {
            LOG_INF("Wi-Fi baglantisi kuruldu.");
            connected = true;
        } else {
            connected = false;
        }
        break;
    }
    case NET_EVENT_IPV4_ADDR_ADD:
        if (iface && iface->config.ip.ipv4) {
            net_addr_ntop(AF_INET, &iface->config.ip.ipv4->unicast[0].ipv4.address.in_addr,
                          ip_str, sizeof(ip_str));
            LOG_INF("IP Alindi: %s", ip_str);
            if (ip_callback) {
                ip_callback(ip_str);
            }
        }
        break;
    case NET_EVENT_WIFI_DISCONNECT_RESULT:
        connected = false;
        break;
    }
}

int wifi_mgr_init(wifi_ip_ready_cb_t on_ip_ready)
{
    ip_callback = on_ip_ready;
    net_mgmt_init_event_callback(&wifi_cb, wifi_mgmt_event_handler,
                                 NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT);
    net_mgmt_init_event_callback(&ipv4_cb, wifi_mgmt_event_handler,
                                 NET_EVENT_IPV4_ADDR_ADD);

    net_mgmt_add_event_callback(&wifi_cb);
    net_mgmt_add_event_callback(&ipv4_cb);
    return 0;
}

int wifi_mgr_connect(const char *ssid, const char *psk)
{
    struct net_if *iface = net_if_get_default();
    struct wifi_connect_req_params params = {
        .ssid = (const uint8_t *)ssid,
        .ssid_length = strlen(ssid),
        .psk = (const uint8_t *)psk,
        .psk_length = strlen(psk),
        .channel = WIFI_CHANNEL_ANY,
        .security = WIFI_SECURITY_TYPE_PSK,
        .band = WIFI_FREQ_BAND_2_4_GHZ,
        .mfp = WIFI_MFP_OPTIONAL,
    };
    return net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));
}

bool wifi_mgr_is_connected(void)
{
    return connected;
}