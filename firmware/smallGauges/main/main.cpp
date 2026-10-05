#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_ota_ops.h"
#include "driver/gpio.h"

#include "ota_handler.h"
#include "ST77916.h"
#include "PCF85063.h"
#include "QMI8658.h"
#include "Wireless.h"
#include "TCA9554PWR.h"
#include "BAT_Driver.h"
#include "PWR_Key.h"

#define LV_CONF_INCLUDE_SIMPLE
#include "lv_conf.h"
#include "lvgl.h"
#include "LVGL_Driver.h"

#include "sdkconfig.h"
#include "ui.h"
#include "updateUI.h"
#include "espnow_receiver.h"
#include "lvgl_lock.h"
#include "wifi_ota.h"

// C++ VCAN Receiver Header
#include "vcan_receiver.h"
#include "small_gauge.h"

#define CONFIG_ESPNOW_CHANNEL 1
#define LVGL_BUF_LEN (EXAMPLE_LCD_WIDTH * EXAMPLE_LCD_HEIGHT)

volatile bool ui_ready = false;
volatile bool lvgl_started = false;

static const char *TAG = "MAIN";
#define ONBOARD_LED_GPIO 2

// Define stringification helpers
#define STRINGIFY(x) #x
#define TOSTRING(x)  STRINGIFY(x)

static void lvgl_task(void *arg)
{
    static bool first = true;

    while (1)
    {
        lvgl_lock();

        if (first)
        {
            ESP_LOGI("LVGL", "Starting UI initialization...");
            ui_init();
            ui_ready = true;
            lvgl_started = true;

            first = false;
            ESP_LOGI("LVGL", "UI ready");
        }

        lv_timer_handler();
        lvgl_unlock();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void gauge_task(void *arg)
{
    bool was_stale = false;

    while (!ui_ready || !lvgl_started)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    while (1)
    {
        if (get_wifi_state() != APP_WIFI_STATE_ESP_NOW_ONLY)
        {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // Direct C++ Singleton Call
        bool is_stale = vcan::Receiver::instance().isStale(2000);
        if (is_stale != was_stale)
        {
            if (is_stale)
                ESP_LOGW("GAUGE", "ESP-NOW Link LOST");
            else
                ESP_LOGI("GAUGE", "ESP-NOW Link RESTORED");
            was_stale = is_stale;
        }

        lvgl_lock();
        gauge_ui_update(is_stale);
        lvgl_unlock();

        vTaskDelay(pdMS_TO_TICKS(16));
    }
}

void monitor_task(void *arg)
{
    const esp_partition_t *running = esp_ota_get_running_partition();

    while (1)
    {
        char ip_addr_str[16] = "N/A";
        get_wifi_ip_str(ip_addr_str, sizeof(ip_addr_str));

        ESP_LOGI(TAG, "WiFi State: %s | IP: %s | Partition: %s",
                 wifi_state_to_str(get_wifi_state()), ip_addr_str, running->label);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void Driver_Init(void)
{
    I2C_Init();
    EXIO_Init();
}

extern "C" void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_NONE);
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("small_gauge", ESP_LOG_INFO);
    esp_log_level_set("updateUI", ESP_LOG_NONE);
    esp_log_level_set("UI", ESP_LOG_INFO);
    esp_log_level_set("VCAN_RX", ESP_LOG_INFO);
    ESP_LOGI(TAG, "Starting Truck Gauge Cluster");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running->subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_MIN &&
        running->subtype <= ESP_PARTITION_SUBTYPE_APP_OTA_MAX)
    {
        ESP_LOGI(TAG, "Running from OTA slot, validating image...");
        ESP_ERROR_CHECK(esp_ota_mark_app_valid_cancel_rollback());
    }

    network_setup();
    init_wifi_state_machine();
    Driver_Init();

    ESP_LOGI(TAG, "Provisioning SSID: %s | Hostname: %s", TOSTRING(APP_PROJECT_NAME), TOSTRING(APP_PROJECT_NAME));

    LCD_Init();
    Set_Backlight(100);
    LVGL_Init();

    if (disp != NULL && disp->driver != NULL)
    {
        disp->driver->user_data = panel_handle;
    }

    lvgl_lock_init();

    xTaskCreatePinnedToCore(
        lvgl_task,
        "lvgl_task",
        8192,
        NULL,
        5,
        NULL,
        1);

    espnow_receiver_init(1);
    
    // Direct C++ Singleton Initialization
    vcan::Receiver::instance().init();
    
    small_gauge_init();

    xTaskCreatePinnedToCore(
        gauge_task,
        "gauge_task",
        12288,
        NULL,
        5,
        NULL,
        0);

    xTaskCreate(monitor_task, "monitor_task", 4096, NULL, 1, NULL);
}