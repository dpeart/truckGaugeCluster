#include "updateUI.h"
#include "vcan_protocol.h"
#include "vcan_receiver.h"
#include "digitalPins.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "ui.h"
#include <atomic>
#include <math.h>

static const char *TAG = "updateUI";

// --- Thread-Safe Atomic Caches (Handoff from VCAN Task -> LVGL Task) ---
static std::atomic<int32_t> target_speed_val(0);
static std::atomic<int32_t> target_rpm_val(0);
static std::atomic<uint16_t> target_digital_pins(0xFFFF);
static std::atomic<uint32_t> target_odometer_tenths(0);

// Smoothing state variables (managed exclusively within the LVGL thread context)
static float current_smooth_speed = 0.0f;
static float current_smooth_rpm = 0.0f;
#define LERP_FACTOR 0.15f

// Track previous states locally to avoid redundant LVGL calls
static uint16_t last_processed_digital_pins = 0xFFFF;
static uint32_t last_processed_odometer = 0xFFFFFFFF;

// --- Forward Declarations for VCAN PGN Callbacks ---
static void cb_engine_core(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx);
static void cb_exhaust_dig(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx);
static void cb_cruise_odo(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx);

float get_current_speed_mph(void)
{
    return static_cast<float>(target_speed_val.load(std::memory_order_relaxed));
}

// --- Initialization / Deinitialization ---
void main_cluster_init(void)
{
    // Register subscriptions for the PGNs powering the P4 main dashboard
    vcan_receiver_register_pgn(PGN_ENGINE_CORE, cb_engine_core, NULL);
    vcan_receiver_register_pgn(PGN_EXHAUST_DIG, cb_exhaust_dig, NULL);
    vcan_receiver_register_pgn(PGN_CRUISE_ODO, cb_cruise_odo, NULL);

    ESP_LOGI(TAG, "Main cluster UI and thread-safe VCAN subscriptions initialized");
}

void main_cluster_deinit(void)
{
    vcan_receiver_unregister_pgn(PGN_ENGINE_CORE, cb_engine_core, NULL);
    vcan_receiver_unregister_pgn(PGN_EXHAUST_DIG, cb_exhaust_dig, NULL);
    vcan_receiver_unregister_pgn(PGN_CRUISE_ODO, cb_cruise_odo, NULL);
}

// --- VCAN PGN Callbacks (Producer: Runs in VCAN/UART Task context) ---

static void cb_engine_core(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx)
{
    if (len < 8)
    {
        ESP_LOGE(TAG, "[PGN_ENGINE_CORE] Rejected packet: len (%u) < 8", len);
        return;
    }

    int32_t raw_rpm = vcan_read_u16_le(&payload[ENGINE_RPM_OFFSET]);
    int32_t raw_speed = vcan_read_s16_le(&payload[ENGINE_SPEED_OFFSET]);

    ESP_LOGI(TAG, "[PGN_ENGINE_CORE] Raw Payload RPM bytes: 0x%02X 0x%02X -> Decoded RPM: %ld | Speed bytes: 0x%02X 0x%02X -> Decoded Speed: %ld",
             payload[ENGINE_RPM_OFFSET], payload[ENGINE_RPM_OFFSET + 1], (long)raw_rpm,
             payload[ENGINE_SPEED_OFFSET], payload[ENGINE_SPEED_OFFSET + 1], (long)raw_speed);

    // 2. Scale back to engineering RPM (0.125 RPM/bit -> divide by 8 or multiply by 0.125)
    int32_t real_rpm = (int32_t)(raw_rpm * 0.125f);

    // 3. Store target RPM as atomic int32_t
    target_rpm_val.store(real_rpm, std::memory_order_relaxed);
    target_speed_val.store(raw_speed, std::memory_order_relaxed);
}

