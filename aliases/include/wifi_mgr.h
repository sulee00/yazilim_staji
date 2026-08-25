#ifndef WIFI_MGR_H_
#define WIFI_MGR_H_

#include <stdbool.h>

typedef void (*wifi_ip_ready_cb_t)(const char *ip_addr);

int wifi_mgr_init(wifi_ip_ready_cb_t on_ip_ready);
int wifi_mgr_connect(const char *ssid, const char *psk);
bool wifi_mgr_is_connected(void);

#endif