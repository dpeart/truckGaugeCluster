#include "updateUI.h"
#include "vcan_protocol.h"
#include "vcan_receiver.h"
#include "digitalPins.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "ui.h"
#include <atomic>
#include <cstring>
#include <cmath>

static const char *TAG = "updateUI";

// --- Thread-Safe Atomic Caches (Handoff from VCAN Task -> LVGL Task) ---
static std::atomic<int32_t> target_speed_val(0);
static std::atomic<int32_t> target_rpm_val(0);
static std::atomic<uint16_t> target_digital_pins(0x0000);
static std::atomic<uint32_t> target_odometer_tenths(0);

// Smoothing state variables (managed exclusively within the LVGL thread context)
static float current_smooth_speed = 0.0f;
static float current_smooth_rpm = 0.0f;
#define LERP_FACTOR 0.15f

// Track previous states locally to avoid redundant LVGL calls
// Initialized to 0x0000 so initial sync forces processing
static uint16_t last_processed_digital_pins = 0x0000;
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
    auto &receiver = vcan::Receiver::instance();

    // Register callbacks using vcan::Receiver singleton
    receiver.registerCallback(PGN_ENGINE_CORE, [](uint16_t pgn, const uint8_t *data, uint8_t len)
                              { cb_engine_core(pgn, 0, data, len, nullptr); });

    receiver.registerCallback(PGN_EXHAUST_DIG, [](uint16_t pgn, const uint8_t *data, uint8_t len)
                              { cb_exhaust_dig(pgn, 0, data, len, nullptr); });

    receiver.registerCallback(PGN_CRUISE_ODO, [](uint16_t pgn, const uint8_t *data, uint8_t len)
                              { cb_cruise_odo(pgn, 0, data, len, nullptr); });

    ESP_LOGI(TAG, "Main cluster UI and C++ vcan::Receiver callbacks initialized");
}

void main_cluster_deinit(void)
{
    // Clears subscriptions and resets internal tracked PGN caches
    vcan::Receiver::instance().reset();
    ESP_LOGI(TAG, "Main cluster UI deinitialized and vcan::Receiver reset");
}

// --- VCAN PGN Callbacks (Producer: Runs in VCAN/UART Task context) ---

static void cb_engine_core(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx)
{
    if (len < sizeof(pgn_engine_core_t))
    {
        ESP_LOGE(TAG, "[PGN_ENGINE_CORE] Rejected packet: len (%u) < %u", len, (unsigned)sizeof(pgn_engine_core_t));
        return;
    }

    const auto *msg = reinterpret_cast<const pgn_engine_core_t *>(payload);

    int32_t real_rpm = static_cast<int32_t>(msg->rpm);
    int32_t real_speed = static_cast<int32_t>(msg->speed);

    ESP_LOGD(TAG, "[PGN_ENGINE_CORE] RPM: %ld | Speed: %ld", (long)real_rpm, (long)real_speed);

    target_rpm_val.store(real_rpm, std::memory_order_relaxed);
    target_speed_val.store(real_speed, std::memory_order_relaxed);
}

static void cb_exhaust_dig(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx)
{
    if (len < sizeof(pgn_exhaust_dig_t))
    {
        ESP_LOGE(TAG, "[PGN_EXHAUST_DIG] Rejected packet: len (%u) < %u", len, (unsigned)sizeof(pgn_exhaust_dig_t));
        return;
    }

    const auto *msg = reinterpret_cast<const pgn_exhaust_dig_t *>(payload);

    // Extract digital_pins bitmask
    uint16_t pins = msg->digital_pins;

    ESP_LOGD(TAG, "[PGN_EXHAUST_DIG] Struct Digital Pins: 0x%04X", pins);
    target_digital_pins.store(pins, std::memory_order_relaxed);
}

static void cb_cruise_odo(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx)
{
    if (len < sizeof(pgn_cruise_odo_t))
    {
        ESP_LOGE(TAG, "[PGN_CRUISE_ODO] Rejected packet: len (%u) < %u", len, (unsigned)sizeof(pgn_cruise_odo_t));
        return;
    }

    const auto *msg = reinterpret_cast<const pgn_cruise_odo_t *>(payload);

    ESP_LOGD(TAG, "[PGN_CRUISE_ODO] Odometer Tenths: %lu", (unsigned long)msg->odometer_tenths);
    target_odometer_tenths.store(msg->odometer_tenths, std::memory_order_relaxed);
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

    last_processed_odometer = mileage_tenths;

    int miles = mileage_tenths / 10;
    int tenth = mileage_tenths % 10;

    static char odoStr[16];
    snprintf(odoStr, sizeof(odoStr), "%06d.%d", miles, tenth);

    if (objects.odometer != NULL)
    {
        lv_label_set_text(objects.odometer, odoStr);
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
    static char speed_str[8];
    snprintf(speed_str, sizeof(speed_str), "%ld", (long)target_speed);

    static char last_speed_str[8] = "";
    if (strcmp(speed_str, last_speed_str) != 0)
    {
        if (objects.digital_speed != NULL)
        {
            lv_label_set_text(objects.digital_speed, speed_str);
            strncpy(last_speed_str, speed_str, sizeof(last_speed_str));
        }
    }
}

static void update_tach_ui(int32_t target_rpm)
{
    int32_t target_scaled = target_rpm / 20;
    float alpha = 0.15f;
    float previous_smooth = current_smooth_rpm;

    current_smooth_rpm = alpha * target_scaled + (1.0f - alpha) * current_smooth_rpm;
    int32_t display_val = (int32_t)(current_smooth_rpm + 0.5f);

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