static void cb_exhaust_dig(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx)
{
    if (len < (EXHAUST_DIGITAL_OFFSET + 2))
    {
        ESP_LOGE(TAG, "[PGN_EXHAUST_DIG] Rejected packet: len (%u) < %d", len, EXHAUST_DIGITAL_OFFSET + 2);
        return;
    }

    uint16_t pins = vcan_read_u16_le(&payload[EXHAUST_DIGITAL_OFFSET]);
    ESP_LOGD(TAG, "[PGN_EXHAUST_DIG] Raw digital pins: 0x%04X", pins);
    target_digital_pins.store(pins, std::memory_order_relaxed);
}

static void cb_cruise_odo(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx)
{
    ESP_LOGI(TAG, "[PGN_CRUISE_ODO] Callback triggered! len = %u (required: %d)", len, ODOMETER_TENTHS_OFFSET + 4);

    if (len < (ODOMETER_TENTHS_OFFSET + 4))
    {
        ESP_LOGE(TAG, "[PGN_CRUISE_ODO] Payload too short! len=%u < required %d", len, ODOMETER_TENTHS_OFFSET + 4);
        return;
    }

    // Log raw hex bytes for verification
    ESP_LOGI(TAG, "[PGN_CRUISE_ODO] Odo Raw Bytes: [0x%02X 0x%02X 0x%02X 0x%02X]",
             payload[ODOMETER_TENTHS_OFFSET],
             payload[ODOMETER_TENTHS_OFFSET + 1],
             payload[ODOMETER_TENTHS_OFFSET + 2],
             payload[ODOMETER_TENTHS_OFFSET + 3]);

    uint32_t odo = vcan_read_u32_le(&payload[ODOMETER_TENTHS_OFFSET]);
    ESP_LOGI(TAG, "[PGN_CRUISE_ODO] Parsed uint32_t Odometer Tenths: %lu", (unsigned long)odo);

    target_odometer_tenths.store(odo, std::memory_order_relaxed);
}

// --- UI Control Helpers (Consumer: Safe to call LVGL here) ---

static void update_left_turn(bool state)
{
    if (objects.left != NULL)
    {
        lv_obj_set_style_img_opa(objects.left, state ? LV_OPA_COVER : 50, LV_PART_MAIN);
    }
}

static void update_right_turn(bool state)
{
    if (objects.right != NULL)
    {
        lv_obj_set_style_img_opa(objects.right, state ? LV_OPA_COVER : 50, LV_PART_MAIN);
    }
}

static void update_high_beam(bool state)
{
    if (objects.high_beam != NULL)
    {
        lv_obj_set_style_img_opa(objects.high_beam, state ? LV_OPA_COVER : 50, LV_PART_MAIN);
    }
}

static void process_odometer(uint32_t mileage_tenths)
{
    if (mileage_tenths == last_processed_odometer)
        return;

    ESP_LOGI(TAG, "[process_odometer] New value received: %lu tenths (previous: %lu)",
             (unsigned long)mileage_tenths, (unsigned long)last_processed_odometer);

    last_processed_odometer = mileage_tenths;

    int miles = mileage_tenths / 10;
    int tenth = mileage_tenths % 10;

    static char odoStr[16];
    snprintf(odoStr, sizeof(odoStr), "%06d.%d", miles, tenth);

    if (objects.odometer != NULL)
    {
        ESP_LOGI(TAG, "[process_odometer] Updating LVGL label 'objects.odometer' with text: '%s'", odoStr);
        lv_label_set_text(objects.odometer, odoStr);
    }
    else
    {
        ESP_LOGE(TAG, "[process_odometer] CRITICAL: objects.odometer is NULL! Text '%s' cannot be displayed.", odoStr);
    }
}

