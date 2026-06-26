#pragma once

#include <stdbool.h>
#include "esp_err.h"

typedef enum {
    APP_WIFI_STATE_INIT,
    APP_WIFI_STATE_ESP_NOW_ONLY,
    APP_WIFI_STATE_PROVISIONING,
    APP_WIFI_STATE_CONNECTED,
} app_wifi_state_t;

extern bool g_ota_mode_enabled;

void network_setup(void);
void init_wifi_state_machine(void);
void enter_ota_mode(void);
void exit_ota_mode(void);

// Helpers for monitoring
app_wifi_state_t get_wifi_state(void);
const char* wifi_state_to_str(app_wifi_state_t state);
void get_wifi_ip_str(char *buf, size_t len);