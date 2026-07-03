#include "updateSpeed.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "digitalPins.h"
#include "GaugePacket.h"

static const char *TAG = "updateSpeed";

// Static variables preserve the actual smooth floating-point positions across frames
static float current_smooth_speed = 0.0f;
static float current_smooth_rpm = 0.0f;

// Tweaking this factor changes needle responsiveness:
// Higher value (e.g., 0.25) = snappier, less smooth
// Lower value (e.g., 0.08)  = heavier weight, silky smooth sweep
#define LERP_FACTOR 0.15f 

void update_left_turn(bool state)
{
    lv_obj_set_style_img_opa(objects.left, state ? LV_OPA_COVER : 50, LV_PART_MAIN);
}

void update_right_turn(bool state)
{
    lv_obj_set_style_img_opa(objects.right, state ? LV_OPA_COVER : 50, LV_PART_MAIN);
}

void update_high_beam(bool state)
{
    lv_obj_set_style_img_opa(objects.high_beam, state ? LV_OPA_COVER : 50, LV_PART_MAIN);
}

// Global mileage counter in 1/10 mile units
static int mileage_tenths = 0;
static int odo_divider = 0;
static int last_printed_mileage = -1; 

void incrementOdometer(void)
{
    odo_divider++;
    if (odo_divider < 100)
    {
        return; 
    }
    odo_divider = 0; 

    mileage_tenths++;

    if (mileage_tenths != last_printed_mileage)
    {
        last_printed_mileage = mileage_tenths;
        
        int miles = mileage_tenths / 10;
        int tenth = mileage_tenths % 10;

        static char odoStr[16];
        snprintf(odoStr, sizeof(odoStr), "%06d.%d", miles, tenth);

        lv_label_set_text(objects.odometer, odoStr);
    }
}

static uint16_t last_digital_pins = 0xFFFF; 

void updateIndicators(const GaugePacket &pkt)
{
    uint16_t current_pins = pkt.digitalPins;

    if (current_pins == last_digital_pins)
    {
        return;
    }

    if ((current_pins & IND_LEFT) != (last_digital_pins & IND_LEFT))
    {
        update_left_turn(current_pins & IND_LEFT);
    }

    if ((current_pins & IND_RIGHT) != (last_digital_pins & IND_RIGHT))
    {
        update_right_turn(current_pins & IND_RIGHT);
    }

    if ((current_pins & IND_HIGHBEAM) != (last_digital_pins & IND_HIGHBEAM))
    {
        update_high_beam(current_pins & IND_HIGHBEAM);
    }

    last_digital_pins = current_pins;
}

void update_speed_ui(int32_t target_speed)
{
    // PIXEL TRICK: Scale up by 10. If data is 45 MPH, target becomes 450.
    // This unlocks 10 micro-steps between every single MPH digit!
    int32_t target_scaled = target_speed * 10; 
    float previous_smooth = current_smooth_speed;

    // The filter now glides seamlessly through individual pixel steps (e.g., 441, 442, 443...)
    current_smooth_speed += (target_scaled - current_smooth_speed) * LERP_FACTOR;

    // Round to the nearest integer for exact sub-pixel layout alignment
    int32_t display_val = (int32_t)(current_smooth_speed + 0.5f);

    // LOGGING: Triggers on actual micro-pixel adjustments
    if ((int32_t)(previous_smooth + 0.5f) != display_val)
    {
        // To read the log as normal MPH, just look at the float divided by 10
        ESP_LOGI(TAG, "[SPEED] Target: %.1f MPH | Smooth Sweep: %.2f MPH -> Dispatched to LVGL: %ld", 
                 (float)target_scaled / 10.0f, current_smooth_speed / 10.0f, display_val);
    }

    if (screen_main_state.speed_indicator != NULL)
    {
        // Sends the high-precision value (0-1400) to match your scale perfectly
        lv_meter_set_indicator_value(objects.speed, 
                                     (lv_meter_indicator_t *)screen_main_state.speed_indicator, 
                                     display_val);
    }
}

void update_tach_ui(int32_t target_rpm)
{
    int32_t target_scaled = target_rpm / 20;
    float previous_smooth = current_smooth_rpm;

    // Glides the tach position closer to the target
    current_smooth_rpm += (target_scaled - current_smooth_rpm) * LERP_FACTOR;

    int32_t display_val = (int32_t)(current_smooth_rpm + 0.5f);

    // LOGGING: Only prints when the RPM needle is actively moving
    if ((int32_t)(previous_smooth + 0.5f) != display_val)
    {
        ESP_LOGI(TAG, "[TACH] Target: %ld | Smooth Calculated: %.2f -> Dispatched Int: %ld", 
                 target_scaled, current_smooth_rpm, display_val);
    }

    if (screen_main_state.tach_indicator != NULL)
    {
        lv_meter_set_indicator_value(objects.tach, 
                                     (lv_meter_indicator_t *)screen_main_state.tach_indicator, 
                                     display_val);
    }
}