#include "updateSpeed.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "digitalPins.h"
#include "GaugePacket.h"

void update_left_turn(bool state)
{
    lv_obj_set_style_img_opa(
        objects.left,
        state ? LV_OPA_COVER : 50,
        LV_PART_MAIN);
}

void update_right_turn(bool state)
{
    lv_obj_set_style_img_opa(
        objects.right,
        state ? LV_OPA_COVER : 50,
        LV_PART_MAIN);
}

void update_high_beam(bool state)
{
    lv_obj_set_style_img_opa(
        objects.high_beam,
        state ? LV_OPA_COVER : 50,
        LV_PART_MAIN);
}

// Global mileage counter in 1/10 mile units
static int mileage_tenths = 0;
static int odo_divider = 0;

void incrementOdometer(void)
{
    // Timer is firing 10× too fast, so divide by 10
    odo_divider++;
    if (odo_divider < 100)
    {
        return; // skip this tick
    }
    odo_divider = 0; // reset every 10 ticks

    // Now increment exactly 1/10 mile per second
    mileage_tenths++;

    int miles = mileage_tenths / 10;
    int tenth = mileage_tenths % 10;

    static char odoStr[16];
    snprintf(odoStr, sizeof(odoStr), "%06d.%d", miles, tenth);

    lv_label_set_text(objects.odometer, odoStr);
}

void updateIndicators(const GaugePacket &pkt)
{
    uint16_t pins = pkt.digitalPins;

    update_left_turn(pins & IND_LEFT);
    update_right_turn(pins & IND_RIGHT);
    update_high_beam(pins & IND_HIGHBEAM);
}

// Callback for the LVGL animation engine
void speed_anim_cb(void *var, int32_t val)
{
    // Uses the 'speed' object from screens.h
    lv_meter_set_indicator_value(objects.speed, (lv_meter_indicator_t *)var, val);
    // ESP_LOGI("tag", "%d", val);
}

void tach_anim_cb(void *var, int32_t val)
{

    // Update the indicator
    lv_meter_set_indicator_value(objects.tach, (lv_meter_indicator_t *)var, val);
}

void update_speed_ui(int32_t current_speed, int32_t target_speed)
{
    int32_t current_scaled = current_speed * 5;
    int32_t target_scaled = target_speed * 5;
    // Static pointer ensures we only find the needle once
    // lv_meter_indicator_t* speed_needle;

    // Fetch the first indicator in the list (the needle)
    // speed_needle = (lv_meter_indicator_t *)_lv_ll_get_head(objects.speed->indicator_ll);

    // 2. Safety check: ensure screen is loaded and needle exists
    // if (!speed_needle) return;

    // 3. Setup and start the animation
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, speed_anim_cb);
    lv_anim_set_var(&a, screen_main_state.speed_indicator);
    // lv_anim_set_path_cb(&a, lv_anim_path_ease_out); // Non-linear movement
    // lv_anim_set_path_cb(&a, lv_anim_path_overshoot);  // bounces needle at end

    // Animate from current needle position to target
    lv_anim_set_values(&a, current_scaled, target_scaled);

    lv_anim_set_time(&a, 250); // 300ms for a responsive feel

    // lv_anim_set_time(&a, 500);
    // lv_anim_set_repeat_delay(&a, 250);
    // lv_anim_set_playback_time(&a, 500);
    // lv_anim_set_playback_delay(&a, 50);
    // lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);

    lv_anim_start(&a);
}

void update_tach_ui(int32_t current_rpm, int32_t target_rpm)
{
    // Scale for gauge RMP/10
    int32_t current_scaled = current_rpm / 20;
    int32_t target_scaled = target_rpm / 20;
    ESP_LOGI("TAG", "CurrentRPM %d", current_scaled);
    ESP_LOGI("TAG", "TargetRPM %d", target_scaled);
    // Static pointer ensures we only find the needle once
    // lv_meter_indicator_t* speed_needle;

    // Fetch the first indicator in the list (the needle)
    // speed_needle = (lv_meter_indicator_t *)_lv_ll_get_head(objects.speed->indicator_ll);

    // 2. Safety check: ensure screen is loaded and needle exists
    // if (!speed_needle) return;

    // 3. Setup and start the animation
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, tach_anim_cb);
    lv_anim_set_var(&a, screen_main_state.tach_indicator);
    // lv_anim_set_path_cb(&a, lv_anim_path_ease_out); // Non-linear movement
    // lv_anim_set_path_cb(&a, lv_anim_path_overshoot);  // bounces needle at end

    // Animate from current needle position to target
    lv_anim_set_values(&a, current_scaled, target_scaled);

    lv_anim_set_time(&a, 250); // 300ms for a responsive feel

    // lv_anim_set_time(&a, 500);
    // lv_anim_set_repeat_delay(&a, 250);
    // lv_anim_set_playback_time(&a, 500);
    // lv_anim_set_playback_delay(&a, 50);
    // lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);

    lv_anim_start(&a);
}