static void process_indicators(uint16_t current_pins)
{
    if (current_pins == last_processed_digital_pins)
        return;

    if ((current_pins & IND_LEFT) != (last_processed_digital_pins & IND_LEFT))
    {
        update_left_turn(current_pins & IND_LEFT);
    }
    if ((current_pins & IND_RIGHT) != (last_processed_digital_pins & IND_RIGHT))
    {
        update_right_turn(current_pins & IND_RIGHT);
    }
    if ((current_pins & IND_HIGHBEAM) != (last_processed_digital_pins & IND_HIGHBEAM))
    {
        update_high_beam(current_pins & IND_HIGHBEAM);
    }

    last_processed_digital_pins = current_pins;
}

static void update_speed_ui(int32_t target_speed)
{
    int32_t target_scaled = target_speed * 10;
    float previous_smooth = current_smooth_speed;

    current_smooth_speed += (target_scaled - current_smooth_speed) * LERP_FACTOR;
    int32_t display_val = (int32_t)(current_smooth_speed + 0.5f);

    if ((int32_t)(previous_smooth + 0.5f) != display_val && screen_main_state.speed_indicator != NULL)
    {
        lv_meter_set_indicator_value(objects.speed,
                                     (lv_meter_indicator_t *)screen_main_state.speed_indicator,
                                     display_val);
    }
}

static void update_speed_digital_ui(int32_t target_speed)
{
    // Format speed as 3-digit zero-padded string (e.g. 065 or 000)
    static char speed_str[8];
    snprintf(speed_str, sizeof(speed_str), "%ld", (long)target_speed);

    // Only trigger an LVGL text update if the string value has actually changed
    static char last_speed_str[8] = "";
    if (strcmp(speed_str, last_speed_str) != 0)
    {
        if (objects.digital_speed != NULL)
        {
            lv_label_set_text(objects.digital_speed, speed_str);
            strncpy(last_speed_str, speed_str, sizeof(last_speed_str));
        }
        else
        {
            ESP_LOGE(TAG, "[update_speed_digital_ui] objects.digital_speed is NULL!");
        }
    }
}

static void update_tach_ui(int32_t target_rpm)
{
    // LOG SCALING TO DIAGNOSE SUSPECT RPM RANGES
    int32_t target_scaled = target_rpm / 20;
    float alpha = 0.15f;
    float previous_smooth = current_smooth_rpm;

    current_smooth_rpm = alpha * target_scaled + (1.0f - alpha) * current_smooth_rpm;
    int32_t display_val = (int32_t)(current_smooth_rpm + 0.5f);

    static uint32_t rpm_log_throttle = 0;
    if (esp_log_timestamp() - rpm_log_throttle > 1000)
    {
        rpm_log_throttle = esp_log_timestamp();
        ESP_LOGI(TAG, "[RPM DIG] Raw target_rpm: %ld -> target_scaled (/20): %ld -> smoothed display_val: %ld",
                 (long)target_rpm, (long)target_scaled, (long)display_val);
    }

    if ((int32_t)(previous_smooth + 0.5f) != display_val && screen_main_state.tach_indicator != NULL)
    {
        lv_meter_set_indicator_value(objects.tach,
                                     (lv_meter_indicator_t *)screen_main_state.tach_indicator,
                                     display_val);
    }
}

// --- Master UI Render Hook (Called every 20ms inside LVGL Thread) ---
void gauge_ui_update(bool is_stale)
{
    (void)is_stale;

    // 1. Safely load atomic targets from background VCAN thread
    int32_t active_speed = target_speed_val.load(std::memory_order_relaxed);
    int32_t active_rpm = target_rpm_val.load(std::memory_order_relaxed);
    uint16_t active_pins = target_digital_pins.load(std::memory_order_relaxed);
    uint32_t active_odo = target_odometer_tenths.load(std::memory_order_relaxed);

    // 2. Process smooth needle animations on every frame tick
    update_speed_ui(active_speed);
    update_speed_digital_ui(active_speed);
    update_tach_ui(active_rpm);

    // 3. Process discrete UI state transitions safely under LVGL context
    process_indicators(active_pins);
    process_odometer(active_odo);
}