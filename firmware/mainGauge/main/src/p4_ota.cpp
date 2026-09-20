#include "p4_ota.h"
#include "esp_log.h"
#include "esp_system.h"
#include "p4_uart.h"
#include "p4_modes.h"

#include "esp_ota_ops.h"   // esp_ota_handle_t, esp_ota_* APIs, OTA_SIZE_UNKNOWN
#include "esp_partition.h" // esp_partition_t
#include "freertos/FreeRTOS.h"
#include "freertos/task.h" // vTaskDelay, pdMS_TO_TICKS
#include <string.h>
#include <inttypes.h>

static const char *TAG = "P4_OTA";

// Core State Engine Controls
static esp_ota_handle_t ota_handle = 0;
static const esp_partition_t *update_partition = nullptr;
static bool ota_active = false;
QueueHandle_t ota_queue = nullptr;

// Consolidated Progress Trackers
static size_t ota_total_expected_bytes = 0;
static uint32_t total_bytes_written = 0;
static uint8_t last_ui_percent = 0;

void p4_ota_begin()
{
    if (ota_active)
        return;

    update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition)
    {
        // ========================================================
        // UI UPDATE: Notify display that an error occurred
        // ========================================================
        // cluster_ui_show_progress("Partition Error!", 0);
        ESP_LOGE(TAG, "No OTA partition found!");
        return;
    }

    ESP_LOGI(TAG, "Starting P4 OTA: partition=%s @ 0x%lx",
             update_partition->label,
             (unsigned long)update_partition->address);

    // Pass the actual expected size if known, or fallback safely to dynamic sizing
    size_t start_size = (ota_total_expected_bytes > 0) ? ota_total_expected_bytes : OTA_SIZE_UNKNOWN;
    esp_err_t err = esp_ota_begin(update_partition, start_size, &ota_handle);
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

    esp_err_t err = esp_ota_write(ota_handle, data, len);
    if (err == ESP_OK)
    {
        total_bytes_written += len;
    }
    else
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
    static ota_event_t evt;

    for (;;)
    {
        if (xQueueReceive(ota_queue, &evt, portMAX_DELAY))
        {
            // ============================================================
            // 1. INITIALIZE OTA SESSION (OTA_EVENT_BEGIN)
            // ============================================================
            if (evt.type == OTA_EVENT_BEGIN)
            {
                ESP_LOGI(TAG, "OTA_EVENT_BEGIN: Initializing OTA session for %u bytes...",
                         (unsigned)evt.len);

                // Cache metrics
                ota_total_expected_bytes = evt.len;
                total_bytes_written = 0;
                last_ui_percent = 0;

                update_partition = esp_ota_get_next_update_partition(NULL);
                if (update_partition != NULL)
                {
                    ESP_LOGI(TAG, "Using partition '%s' @ 0x%lx",
                             update_partition->label,
                             (unsigned long)update_partition->address);

                    esp_err_t err = esp_ota_begin(update_partition, evt.len, &ota_handle);
                    if (err == ESP_OK)
                    {
                        ESP_LOGI(TAG, "OTA session initialized successfully! Ready for stream.");
                        ota_active = true;

                        // Single ACK to C6: "erase + begin" complete
                        uart_send_frame(CMD_ACK, NULL, 0);
                    }
                    else
                    {
                        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
                        ota_active = false;
                        uart_send_frame(CMD_NACK, NULL, 0);
                    }
                }
                else
                {
                    ESP_LOGE(TAG, "Failed to locate OTA partition.");
                    ota_active = false;
                    uart_send_frame(CMD_NACK, NULL, 0);
                }
            }

            // ============================================================
            // 2. WRITE CHUNK (OTA_EVENT_CHUNK)
            // ============================================================
            else if (evt.type == OTA_EVENT_CHUNK)
            {
                if (!ota_active)
                {
                    ESP_LOGE(TAG, "Chunk received but OTA not active — dropping!");
                    uart_send_frame(CMD_NACK, NULL, 0);
                    continue;
                }

                ESP_LOGI(TAG, "OTA_EVENT_CHUNK: len=%u", (unsigned)evt.len);

                esp_err_t err = esp_ota_write(ota_handle, evt.data, evt.len);
                if (err == ESP_OK)
                {
                    total_bytes_written += evt.len;

                    if (ota_total_expected_bytes > 0)
                    {
                        uint8_t current_percent =
                            (uint32_t)(total_bytes_written * 100) / ota_total_expected_bytes;
                        if (current_percent > 100)
                            current_percent = 100;

                        if (current_percent != last_ui_percent)
                        {
                            last_ui_percent = current_percent;
                            ESP_LOGI(TAG, "OTA Progress: %u%% (%lu/%lu)",
                                     current_percent,
                                     (unsigned long)total_bytes_written,
                                     (unsigned long)ota_total_expected_bytes);
                        }
                    }

                    ESP_LOGI(TAG, "Chunk write OK, sending ACK");
                    uart_send_frame(CMD_ACK, NULL, 0);
                }
                else
                {
                    ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
                    ESP_LOGW(TAG, "Sending NACK for failed chunk");
                    uart_send_frame(CMD_NACK, NULL, 0);
                }
            }

            // ============================================================
            // 3. FINALIZE (OTA_EVENT_FINALIZE)
            // ============================================================
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
    // FIX 1: Drop queue depth from 64 slots down to 2 or 3 slots.
    // 3 slots * 4.1KB = ~12.3KB. This fits perfectly inside internal RAM.
    ota_queue = xQueueCreate(3, sizeof(ota_event_t));

    if (!ota_queue)
    {
        ESP_LOGE(TAG, "Failed to create OTA queue - Out of Memory!");
        return; // Stop here if memory allocation failed!
    }

    // FIX 2: Increase the task stack size from 4096 to 8192.
    // This gives the task plenty of workspace to handle a local 4.1KB ota_event_t struct.
    xTaskCreate(p4_ota_task, "p4_ota_task", 8192, NULL, 8, NULL);
}

void p4_ota_queue_chunk(const uint8_t *data, uint32_t len)
{
    // FIX: Lives in permanent RAM, completely removing the 4.1KB footprint from the stack
    static ota_event_t evt;

    // Explicitly clear it since static variables retain data between calls
    memset(&evt, 0, sizeof(ota_event_t));

    evt.type = OTA_EVENT_CHUNK;
    evt.len = len;
    memcpy(evt.data, data, len);

    if (ota_queue)
    {
        // FreeRTOS copies the data out of &evt into the heap queue pool safely
        if (xQueueSend(ota_queue, &evt, portMAX_DELAY) != pdPASS)
        {
            ESP_LOGE(TAG, "Failed to enqueue OTA chunk (len=%" PRIu32 ")", len);
        }
    }
}