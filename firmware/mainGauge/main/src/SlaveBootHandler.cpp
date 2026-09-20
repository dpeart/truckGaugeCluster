#include "SlaveBootHandler.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "p4_modes.h"

static const char *TAG = "SlaveBoot";

SlaveBootHandler::SlaveBootHandler() {}

SlaveBootHandler::~SlaveBootHandler()
{
    if (_task_handle != nullptr)
    {
        vTaskDelete(_task_handle);
    }
}

esp_err_t SlaveBootHandler::begin(UBaseType_t priority)
{
    // Configure Trigger Pin (GPIO 51)
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << TRIGGER_GPIO);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE; // Internal pull-up for momentary switch to GND
    gpio_config(&io_conf);

    // Configure Output Control Pins (EN and IO9)
    io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << SLAVE_EN_GPIO) | (1ULL << SLAVE_IO9_GPIO);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    // Ensure pins start HIGH (inactive state for C6)
    gpio_set_level(SLAVE_EN_GPIO, 1);
    gpio_set_level(SLAVE_IO9_GPIO, 1);

    // Configure GPIO30 as OTA trigger input
    io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << C6_OTA_GPIO);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    // --- Configure factory reset trigger pin ---
    io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << FACTORY_RESET_GPIO);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    // --- Configure GPIO47 as input for provisioning mode trigger ---
    io_conf = {};
    io_conf.pin_bit_mask = (1ULL << PROVISIONING_GPIO);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

    // Configure OTA Trigger Pin (GPIO31)
    io_conf = {};
    io_conf.pin_bit_mask = (1ULL << P4_OTA_GPIO);
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE; // internal pull-up
    gpio_config(&io_conf);

    BaseType_t xReturned = xTaskCreate(
        SlaveBootHandler::task_wrapper,
        "slave_boot_task",
        3072,
        this,
        priority,
        &_task_handle);

    return (xReturned == pdPASS) ? ESP_OK : ESP_FAIL;
}

void SlaveBootHandler::task_wrapper(void *pvParameters)
{
    static_cast<SlaveBootHandler *>(pvParameters)->run();
}

