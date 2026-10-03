#include "updateUI.h"
#include "small_gauge.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "updateUI";

void text_update_cb(lv_obj_t *label, const char *str)
{
    if (!label || !str)
        return;
    lv_label_set_text(label, str);
    ESP_LOGI(TAG, "text_update_cb: label=%p text=\"%s\"", (void *)label, str);
}

static void updateIndicators(const GaugePacket *pkt)
{
    typedef struct
    {
        lv_obj_t *obj;
        bool active;
    } indicator_map_t;

    indicator_map_t map[] = {
        {objects.water_in_fuel, pkt->ind_water_in_fuel},
        {objects.low_washer_fluid, pkt->ind_low_washer},
        {objects.low_fuel, pkt->ind_low_fuel},
        {objects.low_battery, pkt->ind_low_battery},
        {objects.engine_temp, pkt->ind_engine_temp}};

    int total = sizeof(map) / sizeof(map[0]);
    int active_indices[total];
    int active_count = 0;

    for (int i = 0; i < total; i++)
        if (map[i].active)
            active_indices[active_count++] = i;

    ESP_LOGI(TAG, "updateIndicators: raw booleans WIF=%d washer=%d lowFuel=%d lowBatt=%d engTemp=%d",
             pkt->ind_water_in_fuel, pkt->ind_low_washer, pkt->ind_low_fuel,
             pkt->ind_low_battery, pkt->ind_engine_temp);

    static int rotation_idx = 0;
    static int64_t last_switch = 0;
    int64_t now = esp_timer_get_time();

    if (now - last_switch > 5000000)
    {
        rotation_idx++;
        last_switch = now;
        ESP_LOGI(TAG, "updateIndicators: rotation tick -> rotation_idx=%d", rotation_idx);
    }

    if (active_count > 0)
        rotation_idx %= active_count;
    else
        rotation_idx = 0;

    ESP_LOGI(TAG, "updateIndicators: active_count=%d rotation_idx=%d", active_count, rotation_idx);

    /* Hide all indicators */
    for (int i = 0; i < total; i++)
    {
        lv_obj_add_flag(map[i].obj, LV_OBJ_FLAG_HIDDEN);
    }

    if (active_count > 0)
    {
        int target = active_indices[rotation_idx];
        ESP_LOGI(TAG, "updateIndicators: showing indicator index=%d", target);
        lv_obj_clear_flag(map[target].obj, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(objects.info, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(TAG, "updateIndicators: info hidden");
    }
    else
    {
        lv_obj_clear_flag(objects.info, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(TAG, "updateIndicators: no active indicators, info shown");
    }
}

void gauge_ui_update(bool is_stale)
{
    // 1. Process the raw CAN data into the UI-ready packet
    small_gauge_draw();

    const GaugePacket *pkt = small_gauge_get_last_packet();
    if (!pkt)
        return;

    ESP_LOGI(TAG, "gauge_ui_update called: ambient=%d heading=\"%s\" time=%02u:%02u WIF=%d washer=%d lowFuel=%d lowBatt=%d engTemp=%d",
             pkt->ambientTemp, pkt->compass8, pkt->hour, pkt->minute,
             pkt->ind_water_in_fuel, pkt->ind_low_washer, pkt->ind_low_fuel,
             pkt->ind_low_battery, pkt->ind_engine_temp);

    static int16_t last_ambient = -999;
    static char last_heading[16] = {0};
    static char last_time[10] = {0};

    /* Ambient temp */
    if (pkt->ambientTemp != last_ambient)
    {
        char buf[12];
        snprintf(buf, sizeof(buf), "%d", pkt->ambientTemp / INT_SCALING);
        ESP_LOGI(TAG, "Ambient changed: %d -> %s", last_ambient, buf);
        text_update_cb(objects.ambient_temp, buf);
        last_ambient = pkt->ambientTemp;
    }

    /* Heading */
    if (strcmp(pkt->compass8, last_heading) != 0)
    {
        ESP_LOGI(TAG, "Heading changed: \"%s\" -> \"%s\"", last_heading, pkt->compass8);
        text_update_cb(objects.heading, pkt->compass8);
        strncpy(last_heading, pkt->compass8, sizeof(last_heading) - 1);
        last_heading[sizeof(last_heading) - 1] = '\0';
    }

    /* Time */
    char current_time[10];
    int display_hour = pkt->hour % 12;
    if (display_hour == 0)
        display_hour = 12;

    snprintf(current_time, sizeof(current_time), "%d:%02d",
             display_hour, pkt->minute);

    if (strcmp(current_time, last_time) != 0)
    {
        ESP_LOGI(TAG, "Time changed: \"%s\" -> \"%s\"", last_time, current_time);
        text_update_cb(objects.time, current_time);
        strncpy(last_time, current_time, sizeof(last_time) - 1);
        last_time[sizeof(last_time) - 1] = '\0';
    }

    /* Indicators */
    updateIndicators(pkt);

    if (is_stale)
        ESP_LOGI(TAG, "small gauge data is stale");
}
