#pragma once

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

/**
 * @brief Handles putting the ESP32-C6 co-processor into bootloader/flash mode
 * triggered by grounding GPIO 51.
 */
class SlaveBootHandler
{
public:
    // GPIO Definitions based on hardware request
    static constexpr gpio_num_t TRIGGER_GPIO = GPIO_NUM_51;       // Input trigger (grounded)
    static constexpr gpio_num_t SLAVE_EN_GPIO = GPIO_NUM_54;      // C6 CHIP_EN
    static constexpr gpio_num_t SLAVE_IO9_GPIO = GPIO_NUM_50;     // C6 IO9 (Boot Pin)
    static constexpr gpio_num_t C6_OTA_GPIO = GPIO_NUM_30;   // Input trigger for C6 OTA mode (grounded)
    static constexpr gpio_num_t FACTORY_RESET_GPIO = GPIO_NUM_46; // Input trigger for Factory Reset (grounded)
    static constexpr gpio_num_t PROVISIONING_GPIO = GPIO_NUM_47;  // Input trigger for Provisioning (grounded)
    static constexpr gpio_num_t P4_OTA_GPIO = GPIO_NUM_31;  // Input trigger for P4 OTA mode (grounded)

    SlaveBootHandler();
    ~SlaveBootHandler();

    esp_err_t begin(UBaseType_t priority = 5);

private:
    TaskHandle_t _task_handle = nullptr;
    static void task_wrapper(void *pvParameters);
    void run();

    void execute_boot_sequence();
    void execute_normal_reset();
};
