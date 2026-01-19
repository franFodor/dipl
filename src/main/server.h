#pragma once

#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#define WIFI_AP_SSID      "ESP_GUITAR_TUNER"
#define WIFI_AP_PASS      "12345678"
#define WIFI_AP_CHANNEL   1
#define WIFI_AP_MAX_CONN  4

void web_server_start(void);
void wifi_ap_start(void);

// call from audio processing
void web_server_update_note(const char *note, float frequency, float cents);
