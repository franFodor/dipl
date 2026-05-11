#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "mdns.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

#define TAG             "wifi_mgr"
#define NVS_NAMESPACE   "wifi_cfg"
#define NVS_SSID_KEY    "ssid"
#define NVS_PASS_KEY    "pass"
#define AP_SSID         "ESPlay"
#define AP_CHANNEL      1
#define AP_MAX_CONN     4
#define MDNS_HOSTNAME   "espplay"
#define MAX_SCAN        15
#define STA_MAX_RETRIES  5

typedef enum { MODE_AP, MODE_STA } mgr_mode_t;

static esp_netif_t *ap_netif  = NULL;
static esp_netif_t *sta_netif = NULL;

static mgr_mode_t mgr_mode      = MODE_AP;
static bool       sta_connecting = false;
static int        sta_retries    = 0;
static char       sta_ssid[33]   = "";
static char       current_ip[16] = "";

/* ---- helpers ---- */

static void apply_ap_config(void) {
    wifi_config_t cfg = {
        .ap = {
            .channel        = AP_CHANNEL,
            .max_connection = AP_MAX_CONN,
            .authmode       = WIFI_AUTH_OPEN,
        }
    };
    memcpy(cfg.ap.ssid, AP_SSID, strlen(AP_SSID));
    cfg.ap.ssid_len = strlen(AP_SSID);
    esp_wifi_set_config(WIFI_IF_AP, &cfg);
}

static void start_apsta(void) {
    esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    apply_ap_config();
    esp_wifi_start();
    mgr_mode      = MODE_AP;
    current_ip[0] = '\0';
    sta_ssid[0]   = '\0';
    ESP_LOGI(TAG, "APSTA started (AP: %s, STA ready for scan)", AP_SSID);
}

static void start_sta(const char *ssid, const char *pass) {
    esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_STA);

    wifi_config_t cfg = {0};
    strncpy((char *)cfg.sta.ssid,     ssid,             sizeof(cfg.sta.ssid) - 1);
    strncpy((char *)cfg.sta.password, pass ? pass : "", sizeof(cfg.sta.password) - 1);
    esp_wifi_set_config(WIFI_IF_STA, &cfg);

    esp_wifi_start();
    esp_wifi_connect();

    mgr_mode      = MODE_STA;
    sta_connecting = true;
    sta_retries    = 0;
    strncpy(sta_ssid, ssid, sizeof(sta_ssid) - 1);
    current_ip[0] = '\0';
    ESP_LOGI(TAG, "STA connecting to: %s", ssid);
}

static void save_creds(const char *ssid, const char *pass) {
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_str(h, NVS_SSID_KEY, ssid);
    nvs_set_str(h, NVS_PASS_KEY, pass ? pass : "");
    nvs_commit(h);
    nvs_close(h);
}

static void clear_creds(void) {
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_erase_key(h, NVS_SSID_KEY);
    nvs_erase_key(h, NVS_PASS_KEY);
    nvs_commit(h);
    nvs_close(h);
}

static bool load_creds(char *ssid, char *pass) {
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return false;
    size_t sl = 33, pl = 65;
    esp_err_t err = nvs_get_str(h, NVS_SSID_KEY, ssid, &sl);
    nvs_get_str(h, NVS_PASS_KEY, pass, &pl);
    nvs_close(h);
    return err == ESP_OK && ssid[0] != '\0';
}

/* ---- event handler ---- */

static void event_handler(void *arg, esp_event_base_t base,
                           int32_t event_id, void *data) {
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (mgr_mode != MODE_STA) return;
        if (sta_retries < STA_MAX_RETRIES) {
            sta_retries++;
            esp_wifi_connect();
            ESP_LOGW(TAG, "STA retry %d/%d", sta_retries, STA_MAX_RETRIES);
        } else {
            ESP_LOGE(TAG, "STA failed, reverting to APSTA");
            sta_connecting = false;
            sta_retries    = 0;
            start_apsta();
        }
    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
        snprintf(current_ip, sizeof(current_ip), IPSTR, IP2STR(&ev->ip_info.ip));
        sta_connecting = false;
        sta_retries    = 0;
        ESP_LOGI(TAG, "STA got IP: %s", current_ip);
    }
}

