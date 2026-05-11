#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include <stddef.h>

/**
 * Initialise NVS, TCP/IP stack, WiFi driver, and mDNS.
 * If credentials are saved in NVS, starts in STA mode.
 * Otherwise starts in APSTA mode: AP named "ESPlay" is active so the user
 * can connect and use the web UI; STA interface is ready for scanning.
 * Replaces wifi_ap_start() from server.c.
 */
void wifi_manager_init(void);

/**
 * Scan for nearby networks and write a JSON array to buf.
 * Each element: {"ssid":"...","rssi":-65,"auth":1}
 * auth=0 = open, 1 = requires password.
 * Returns number of networks found.
 */
int wifi_manager_scan_json(char *buf, size_t buf_size);

/**
 * Save credentials to NVS and schedule a STA connection 800 ms later
 * (so the HTTP response is delivered before the AP interface goes down).
 * Returns ESP_OK immediately.
 */
esp_err_t wifi_manager_connect(const char *ssid, const char *pass);

/**
 * Clear saved NVS credentials and switch back to APSTA mode.
 */
void wifi_manager_disconnect(void);

/**
 * Write current WiFi status as JSON to buf.
 * AP:  {"mode":"ap","ssid":"ESPlay","ip":"192.168.4.1"}
 * STA: {"mode":"sta","ssid":"HomeWiFi","ip":"192.168.1.100","connecting":false}
 */
void wifi_manager_status_json(char *buf, size_t buf_size);

#endif // WIFI_MANAGER_H