void SlaveBootHandler::run()
{
    ESP_LOGI(TAG,
             "Monitoring GPIO %d for boot trigger, GPIO30 for OTA trigger, "
             "GPIO46 for factory reset, and GPIO47 for provisioning...",
             TRIGGER_GPIO);

    const int debounce_ms = 50;

    bool last_ota_state = true;   // GPIO30 pulled-up = HIGH
    bool last_reset_state = true; // GPIO46 pulled-up = HIGH
    bool last_prov_state = true;  // GPIO47 pulled-up = HIGH
    bool last_p4ota_state = true; // GPIO31 pulled-up = HIGH

    while (true)
    {
        // ---------------------------------------------------------
        // OTA Trigger (GPIO30 falling edge)
        // ---------------------------------------------------------
        bool ota_state = gpio_get_level(GPIO_NUM_30);

        if (last_ota_state == true && ota_state == false)
        {
            vTaskDelay(pdMS_TO_TICKS(debounce_ms));

            if (gpio_get_level(GPIO_NUM_30) == false)
            {
                ESP_LOGW(TAG, "GPIO30 pulled LOW — requesting C6 OTA mode");
                send_mode_c6_ota();
            }
        }

        last_ota_state = ota_state;

        // ---------------------------------------------------------
        // Factory Reset Trigger (GPIO46 falling edge)
        // ---------------------------------------------------------
        bool reset_state = gpio_get_level(GPIO_NUM_46);

        if (last_reset_state == true && reset_state == false)
        {
            vTaskDelay(pdMS_TO_TICKS(debounce_ms));

            if (gpio_get_level(GPIO_NUM_46) == false)
            {
                ESP_LOGW(TAG, "GPIO46 pulled LOW — requesting C6 FACTORY RESET");
                send_mode_factory_reset();
            }
        }

        last_reset_state = reset_state;

        // ---------------------------------------------------------
        // Provisioning Trigger (GPIO47 falling edge)
        // ---------------------------------------------------------
        bool prov_state = gpio_get_level(GPIO_NUM_47);

        if (last_prov_state == true && prov_state == false)
        {
            vTaskDelay(pdMS_TO_TICKS(debounce_ms));

            if (gpio_get_level(GPIO_NUM_47) == false)
            {
                ESP_LOGW(TAG, "GPIO47 pulled LOW — requesting C6 PROVISIONING mode");
                send_mode_provisioning(); // <-- NEW
            }
        }

        last_prov_state = prov_state;

        // ---------------------------------------------------------
        // Boot Trigger (GPIO51 falling edge)
        // ---------------------------------------------------------
        if (gpio_get_level(TRIGGER_GPIO) == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(debounce_ms));

            if (gpio_get_level(TRIGGER_GPIO) == 0)
            {
                ESP_LOGW(TAG, "Boot trigger detected! Putting C6 into Flash Mode...");
                execute_boot_sequence();

                while (gpio_get_level(TRIGGER_GPIO) == 0)
                    vTaskDelay(pdMS_TO_TICKS(100));

                ESP_LOGI(TAG, "Trigger released. Resetting C6 to Normal Mode...");
                execute_normal_reset();
            }
        }

        // ---------------------------------------------------------
        // P4 Self-OTA Trigger (GPIO31 falling edge)
        // ---------------------------------------------------------
        bool p4ota_state = gpio_get_level(GPIO_NUM_31);

        if (last_p4ota_state == true && p4ota_state == false)
        {
            vTaskDelay(pdMS_TO_TICKS(debounce_ms));

            if (gpio_get_level(GPIO_NUM_31) == false)
            {
                ESP_LOGW(TAG, "GPIO31 pulled LOW — initiating P4 OTA mode");
                send_mode_p4_ota(); // <-- NEW: P4 requests OTA from C6
            }
        }

        last_p4ota_state = p4ota_state;

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void SlaveBootHandler::execute_boot_sequence()
{
    // 1. Drive EN low first as requested
    ESP_LOGI(TAG, "Resetting C6: EN LOW (GPIO %d)", SLAVE_EN_GPIO);
    gpio_set_level(SLAVE_EN_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(10)); // Stability margin

    // 2. Drive IO9 low
    ESP_LOGI(TAG, "Setting Boot Strap: IO9 LOW (GPIO %d)", SLAVE_IO9_GPIO);
    gpio_set_level(SLAVE_IO9_GPIO, 0);

    // 3. Hold for 250ms to ensure reset is captured
    vTaskDelay(pdMS_TO_TICKS(250));

    // 4. Release EN high (IO9 is low, satisfying "LOW before EN rises")
    ESP_LOGI(TAG, "Releasing Reset: EN HIGH");
    gpio_set_level(SLAVE_EN_GPIO, 1);

    // 5. Wait 100ms (Safety margin: must remain LOW for at least ~5ms after EN HIGH)
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "Releasing Strap: IO9 HIGH");
    gpio_set_level(SLAVE_IO9_GPIO, 1);
}

void SlaveBootHandler::execute_normal_reset()
{
    ESP_LOGI(TAG, "Performing hardware normal reset on C6...");

    // EN/IO9 as outputs
    gpio_set_direction(SLAVE_EN_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(SLAVE_IO9_GPIO, GPIO_MODE_OUTPUT);

    // 1. EN low (reset)
    gpio_set_level(SLAVE_EN_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(10));

    // 2. IO9 high (normal boot)
    gpio_set_level(SLAVE_IO9_GPIO, 1);

    // 3. EN high (release reset)
    gpio_set_level(SLAVE_EN_GPIO, 1);
    ESP_LOGI(TAG, "C6 EN HIGH, IO9 HIGH → normal boot requested");

    vTaskDelay(pdMS_TO_TICKS(200));
}
