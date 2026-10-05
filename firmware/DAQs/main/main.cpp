#include <stdio.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/i2c.h"

#include "esp_log.h"
#include <esp_timer.h>
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "Globals.h"
#include "CruiseControl.h"
#include "interruptHandlers.h"
#include "vcan_protocol.h"
#include "vcan_sender.h"
#include "sender_sched.h"
#include "daq_cache.h"

// -------------------- LOGGING --------------------
static const char *TAG = "GaugeCluster";

bool DEBUG_SIMULATION_MODE = true;

// -------------------- Legacy I2C Init --------------------
static void i2c_master_init(void)
{
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = (gpio_num_t)I2C_MASTER_SDA_IO;
    conf.scl_io_num = (gpio_num_t)I2C_MASTER_SCL_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;

    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));
}

// -------------------- Legacy I2C Scanner --------------------
void scanI2CBus(void)
{
    ESP_LOGI("I2C_SCAN", "Scanning I2C bus...");
    int devices_found = 0;

    for (uint8_t addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);

        esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK) {
            ESP_LOGI("I2C_SCAN", "Found device at address: 0x%02X", addr);
            devices_found++;
        }
    }

    if (devices_found == 0) {
        ESP_LOGW("I2C_SCAN", "No I2C devices found!");
    }
}

// -------------------- ESP-NOW INIT --------------------
void initEspNow(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_now_init());
    esp_now_peer_info_t peerInfo = {};
    uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ESP-NOW peer");
    } else {
        ESP_LOGI(TAG, "ESP-NOW peer added successfully");
    }

    if (vcan::Sender::instance().init() != ESP_OK) {
        ESP_LOGW(TAG, "vcan::Sender init failed");
    }
}

void configure_log_levels(void)
{
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set("GaugeCluster",    ESP_LOG_INFO);
    esp_log_level_set("DAQ_CACHE",       ESP_LOG_VERBOSE);
    esp_log_level_set("sender_sched",    ESP_LOG_NONE);
    esp_log_level_set("mcp960x",         ESP_LOG_NONE);
    esp_log_level_set("GNSS_TEST",       ESP_LOG_NONE);
    esp_log_level_set("ADC_TEST",        ESP_LOG_NONE);
    esp_log_level_set("RTD_TEST",        ESP_LOG_NONE);
    esp_log_level_set("MAIN_TEST",       ESP_LOG_NONE);
    esp_log_level_set("MERGED_SYSTEM",   ESP_LOG_NONE);
    esp_log_level_set("VCAN_RX",         ESP_LOG_VERBOSE);
    esp_log_level_set("VCAN-SENDER",     ESP_LOG_VERBOSE);
}

// -------------------- MAIN --------------------
extern "C" void app_main(void)
{
    configure_log_levels();

    i2c_master_init();
    scanI2CBus();
    initEspNow();
    initInterruptHandlers();

    if (daq_cache_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize DAQ Cache!");
    }

    // Initialize sender scheduler and register PGNs
    sender_sched_init();

    sender_sched_register(PGN_HEARTBEAT, CAN_PRIORITY_HIGH, 1000);  // 1s
    sender_sched_register(PGN_ENGINE_CORE, CAN_PRIORITY_HIGH, 200); // 5Hz
    sender_sched_register(PGN_PRESSURES, CAN_PRIORITY_HIGH, 250);   // 4Hz
    sender_sched_register(PGN_TEMPS, CAN_PRIORITY_MED, 500);        // 2Hz
    sender_sched_register(PGN_EXHAUST_DIG, CAN_PRIORITY_MED, 250);  // 4Hz
    sender_sched_register(PGN_CRUISE_ODO, CAN_PRIORITY_LOW, 1000);  // 1s
    sender_sched_register(PGN_GPS_POS, CAN_PRIORITY_MED, 1000);     // 1s
    sender_sched_register(PGN_GPS_MOTION, CAN_PRIORITY_MED, 500);   // 2Hz
    sender_sched_register(PGN_GPS_STATUS, CAN_PRIORITY_LOW, 1000);  // 1s
    sender_sched_register(PGN_GNSS_TIME, CAN_PRIORITY_HIGH, 1000);  // 1s

    while (true)
    {
        // 1. Sample hardware off-mutex and update cache snapshot
        daq_cache_update();

        // 2. Tick scheduler (sole owner of vCAN frame packing and transmission)
        sender_sched_tick();

        // 3. Yield to FreeRTOS (~10ms cadence)
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}