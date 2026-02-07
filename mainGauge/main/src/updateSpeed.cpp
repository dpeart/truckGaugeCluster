#include "updateSpeed.h"
#include "esp_log.h"
#include "esp_timer.h"

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
    if (odo_divider < 10) {
        return;   // skip this tick
    }
    odo_divider = 0;  // reset every 10 ticks

    // Now increment exactly 1/10 mile per second
    mileage_tenths++;

    int miles = mileage_tenths / 10;
    int tenth = mileage_tenths % 10;

    static char odoStr[16];
    snprintf(odoStr, sizeof(odoStr), "%06d.%d", miles, tenth);

    lv_label_set_text(objects.odometer, odoStr);
}

void generateTurnSignalPattern()
{
    static int64_t lastChange = 0;
    static uint8_t state = 0;

    int64_t now = esp_timer_get_time(); // microseconds

    if (now - lastChange < 1000000)
        return; // 1 second
    lastChange = now;

    switch (state)
    {
    case 0: // Left ON
        update_left_turn(true);
        update_right_turn(false);
        break;

    case 1: // Left OFF
        update_left_turn(false);
        update_right_turn(false);
        break;

    case 2: // Right ON
        update_left_turn(false);
        update_right_turn(true);
        break;

    case 3: // Right OFF
        update_left_turn(false);
        update_right_turn(false);
        break;

    case 4: // Both ON
        update_left_turn(true);
        update_right_turn(true);
        update_high_beam(true);
        break;

    case 5: // Both OFF
        update_left_turn(false);
        update_right_turn(false);
        update_high_beam(false);
        break;
    }

    state = (state + 1) % 6;
}

// Callback for the LVGL animation engine
static void meter_anim_cb(void *var, int32_t val)
{
    // Uses the 'speed' object from screens.h
    lv_meter_set_indicator_value(objects.main, (lv_meter_indicator_t *)var, val);
    // ESP_LOGI("tag", "%d", val);
}

void update_speed_ui(int32_t current_speed, int32_t target_speed)
{
    // Static pointer ensures we only find the needle once
    // lv_meter_indicator_t* speed_needle;

    // Fetch the first indicator in the list (the needle)
    // speed_needle = (lv_meter_indicator_t *)_lv_ll_get_head(objects.speed->indicator_ll);

    // 2. Safety check: ensure screen is loaded and needle exists
    // if (!speed_needle) return;

    // 3. Setup and start the animation
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, meter_anim_cb);
    lv_anim_set_var(&a, screen_main_state.speed_indicator);

    // Animate from current needle position to target
    lv_anim_set_values(&a, current_speed, target_speed);

    lv_anim_set_time(&a, 2300); // 300ms for a responsive feel

    // lv_anim_set_time(&a, 500);
    lv_anim_set_repeat_delay(&a, 250);
    lv_anim_set_playback_time(&a, 500);
    lv_anim_set_playback_delay(&a, 50);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);

    lv_anim_start(&a);
}

void update_tach_ui(int32_t current_rpm, int32_t target_rpm)
{
    // Static pointer ensures we only find the needle once
    // lv_meter_indicator_t* speed_needle;

    // Fetch the first indicator in the list (the needle)
    // speed_needle = (lv_meter_indicator_t *)_lv_ll_get_head(objects.speed->indicator_ll);

    // 2. Safety check: ensure screen is loaded and needle exists
    // if (!speed_needle) return;

    // 3. Setup and start the animation
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, meter_anim_cb);
    lv_anim_set_var(&a, screen_main_state.tach_indicator);

    // Animate from current needle position to target
    lv_anim_set_values(&a, current_rpm, target_rpm);

    lv_anim_set_time(&a, 500); // 300ms for a responsive feel

    // lv_anim_set_time(&a, 500);
    lv_anim_set_repeat_delay(&a, 250);
    lv_anim_set_playback_time(&a, 500);
    lv_anim_set_playback_delay(&a, 50);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);

    lv_anim_start(&a);
}