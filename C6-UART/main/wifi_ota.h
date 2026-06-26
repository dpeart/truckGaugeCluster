#pragma once

#include <stdbool.h>
#include "esp_err.h"

typedef enum
{
    APP_WIFI_STATE_INIT,
    APP_WIFI_STATE_ESP_NOW_ONLY,
    APP_WIFI_STATE_PROVISIONING,
    APP_WIFI_STATE_CONNECTED,
} app_wifi_state_t;

typedef enum
{
    OTA_TARGET_C6 = 0, // C6 updates itself
    OTA_TARGET_P4 = 1  // C6 streams firmware to P4
} ota_target_t;

// Set which OTA target is active (called by c6_modes.c)
void wifi_ota_set_target(ota_target_t target);

// Get current OTA target (used by ota_handler.c)
ota_target_t wifi_ota_get_target(void);

extern bool g_ota_mode_enabled;

void network_setup(void);
void init_wifi_state_machine(void);
void enter_ota_mode(void);
void exit_ota_mode(void);
void enter_provisioning_mode(void);
void exit_provisioning_mode(void);

app_wifi_state_t get_wifi_state(void);
const char *wifi_state_to_str(app_wifi_state_t state);
void get_wifi_ip_str(char *buf, size_t len);
