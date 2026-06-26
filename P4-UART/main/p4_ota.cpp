#include "p4_ota.h"
#include "esp_log.h"
#include "esp_system.h"

#include "esp_ota_ops.h"   // esp_ota_handle_t, esp_ota_* APIs, OTA_SIZE_UNKNOWN
#include "esp_partition.h" // esp_partition_t
#include "freertos/FreeRTOS.h"
#include "freertos/task.h" // vTaskDelay, pdMS_TO_TICKS
#include <string.h>

static const char *TAG = "P4_OTA";

static esp_ota_handle_t ota_handle = 0;
static const esp_partition_t *update_partition = nullptr;
static bool ota_active = false;
static QueueHandle_t ota_queue = nullptr;

static uint32_t total_bytes_written = 0;

void p4_ota_begin()
{
    if (ota_active)
        return;

    update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition)
    {
        ESP_LOGE(TAG, "No OTA partition found!");
        return;
    }

    ESP_LOGI(TAG, "Starting P4 OTA: partition=%s @ 0x%lx",
             update_partition->label,
             (unsigned long)update_partition->address);

    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        return;
    }

    ota_active = true;
}

void p4_ota_write(const uint8_t *data, uint32_t len)
{
    if (!ota_active)
        p4_ota_begin();

    total_bytes_written += len;

    esp_err_t err = esp_ota_write(ota_handle, data, len);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
    }
}

void p4_ota_finalize_event(void)
{
    ota_event_t evt = {};
    evt.type = OTA_EVENT_FINALIZE;
    xQueueSend(ota_queue, &evt, 0);
}

void p4_ota_finalize()
{
    if (!ota_active)
        return;

    ESP_LOGI(TAG, "Total OTA bytes written: %" PRIu32, total_bytes_written);

    esp_err_t err = esp_ota_end(ota_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        return;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "P4 OTA complete — rebooting!");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

void p4_ota_task(void *arg)
{
    ota_event_t evt;

    for (;;)
    {
        if (xQueueReceive(ota_queue, &evt, portMAX_DELAY))
        {

            if (evt.type == OTA_EVENT_CHUNK)
            {
                p4_ota_write(evt.data, evt.len); // ✔ safe here
            }

            else if (evt.type == OTA_EVENT_FINALIZE)
            {
                ESP_LOGI(TAG, "Finalizing P4 OTA...");
                p4_ota_finalize();
            }
        }
    }
}

void p4_ota_init(void)
{
    ota_queue = xQueueCreate(64, sizeof(ota_event_t));
    if (!ota_queue)
    {
        ESP_LOGE(TAG, "Failed to create OTA queue");
    }
    xTaskCreate(p4_ota_task, "p4_ota_task", 4096, NULL, 8, NULL);
}

void p4_ota_queue_chunk(const uint8_t *data, uint32_t len)
{
    ota_event_t evt = {};
    evt.type = OTA_EVENT_CHUNK;
    evt.len = len;
    memcpy(evt.data, data, len);

    if (ota_queue)
    {
        // Block until there is space in the queue
        if (xQueueSend(ota_queue, &evt, portMAX_DELAY) != pdPASS)
        {
            ESP_LOGE(TAG, "Failed to enqueue OTA chunk (len=%" PRIu32 ")", len);
        }
    }
}