/* ---- public API ---- */

void wifi_manager_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_netif_init();
    esp_event_loop_create_default();

    ap_netif  = esp_netif_create_default_wifi_ap();
    sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL);
    esp_event_handler_register(IP_EVENT,   IP_EVENT_STA_GOT_IP, event_handler, NULL);

    mdns_init();
    mdns_hostname_set(MDNS_HOSTNAME);
    mdns_instance_name_set("ESPlay Guitar Assistant");
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);

    char ssid[33] = {0}, pass[65] = {0};
    if (load_creds(ssid, pass)) {
        start_sta(ssid, pass);
    } else {
        start_apsta();
    }
}

int wifi_manager_scan_json(char *buf, size_t buf_size) {
    wifi_scan_config_t scan_cfg = { .show_hidden = false };
    esp_wifi_scan_start(&scan_cfg, true);

    uint16_t count = MAX_SCAN;
    wifi_ap_record_t records[MAX_SCAN];
    esp_wifi_scan_get_ap_records(&count, records);

    char *ptr = buf;
    char *end = buf + buf_size - 2;
    ptr += snprintf(ptr, end - ptr, "[");
    for (int i = 0; i < count && ptr < end; i++) {
        if (i > 0) ptr += snprintf(ptr, end - ptr, ",");
        // Escape quotes and backslashes in the SSID
        char safe[64] = {0};
        int  si = 0;
        for (int j = 0; records[i].ssid[j] && si < 62; j++) {
            char c = records[i].ssid[j];
            if (c == '"' || c == '\\') safe[si++] = '\\';
            safe[si++] = c;
        }
        ptr += snprintf(ptr, end - ptr,
            "{\"ssid\":\"%s\",\"rssi\":%d,\"auth\":%d}",
            safe, records[i].rssi,
            records[i].authmode == WIFI_AUTH_OPEN ? 0 : 1);
    }
    snprintf(ptr, end - ptr + 2, "]");
    return count;
}

typedef struct { char ssid[33]; char pass[65]; } connect_params_t;

static void deferred_connect_task(void *arg) {
    connect_params_t *p = (connect_params_t *)arg;
    vTaskDelay(pdMS_TO_TICKS(800));
    start_sta(p->ssid, p->pass);
    free(p);
    vTaskDelete(NULL);
}

esp_err_t wifi_manager_connect(const char *ssid, const char *pass) {
    save_creds(ssid, pass ? pass : "");
    strncpy(sta_ssid, ssid, sizeof(sta_ssid) - 1);
    sta_connecting = true;

    connect_params_t *p = malloc(sizeof(connect_params_t));
    if (!p) return ESP_ERR_NO_MEM;
    strncpy(p->ssid, ssid,             sizeof(p->ssid) - 1);
    strncpy(p->pass, pass ? pass : "", sizeof(p->pass) - 1);

    xTaskCreate(deferred_connect_task, "wifi_conn", 4096, p, 5, NULL);
    return ESP_OK;
}

void wifi_manager_disconnect(void) {
    clear_creds();
    sta_connecting = false;
    sta_retries    = 0;
    start_apsta();
}

void wifi_manager_status_json(char *buf, size_t buf_size) {
    if (mgr_mode == MODE_AP) {
        snprintf(buf, buf_size,
            "{\"mode\":\"ap\",\"ssid\":\"%s\",\"ip\":\"192.168.4.1\"}",
            AP_SSID);
    } else {
        snprintf(buf, buf_size,
            "{\"mode\":\"sta\",\"ssid\":\"%s\",\"ip\":\"%s\",\"connecting\":%s}",
            sta_ssid, current_ip,
            sta_connecting ? "true" : "false");
    }
}
