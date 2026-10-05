#include "updateUI.h"
#include "vcan_protocol.h"
#include "vcan_receiver.h"
#include "digitalPins.h"
#include "esp_log.h"
#include "lvgl.h"
#include "ui.h"
#include <cstddef>
#include <cstring>
#include <cmath>

static const char *TAG = "updateUI";

// Cached latest speed for external getter
static int32_t s_cached_speed_mph = 0;

// Smoothing & cached state variables
static float current_smooth_speed  = 0.0f;
static float previous_smooth_speed = 0.0f;

static float current_smooth_rpm    = 0.0f;
static float previous_smooth_rpm   = 0.0f;

#define LERP_FACTOR 0.15f

// Intermediate values computed in update step, consumed in draw step
static int32_t  s_target_speed    = 0;
static int32_t  s_target_rpm      = 0;
static uint16_t s_digital_pins    = 0;
static uint32_t s_odometer_tenths = 0;

// Track previous states locally to avoid redundant LVGL calls
static uint16_t last_processed_digital_pins = 0x0000;
static uint32_t last_processed_odometer     = 0xFFFFFFFF;

float get_current_speed_mph(void)
{
    return static_cast<float>(s_cached_speed_mph);
}

void main_cluster_init(void)
{
    ESP_LOGI(TAG, "Main cluster UI initialized");
}

void main_cluster_deinit(void)
{
    ESP_LOGI(TAG, "Main cluster UI deinitialized");
}

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

static void update_speed_ui(int32_t display_val, int32_t prev_display_val)
{
    if (prev_display_val != display_val && screen_main_state.speed_indicator != NULL)
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

static void update_tach_ui(int32_t display_val, int32_t prev_display_val)
{
    if (prev_display_val != display_val && screen_main_state.tach_indicator != NULL)
    {
        lv_meter_set_indicator_value(objects.tach,
                                     (lv_meter_indicator_t *)screen_main_state.tach_indicator,
                                     display_val);
    }
}

// -----------------------------------------------------------------------------
// Step 1: Poll VCAN receiver & compute smoothed values (OUTSIDE lvgl_lock)
// -----------------------------------------------------------------------------
void main_cluster_update(void)
{
    auto& receiver = vcan::Receiver::instance();
    uint8_t buf[256];
    uint8_t len = 0;
    uint32_t last_seen_ms = 0;

    // 1. Engine Core (RPM & Speed)
    if (receiver.getLastPayload(PGN_ENGINE_CORE, buf, &len, &last_seen_ms) == ESP_OK)
    {
        constexpr size_t min_len = offsetof(pgn_engine_core_t, speed) + sizeof(pgn_engine_core_t::speed);
        if (len >= min_len)
        {
            const auto *msg = reinterpret_cast<const pgn_engine_core_t *>(buf);
            s_target_rpm = static_cast<int32_t>(msg->rpm);
            s_target_speed = static_cast<int32_t>(msg->speed);
            s_cached_speed_mph = s_target_speed;
        }
    }

    // 2. Indicators (Digital Pins)
    if (receiver.getLastPayload(PGN_EXHAUST_DIG, buf, &len, &last_seen_ms) == ESP_OK)
    {
        constexpr size_t min_len = offsetof(pgn_exhaust_dig_t, digital_pins) + sizeof(pgn_exhaust_dig_t::digital_pins);
        if (len >= min_len)
        {
            const auto *msg = reinterpret_cast<const pgn_exhaust_dig_t *>(buf);
            s_digital_pins = msg->digital_pins;
        }
    }

    // 3. Odometer
    if (receiver.getLastPayload(PGN_CRUISE_ODO, buf, &len, &last_seen_ms) == ESP_OK)
    {
        constexpr size_t min_len = offsetof(pgn_cruise_odo_t, odometer_tenths) + sizeof(pgn_cruise_odo_t::odometer_tenths);
        if (len >= min_len)
        {
            const auto *msg = reinterpret_cast<const pgn_cruise_odo_t *>(buf);
            s_odometer_tenths = msg->odometer_tenths;
        }
    }

    // 4. Compute speed LERP while preserving previous smoothed value
    previous_smooth_speed = current_smooth_speed;
    int32_t speed_scaled = s_target_speed * 10;
    current_smooth_speed += (speed_scaled - current_smooth_speed) * LERP_FACTOR;

    // 5. Compute tach LERP while preserving previous smoothed value
    previous_smooth_rpm = current_smooth_rpm;
    int32_t tach_scaled = s_target_rpm / 20;
    float alpha = 0.15f;
    current_smooth_rpm = alpha * tach_scaled + (1.0f - alpha) * current_smooth_rpm;
}

// -----------------------------------------------------------------------------
// Step 2: Apply updated values to LVGL widgets (INSIDE lvgl_lock)
// -----------------------------------------------------------------------------
void gauge_ui_update(bool is_stale)
{
    (void)is_stale;

    int32_t speed_display_val      = (int32_t)(current_smooth_speed + 0.5f);
    int32_t prev_speed_display_val = (int32_t)(previous_smooth_speed + 0.5f);

    int32_t tach_display_val       = (int32_t)(current_smooth_rpm + 0.5f);
    int32_t prev_tach_display_val  = (int32_t)(previous_smooth_rpm + 0.5f);

    process_indicators(s_digital_pins);
    process_odometer(s_odometer_tenths);

    update_speed_ui(speed_display_val, prev_speed_display_val);
    update_speed_digital_ui(s_target_speed);
    update_tach_ui(tach_display_val, prev_tach_display_val);
}